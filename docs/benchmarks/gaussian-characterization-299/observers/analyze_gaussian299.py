from pathlib import Path
import json
import numpy as np
root=Path(__file__).resolve().parents[2];out=root/'build/gaussian-characterization-299'
cases=json.loads((out/'cases.json').read_text())
def log(values):
    a=values.astype(np.float64);pad=np.pad(a,1,mode='edge')
    return ((a-pad[1:-1,:-2])+(a-pad[1:-1,2:]))+((a-pad[:-2,1:-1])+(a-pad[2:,1:-1]))
checks=[]
for case in cases:
    name=case['name'];shape=(case['height'],case['width'])
    blur=np.frombuffer((out/(name+'-blur.f32le')).read_bytes(),dtype='<f4').reshape(shape)
    reference=np.frombuffer((out/(name+'-log.f32le')).read_bytes(),dtype='<f4').reshape(shape)
    computed=log(blur).astype('<f4')
    checks.append(dict(name=name,laplacian_from_captured_blur_exact=np.array_equal(computed,reference),
        maximum_error=float(np.max(np.abs(computed-reference))),positive_mismatches=int(np.count_nonzero((computed>0)!=(reference>0)))))
assert all(p['laplacian_from_captured_blur_exact'] for p in checks)
score=np.frombuffer((out/'roi-sigma1-input.f32le').read_bytes(),dtype='<f4').reshape(128,192)
(out/'roi-f32-input-as-f64.f64le').write_bytes(score.astype('<f8').tobytes())
half=np.asarray(json.loads((out/'roi-sigma1-kernel.json').read_text())[0])
def gaussian(radius,replicate):
    kernel=np.exp(-.5*np.arange(-radius,radius+1,dtype=float)**2)
    values=score.astype(float);weights=np.ones_like(values)
    for axis in (1,0):
        pad=[(0,0),(0,0)];pad[axis]=(radius,radius)
        v=np.pad(values,pad,mode='edge' if replicate else 'constant')
        w=np.pad(weights,pad,mode='edge' if replicate else 'constant')
        shape=values.shape;values=np.zeros(shape);weights=np.zeros(shape)
        for j,k in enumerate(kernel):
            sl=[slice(None),slice(None)];sl[axis]=slice(j,j+shape[axis])
            values+=v[tuple(sl)]*k;weights+=w[tuple(sl)]*k
    return values/weights
reference=np.frombuffer((out/'roi-sigma1-log.f32le').read_bytes(),dtype='<f4').reshape(score.shape)
points=[(32,68),(33,68),(32,81),(33,81)]
variants=[]
for radius in (3,len(half)-1):
    for replicate in (False,True):
        response=log(gaussian(radius,replicate))
        variants.append(dict(radius=radius,replicate=replicate,positive_mismatches=int(np.count_nonzero((response>0)!=(reference>0))),
            probes=[dict(x=x,y=y,response=float(response[y-32,x-32]),positive=bool(response[y-32,x-32]>0)) for x,y in points]))
summary=dict(reference_laplacian_checks=checks,sigma1_imagej_half_kernel=half.tolist(),sigma1_native_radius=3,
    diagnostic_float64_models=variants,diagnostics_are_not_production_equivalence=True)
(out/'analysis.json').write_text(json.dumps(summary,indent=2)+'\n')
print('All37 ImageJ LoG arrays reconstructed exactly from captured blur using zero-flux neighbor Laplacian')
print(json.dumps(variants,indent=2))
