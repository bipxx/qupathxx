"""Owned-buffer pixel recipes: portable predictions, files, tiles and lifetimes."""
import gc
from concurrent.futures import ThreadPoolExecutor
import numpy as np
import pytest
import qupathxx as q

pytestmark=pytest.mark.skipif(not q.has_classifier_training,reason='optional classifier training backend')

@pytest.fixture
def scene():
    pixels=np.zeros((24,32,4),dtype=np.uint8);pixels[:,:,3]=255
    pixels[:,:16,:3]=[240,20,10];pixels[:,16:,:3]=[10,20,240]
    pixels[5,5,3]=0;pixels[6,5,3]=128
    options=q.PixelFeatureOptions();options.sigmas=[0,1];options.gradient_magnitude=True
    buffer=q.PixelBuffer.from_numpy(pixels);features=q.extract_pixel_features(buffer,options)
    valid=features.valid_numpy().reshape(-1).astype(bool)
    labels=np.tile(np.arange(32)>=16,24).astype(np.uint32)
    parameters=q.ForestParameters();parameters.trees=16;parameters.max_depth=4
    forest=q.train_forest(features.numpy().reshape(-1,options.feature_count)[valid],labels[valid],features.features,['red','blue'],parameters)
    return pixels,buffer,q.PixelClassifier(forest,options),labels.reshape(24,32)

def test_recipe_roundtrip_prediction_files_and_schema(scene,tmp_path):
    pixels,buffer,recipe,expected=scene
    result=q.classify_pixels(buffer,recipe)
    labels=expected+1;labels[pixels[:,:,3]!=255]=0
    np.testing.assert_array_equal(result.labels_numpy(),labels)
    assert result.valid_pixels==result.classified_pixels==766
    assert result.class_counts==[382,384] and result.model_sha256==recipe.sha256
    assert np.isnan(result.vote_fractions_numpy()[5,5])
    assert result.estimated_work_bytes<=recipe.options.work_bytes
    portable=q.PixelClassifier.from_bytes(recipe.to_bytes())
    assert portable.to_bytes()==recipe.to_bytes() and portable.sha256==recipe.sha256
    assert q.classify_pixels(buffer,portable).sha256==result.sha256
    path=str(tmp_path/'model.qpc');q.save_pixel_classifier(recipe,path)
    assert q.load_pixel_classifier(path).sha256==recipe.sha256
    with pytest.raises((ValueError,RuntimeError)):q.save_pixel_classifier(recipe,path)
    q.save_pixel_classifier(recipe,path,replace_existing=True)
    invalid=tmp_path/'image.tif';invalid.write_bytes(b'keep this image')
    with pytest.raises((ValueError,RuntimeError)):q.save_pixel_classifier(recipe,str(invalid),replace_existing=True)
    assert invalid.read_bytes()==b'keep this image'
    for blob in [b'',recipe.to_bytes()[:-1],recipe.to_bytes()[:-1]+b'x']:
        with pytest.raises(ValueError):q.PixelClassifier.from_bytes(blob)
    options=recipe.options;options.sigmas=[0,2]
    with pytest.raises(ValueError,match='schema'):q.PixelClassifier(recipe.forest,options)
    assert recipe.options.sigmas==[0,1]

@pytest.mark.parametrize('region',[(0,0,7,6),(10,8,8,9),(25,19,7,5)])
def test_tile_prediction_matches_whole_image_and_context_identity(scene,region):
    pixels,buffer,recipe,_=scene;x,y,w,h=region;halo=recipe.options.halo
    left=max(0,x-halo);top=max(0,y-halo);right=min(32,x+w+halo);bottom=min(24,y+h+halo)
    full=q.classify_pixels(buffer,recipe)
    direct=q.classify_pixels(buffer,recipe,window=q.PixelFeatureWindow(32,24,x,y,w,h))
    tile=q.PixelBuffer.from_numpy(pixels[top:bottom,left:right].copy())
    tiled=q.classify_pixels(tile,recipe,window=q.PixelFeatureWindow(32,24,x,y,w,h,left,top))
    np.testing.assert_array_equal(tiled.labels_numpy(),full.labels_numpy()[y:y+h,x:x+w])
    np.testing.assert_array_equal(tiled.vote_fractions_numpy(),full.vote_fractions_numpy()[y:y+h,x:x+w])
    assert tiled.sha256==direct.sha256 and tiled.input_sha256==direct.input_sha256

def test_result_ownership_thread_safety_and_rejections(scene):
    _,buffer,recipe,_=scene
    with ThreadPoolExecutor(2) as pool:results=list(pool.map(lambda _:q.classify_pixels(buffer,recipe),range(4)))
    assert len({r.sha256 for r in results})==1
    result=results[0];labels=result.labels_numpy();votes=result.vote_fractions_numpy();valid=result.valid_numpy()
    for a in [labels,votes,valid]:
        assert not a.flags.writeable
        with pytest.raises(ValueError):a.flat[0]=0
    copy=result.labels_numpy(copy=True);copy[:]=0
    assert result.labels_numpy()[0,0]==1
    classes=result.classes;classes[0]='changed';assert result.classes[0]=='red'
    saved=labels.copy();del result,results,buffer,recipe;gc.collect();np.testing.assert_array_equal(labels,saved)
    for threshold in [-1,1.1,float('nan')]:
        with pytest.raises(ValueError):q.classify_pixels(scene[1],scene[2],threshold)
    with pytest.raises(ValueError):q.classify_pixels(scene[1],scene[2],work_bytes=1)
    assert q.classify_pixels(scene[1],scene[2],-0.).sha256==q.classify_pixels(scene[1],scene[2],0.).sha256

def test_all_invalid_is_owned_empty_mask(scene):
    pixels=scene[0].copy();pixels[:,:,3]=0
    result=q.classify_pixels(q.PixelBuffer.from_numpy(pixels),scene[2])
    assert result.valid_pixels==result.classified_pixels==0 and result.class_counts==[0,0]
    assert not result.labels_numpy().any() and np.isnan(result.vote_fractions_numpy()).all()


def test_saved_stain_recipe_preserves_transform_and_predictions(scene):
    pixels,buffer,_,expected=scene
    transform=q.StainTransform([[.65,.70,.29],[.27,.57,.78]],white=[230,240,250])
    options=q.PixelFeatureOptions();options.rgb=False;options.optical_density=options.stains=True;options.sigmas=[0,1]
    features=q.extract_pixel_features(buffer,options,transform);valid=features.valid_numpy().ravel().astype(bool)
    parameters=q.ForestParameters();parameters.trees=8;parameters.max_depth=4
    forest=q.train_forest(features.numpy().reshape(-1,options.feature_count)[valid],expected.ravel()[valid],features.features,['red','blue'],parameters)
    recipe=q.PixelClassifier(forest,options,transform);portable=q.PixelClassifier.from_bytes(recipe.to_bytes())
    assert portable.transform.white==transform.white and portable.transform.stains==transform.stains
    assert portable.to_bytes()==recipe.to_bytes()
    result=q.classify_pixels(buffer,recipe);reopened=q.classify_pixels(buffer,portable)
    assert result.sha256==reopened.sha256
    np.testing.assert_array_equal(result.labels_numpy()[pixels[:,:,3]==255],expected[pixels[:,:,3]==255]+1)
    changed=q.StainTransform([[.65,.70,.29],[.27,.57,.78]],white=[220,240,250])
    with pytest.raises(ValueError,match='schema'):q.PixelClassifier(forest,options,changed)
