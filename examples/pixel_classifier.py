"""Train/save/reopen a pixel recipe; classify an owned RGBA buffer."""
import tempfile
from pathlib import Path
import numpy as np
import qupathxx as q


def main():
    rgba=np.zeros((64,96,4),dtype=np.uint8);rgba[:,:,3]=255
    rgba[:,:48,:3]=[220,30,10];rgba[:,48:,:3]=[10,30,220]
    pixels=q.PixelBuffer.from_numpy(rgba)
    options=q.PixelFeatureOptions();options.sigmas=[0,1]
    features=q.extract_pixel_features(pixels,options)
    labels=np.tile(np.arange(96)>=48,64).astype(np.uint32)
    parameters=q.ForestParameters();parameters.trees=16;parameters.max_depth=4
    forest=q.train_forest(features.numpy().reshape(-1,options.feature_count),labels,features.features,['red','blue'],parameters)
    recipe=q.PixelClassifier(forest,options)
    with tempfile.TemporaryDirectory(prefix='qpx-pixel-example-') as folder:
        path=str(Path(folder)/'model.qpc');q.save_pixel_classifier(recipe,path)
        result=q.classify_pixels(pixels,q.load_pixel_classifier(path))
    np.testing.assert_array_equal(result.labels_numpy(),labels.reshape(64,96)+1)
    print(dict(classes=result.classes,counts=result.class_counts,model_sha256=result.model_sha256))


if __name__=='__main__':main()
