"""Installed synthetic pixel pipeline and exact halo-tile oracle; no biological claim."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import time
import numpy as np
import qupathxx as q
from qupathxx import _native


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--size',type=int,default=512);ap.add_argument('--tile',type=int,default=128)
    ap.add_argument('--repeat',type=int,default=3);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    if not 64<=args.size<=1024 or not 32<=args.tile<=512 or not 1<=args.repeat<=10:ap.error('size64..1024 tile32..512 repeat1..10')
    n=args.size;y,x=np.indices((n,n));rgba=np.empty((n,n,4),dtype=np.uint8)
    expected=(x>=n//2).astype(np.uint32)+1
    rgba[:,:,0]=np.where(expected==1,220+x%16,10+x%16);rgba[:,:,1]=30+y%16;rgba[:,:,2]=np.where(expected==1,10+y%16,220+y%16);rgba[:,:,3]=255
    rgba[::37,::41,3]=0;expected[rgba[:,:,3]!=255]=0
    pixels=q.PixelBuffer.from_numpy(rgba);options=q.PixelFeatureOptions();options.sigmas=[0,1,2];options.gradient_magnitude=True
    train_pixels=q.PixelBuffer.from_numpy(rgba[::max(1,n//64),::max(1,n//64)].copy())
    started=time.perf_counter();training=q.extract_pixel_features(train_pixels,options);valid=training.valid_numpy().ravel().astype(bool)
    target=expected[::max(1,n//64),::max(1,n//64)].ravel()[valid]-1
    parameters=q.ForestParameters();parameters.trees=16;parameters.max_depth=4
    forest=q.train_forest(training.numpy().reshape(-1,options.feature_count)[valid],target,training.features,['red','blue'],parameters)
    recipe=q.PixelClassifier.from_bytes(q.PixelClassifier(forest,options).to_bytes());training_ms=(time.perf_counter()-started)*1000
    # Warm both extraction and portable inference before measured observations.
    q.classify_pixels(train_pixels,recipe)
    samples=[];full=None
    for _ in range(args.repeat):
        started=time.perf_counter();full=q.classify_pixels(pixels,recipe);wall=(time.perf_counter()-started)*1000
        np.testing.assert_array_equal(full.labels_numpy(),expected)
        samples.append(dict(wall_ms=wall,native_ms=full.elapsed_ms,estimated_work_bytes=full.estimated_work_bytes))
    stitched=np.zeros((n,n),dtype=np.uint32);fractions=np.empty((n,n),dtype=np.float32);halo=options.halo;maximum_work=0;tiles=0
    started=time.perf_counter()
    for top in range(0,n,args.tile):
        for left in range(0,n,args.tile):
            w=min(args.tile,n-left);h=min(args.tile,n-top);ix=max(0,left-halo);iy=max(0,top-halo);right=min(n,left+w+halo);bottom=min(n,top+h+halo)
            tile=q.PixelBuffer.from_numpy(rgba[iy:bottom,ix:right].copy())
            result=q.classify_pixels(tile,recipe,window=q.PixelFeatureWindow(n,n,left,top,w,h,ix,iy))
            stitched[top:top+h,left:left+w]=result.labels_numpy();fractions[top:top+h,left:left+w]=result.vote_fractions_numpy()
            maximum_work=max(maximum_work,result.estimated_work_bytes);tiles+=1
    tiled_wall=(time.perf_counter()-started)*1000
    np.testing.assert_array_equal(stitched,full.labels_numpy());np.testing.assert_array_equal(fractions,full.vote_fractions_numpy())
    env=q.build_environment();report=dict(schema='qpx.pixel-classifier-benchmark/1',size=n,tile=args.tile,tiles=tiles,feature_count=options.feature_count,halo=halo,
        training_ms=training_ms,full_samples=samples,median_full_wall_ms=statistics.median(s['wall_ms'] for s in samples),tiled_wall_ms=tiled_wall,
        maximum_tile_estimated_work_bytes=maximum_work,full_sha256=full.sha256,input_sha256=full.input_sha256,model_sha256=recipe.sha256,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),platform=platform.platform(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        verified=['every synthetic class label','portable recipe roundtrip','halo-tiled labels and vote fractions exactly equal full output'],
        scope='Synthetic RGB grid; sampled-grid-pixel sigmas, 16 trees/depth4. Three full calls by default and one sequential tiled traversal on a shared workstation. Training uses a subsampled synthetic grid, not biological labels. Work estimates exclude caller RGBA, retained model, Python allocations and process runtime. Tiled timing includes Python slicing/copying, native extraction/inference and stitching. No paired implementation speedup, hardware/frame budget or research accuracy acceptance.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
