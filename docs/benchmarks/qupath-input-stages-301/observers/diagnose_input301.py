from pathlib import Path
import gzip
import hashlib
import importlib.util
import json
import subprocess
import numpy as np
from PIL import Image
import qupathxx as q
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/qupath-input-stages-301';out.mkdir(exist_ok=False)
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
debug=load('input301_roi',work/'tools/qupath_roi_cell_debug.py');debug.verify()
ordinary=work/'tests/fixtures/qupath-cells-0.7.0/capture/roi_cut_hole/input.png'
with Image.open(ordinary) as image:roi=np.asarray(image.convert('RGB'))[32:160,32:224].copy()
resampled=load('input301_resampled',work/'tools/qupath_resampled_cell_debug.py');_,_,rgb,_,_,_=resampled.verified_data(resampled.FIXTURE)
y,x=np.indices((256,256));pairwise=np.stack([x,y,x^y],axis=2).astype(np.uint8)
images={'pairwise':pairwise,'roi':roi,'resampled':rgb}
for name,array in images.items():(out/(name+'.rgb')).write_bytes(array.tobytes())
basis=[[.65,.70,.29],[.07,.99,.11]]
white_models=[[255,255,255],[240,250,230],[1,1,1],[1.25,127.5,254.5],[31.5,128,200],[255,1,255]]
cases=[dict(name='white-'+str(i),image='pairwise',white=white,vectors=basis) for i,white in enumerate(white_models)]
cases += [dict(name='identity',image='pairwise',white=[255,255,255],vectors=[[1,0,0],[0,1,0],[0,0,1]]),
    dict(name='three-explicit',image='pairwise',white=[240.5,251.25,232.75],vectors=[[.7,.3,.2],[.1,.8,.3],[.2,.15,.9]]),
    dict(name='signed-residual',image='pairwise',white=[255,255,255],vectors=[[.8,.4,.2],[.2,.8,.4]]),
    dict(name='roi-default',image='roi',white=[255,255,255],vectors=basis),
    dict(name='resampled-default',image='resampled',white=[255,255,255],vectors=basis)]
for case in cases:case.update(width=images[case['image']].shape[1],height=images[case['image']].shape[0])
(out/'cases.json').write_bytes((json.dumps(cases,indent=2)+'\n').encode())
script=out/'probe.groovy';script.write_text('''import ij.process.ColorProcessor
import qupath.imagej.tools.IJTools
import qupath.lib.color.ColorDeconvolutionHelper
import qupath.lib.color.ColorDeconvolutionStains
import qupath.lib.color.StainVector
import qupath.lib.io.GsonTools
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.file.Files
import java.nio.file.Path
import java.util.zip.GZIPOutputStream
def root=Path.of(args[0]);def gson=GsonTools.getInstance(true)
def cases=gson.fromJson(Files.readString(root.resolve('cases.json')),List)
def writeFloats={name,values ->
    def bytes=ByteBuffer.allocate(values.length*4).order(ByteOrder.LITTLE_ENDIAN)
    for(float value:values)bytes.putFloat(value)
    def stream=new GZIPOutputStream(Files.newOutputStream(root.resolve(name)))
    try {stream.write(bytes.array())}finally{stream.close()}
}
for(item in cases) {
    int w=item.width,h=item.height
    def raw=Files.readAllBytes(root.resolve(item.image+'.rgb'));int[] pixels=new int[w*h]
    for(int i=0;i<pixels.length;i++)pixels[i]=((raw[3*i]&255)<<16)|((raw[3*i+1]&255)<<8)|(raw[3*i+2]&255)
    def cp=new ColorProcessor(w,h,pixels)
    def vector=item.vectors.collectWithIndex {v,index -> StainVector.createStainVector('S'+index,*(v.collect {it as double}))}
    def stains=vector.size()==2 ? new ColorDeconvolutionStains(item.name,vector[0],vector[1],*(item.white.collect {it as double})) :
        new ColorDeconvolutionStains(item.name,vector[0],vector[1],vector[2],*(item.white.collect {it as double}))
    for(int c=0;c<3;c++) {
        def fp=cp.toFloat(c,null)
        ColorDeconvolutionHelper.convertPixelsToOpticalDensities((float[])fp.getPixels(),item.white[c] as double,true)
        writeFloats(item.name+'-od'+c+'.f32le.gz',fp.getPixels())
    }
    writeFloats(item.name+'-sum.f32le.gz',IJTools.convertToOpticalDensitySum(cp,*(item.white.collect {it as double})).getPixels())
    def separated=IJTools.colorDeconvolve(cp,stains)
    for(int c=0;c<3;c++)writeFloats(item.name+'-stain'+c+'.f32le.gz',separated[c].getPixels())
    Files.writeString(root.resolve(item.name+'-model.json'),gson.toJson([normalized:(1..3).collect {stains.getStain(it).getArray()},inverse:stains.getMatrixInverse()])+'\\n')
}
''',encoding='utf-8')
runtime=load('input301_runtime',work/'tools/fetch_qupath.py').fetch(root/'build/upstream/qupath')
args=[str(runtime),'script','--args',str(out),str(script)]
with (out/'driver.log').open('xb') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
(out/'command.json').write_text(json.dumps(dict(argv=args,returncode=r.returncode),indent=2)+'\n');assert r.returncode==0,out/'driver.log'
reports=[]
for case in cases:
    name=case['name'];array=images[case['image']];h,w=array.shape[:2]
    server=q.dense_image_server(q.Image.from_numpy(array));pixels=server.read_region(q.RegionRequest(0,0,0,w,h))
    transform=q.StainTransform(case['vectors'],case['white']);od=transform.optical_densities(pixels).numpy().copy()
    reference=lambda kind:np.frombuffer(gzip.decompress((out/(name+'-'+kind+'.f32le.gz')).read_bytes()),dtype='<f4').reshape(h,w)
    wanted=[reference('od'+str(c)) for c in range(3)];total=(od[:,:,0]+od[:,:,1])+od[:,:,2]
    actual_stains=transform.deconvolve(pixels).numpy().copy();model=json.loads((out/(name+'-model.json')).read_text())
    normalized=q.StainTransform.from_normalized(model['normalized'],case['white']).deconvolve(pixels).numpy().copy()
    def difference(actual,expected):
        delta=np.abs(actual.astype(float)-expected.astype(float));where=np.argwhere(actual.view('u4')!=expected.view('u4'))
        return dict(changed_components=len(where),maximum_absolute_error=float(delta.max()),first_coordinates=where[:8].tolist())
    reports.append(dict(name=name,od_channels=[difference(od[:,:,c].copy(),wanted[c]) for c in range(3)],
        sum=difference(total,reference('sum')),stains=[difference(actual_stains[:,:,c].copy(),reference('stain'+str(c))) for c in range(3)],
        normalized_stains=[difference(normalized[:,:,c].copy(),reference('stain'+str(c))) for c in range(3)]))
    server.close()
summary=dict(native_source=q.build_environment().git_revision,cases=reports,metric='exact float32 bits; no tolerance fitted',native_changed=False)
(out/'initial-comparison.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps([{k:sum(r['changed_components'] for r in v) if isinstance(v,list) else v['changed_components'] for k,v in report.items() if k!='name'}|{'name':report['name']} for report in reports],indent=2))
