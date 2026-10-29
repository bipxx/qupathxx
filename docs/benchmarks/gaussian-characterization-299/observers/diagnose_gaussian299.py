from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/gaussian-characterization-299'
out.mkdir(exist_ok=False)
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
debug=load('gaussian299_debug',work/'tools/qupath_roi_cell_debug.py');debug.verify()
runtime=load('gaussian299_runtime',work/'tools/fetch_qupath.py').fetch(root/'build/upstream/qupath')
with Image.open(debug.FIXTURE/'debug-stack.tif') as image:
    image.seek(0);score=np.asarray(image).copy();image.seek(1);reference=np.asarray(image).copy()
cases=[]
def add(name,values,sigma):
    raw=np.asarray(values,dtype='<f4');assert raw.ndim==2
    (out/(name+'-input.f32le')).write_bytes(raw.tobytes())
    cases.append(dict(name=name,width=raw.shape[1],height=raw.shape[0],sigma=sigma,input_sha256=hashlib.sha256(raw.tobytes()).hexdigest()))
add('roi-sigma1',score,1.)
patterns={'constant':np.full((9,11),.75),'edge-impulse':np.pad(np.array([[1.]]),((0,8),(0,10))),
    'center-impulse':np.pad(np.array([[1.]]),((4,4),(5,5))),
    'ramp':np.fromfunction(lambda y,x:((37*x+71*y)%53)/53,(17,19)),
    'vertical':np.arange(9).reshape(9,1)/8,'horizontal':np.arange(9).reshape(1,9)/8}
for name,values in patterns.items():
    for sigma in (.25,1.,2.,8.5,8.500001,16.):add(name+'-'+str(sigma),values,sigma)
(out/'cases.json').write_text(json.dumps(cases,indent=2)+'\n')
script=out/'probe.groovy'
script.write_text('''import ij.process.FloatProcessor
import ij.plugin.filter.GaussianBlur
import qupath.lib.io.GsonTools
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.file.Files
import java.nio.file.Path
def root=Path.of(args[0])
def cases=GsonTools.getInstance().fromJson(Files.readString(root.resolve('cases.json')),List)
def writeFloats={path, values ->
    def bytes=ByteBuffer.allocate(values.length*4).order(ByteOrder.LITTLE_ENDIAN)
    for(float value:values) bytes.putFloat(value)
    Files.write(path,bytes.array())
}
for(item in cases) {
    int w=item.width,h=item.height
    def buffer=ByteBuffer.wrap(Files.readAllBytes(root.resolve(item.name+'-input.f32le'))).order(ByteOrder.LITTLE_ENDIAN)
    float[] values=new float[w*h]
    for(int i=0;i<values.length;i++) values[i]=buffer.getFloat()
    def fp=new FloatProcessor(w,h,values)
    fp.blurGaussian(item.sigma as double)
    writeFloats(root.resolve(item.name+'-blur.f32le'),fp.getPixels())
    fp.convolve([0f,-1f,0f,-1f,4f,-1f,0f,-1f,0f] as float[],3,3)
    writeFloats(root.resolve(item.name+'-log.f32le'),fp.getPixels())
    def kernel=new GaussianBlur().makeGaussianKernel(item.sigma as double,0.0002d,Math.max(w,h))
    Files.writeString(root.resolve(item.name+'-kernel.json'),GsonTools.getInstance(true).toJson(kernel)+'\\n')
}
''',encoding='utf-8')
args=[str(runtime),'script','--args',str(out),str(script)]
with (out/'driver.log').open('xb') as stream:result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
(out/'command.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n')
assert result.returncode==0,out/'driver.log'
actual=np.frombuffer((out/'roi-sigma1-log.f32le').read_bytes(),dtype='<f4').reshape(reference.shape)
assert np.array_equal(actual,reference),np.max(np.abs(actual-reference))
summary=dict(cases=len(cases),reference_roi_log_bytes_exact=actual.tobytes()==reference.astype('<f4').tobytes(),
    kernel_files=len(list(out.glob('*-kernel.json'))),biological_validation=False,native_modified=False,
    probes=[dict(x=x,y=y,log=float(actual[y-32,x-32])) for x,y in [(32,68),(33,68),(32,81),(33,81)]])
(out/'initial-result.json').write_text(json.dumps(summary,indent=2)+'\n')
print(summary,flush=True)
