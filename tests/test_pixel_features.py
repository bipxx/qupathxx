"""Independent convolution/stencil oracles and exact native context delivery."""
import concurrent.futures
import gc
import numpy as np
import pytest
import qupathxx as q


def rgba(height=13, width=17):
    rng=np.random.default_rng(19)
    p=rng.integers(0,256,(height,width,4),dtype=np.uint8);p[:,:,3]=255
    p[2::5,1::4,3]=0;p[1::6,3::5,3]=128
    return p


def normalized_2d(values, valid, sigma):
    if sigma==0:return np.where(valid,values,0).astype(np.float64)
    radius=int(np.ceil(3*sigma));d=np.arange(-radius,radius+1,dtype=np.float64)
    kernel=np.exp(-.5*(d/sigma)**2);weights=np.outer(kernel,kernel)
    out=np.zeros(values.shape,dtype=np.float64)
    h,w=values.shape
    for y in range(h):
        for x in range(w):
            ys=np.arange(max(0,y-radius),min(h,y+radius+1));xs=np.arange(max(0,x-radius),min(w,x+radius+1))
            weight=weights[np.ix_(ys-y+radius,xs-x+radius)]*valid[np.ix_(ys,xs)]
            if weight.sum():out[y,x]=(values[np.ix_(ys,xs)]*weight).sum()/weight.sum()
    return out


def derivatives(v, valid):
    h,w=v.shape;gradient=np.zeros_like(v);laplacian=np.zeros_like(v)
    for y in range(h):
        for x in range(w):
            c=v[y,x]
            def neighbor(yy,xx):return v[yy,xx] if 0<=yy<h and 0<=xx<w and valid[yy,xx] else c
            l,r,t,b=neighbor(y,x-1),neighbor(y,x+1),neighbor(y-1,x),neighbor(y+1,x)
            gradient[y,x]=np.hypot((r-l)*.5,(b-t)*.5);laplacian[y,x]=l+r+t+b-4*c
    return gradient,laplacian


def test_gaussian_gradient_laplacian_match_independent_2d_oracle():
    pixels=rgba();valid=pixels[:,:,3]==255
    options=q.PixelFeatureOptions();options.sigmas=[0,.7,2.];options.gradient_magnitude=options.laplacian=True
    result=q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),options)
    expected=[]
    for channel in range(3):
        for sigma in options.sigmas:
            smooth=normalized_2d(pixels[:,:,channel],valid,sigma)
            gradient,laplacian=derivatives(smooth,valid)
            expected.extend([smooth,gradient,laplacian])
    expected=np.stack(expected,axis=-1).astype(np.float32);expected[~valid]=np.nan
    np.testing.assert_allclose(result.numpy(),expected,rtol=2e-6,atol=2e-5,equal_nan=True)
    np.testing.assert_array_equal(result.valid_numpy(),valid.astype(np.uint8))
    assert result.valid_pixels==valid.sum() and result.algorithm=='qpx.pixel-features/1'
    assert result.estimated_work_bytes==q.pixel_feature_work_bytes(17,13,options)
    assert len(result.features)==27 and options.halo==7


@pytest.mark.parametrize('bounds', [(0,0,7,5),(34,28,7,7),(12,10,9,8),(1,1,1,1)])
def test_tiled_context_matches_full_frame_bit_for_bit(bounds):
    pixels=rgba(35,41);options=q.PixelFeatureOptions();options.sigmas=[0,1,2];options.gradient_magnitude=options.laplacian=True
    full=q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),options)
    x,y,w,h=bounds;r=options.halo
    left,top=max(0,x-r),max(0,y-r);right,bottom=min(41,x+w+r),min(35,y+h+r)
    window=q.PixelFeatureWindow(41,35,x,y,w,h,left,top)
    tile=q.PixelBuffer.from_numpy(pixels[top:bottom,left:right])
    result=q.extract_pixel_features(tile,options,window=window)
    direct=q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),options,window=q.PixelFeatureWindow(41,35,x,y,w,h))
    np.testing.assert_array_equal(result.numpy(),full.numpy()[y:y+h,x:x+w])
    assert result.sha256==direct.sha256 and result.input_sha256==direct.input_sha256
    assert (result.context_x,result.context_y,result.context_width,result.context_height)==(left,top,right-left,bottom-top)
    assert result.estimated_work_bytes==direct.estimated_work_bytes


def test_stain_and_od_channels_are_transformed_before_smoothing_with_bound_schema():
    pixels=rgba(7,9);valid=pixels[:,:,3]==255
    transform=q.StainTransform([[.65,.70,.29],[.27,.57,.78]],white=[230,240,250])
    od=np.maximum(0,-np.log10(np.maximum(1,pixels[:,:,:3].astype(np.float64))/np.array(transform.white)))
    stains=od@np.linalg.inv(np.array(transform.stains))
    options=q.PixelFeatureOptions();options.rgb=False;options.optical_density=options.stains=True;options.sigmas=[0]
    result=q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),options,transform)
    expected=np.concatenate([od.astype(np.float32),stains.astype(np.float32)],axis=-1);expected[~valid]=np.nan
    np.testing.assert_allclose(result.numpy(),expected,rtol=2e-6,atol=1e-6,equal_nan=True)
    schema=[f.name for f in result.features]
    assert q.stain_model_sha256(transform) in schema[3]
    changed=q.StainTransform([[.65,.70,.29],[.27,.57,.78]],white=[220,240,250])
    assert [f.name for f in q.pixel_feature_schema(options,changed)]!=schema
    rgb=q.PixelFeatureOptions();rgb.sigmas=[0]
    assert q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),rgb,transform).sha256==q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),rgb,changed).sha256


def test_alpha_empty_domain_and_unused_rgb_do_not_change_features():
    pixels=rgba(6,8);options=q.PixelFeatureOptions();options.sigmas=[0,1]
    result=q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),options)
    changed=pixels.copy();changed[changed[:,:,3]!=255,:3]=255
    other=q.extract_pixel_features(q.PixelBuffer.from_numpy(changed),options)
    assert result.sha256==other.sha256 and result.input_sha256!=other.input_sha256
    pixels[:,:,3]=0
    empty=q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),options)
    assert empty.valid_pixels==0 and not empty.valid_numpy().any() and np.isnan(empty.numpy()).all()


def test_native_ownership_views_copies_and_parallel_transforms():
    pixels=q.PixelBuffer.from_numpy(rgba());options=q.PixelFeatureOptions();options.sigmas=[0,1]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        results=list(pool.map(lambda _:q.extract_pixel_features(pixels,options),range(2)))
    assert results[0].sha256==results[1].sha256
    result=results[0];values=result.numpy();valid=result.valid_numpy()
    assert values.dtype==np.float32 and valid.dtype==np.uint8 and not values.flags.writeable and not valid.flags.writeable
    copy=result.numpy(copy=True);copy[:]=0
    valid_copy=result.valid_numpy(copy=True);valid_copy[:]=0
    descriptor=result.features;descriptor[0].name='changed'
    assert result.features[0].name!='changed' and result.valid_pixels>0
    saved=values.copy();del result,results,pixels;gc.collect()
    np.testing.assert_array_equal(values,saved)


@pytest.mark.parametrize('sigmas', [[],[-1],[0,0],[1,0],[float('nan')],[17],[-0.]])
def test_invalid_scales_reject_before_work(sigmas):
    options=q.PixelFeatureOptions();options.sigmas=sigmas
    with pytest.raises(ValueError):options.validate()


def test_resource_and_context_bounds():
    options=q.PixelFeatureOptions();options.sigmas=[1]
    pixels=q.PixelBuffer.from_numpy(rgba(10,10))
    with pytest.raises(ValueError,match='context'):
        q.extract_pixel_features(pixels,options,window=q.PixelFeatureWindow(30,30,11,11,5,5,10,10))
    with pytest.raises(ValueError):q.extract_pixel_features(pixels,options,window=q.PixelFeatureWindow(10,10,9,9,2,2))
    options.work_bytes=1024*1024
    with pytest.raises((ValueError,RuntimeError),match='budget'):q.pixel_feature_work_bytes(10000,10000,options)
    options.sigmas=[0]
    assert q.pixel_feature_work_bytes(1_000_000_000,1,options,q.PixelFeatureWindow(1_000_000_000,1,500_000_000,0,1,1))<1024*1024
    options.rgb=options.optical_density=options.stains=True;options.sigmas=list(range(8));options.gradient_magnitude=options.laplacian=True
    with pytest.raises(ValueError,match='128'):options.validate()


@pytest.mark.skipif(not q.has_classifier_training,reason='optional classifier training backend')
def test_feature_cube_trains_and_predicts_through_portable_forest_engine():
    pixels=np.zeros((16,20,4),dtype=np.uint8);pixels[:,:,3]=255
    pixels[:,:10,:3]=[240,20,10];pixels[:,10:,:3]=[10,20,240]
    options=q.PixelFeatureOptions();options.sigmas=[0,1]
    features=q.extract_pixel_features(q.PixelBuffer.from_numpy(pixels),options)
    matrix=features.numpy().reshape(-1,features.options.feature_count)
    labels=np.tile(np.arange(20)>=10,16).astype(np.uint32)
    parameters=q.ForestParameters();parameters.trees=16;parameters.max_depth=4
    model=q.train_forest(matrix,labels,features.features,['red','blue'],parameters)
    portable=q.ForestModel.from_bytes(model.to_bytes())
    predictions=portable.predict(matrix,features.features)
    np.testing.assert_array_equal([p.class_index for p in predictions],labels)
