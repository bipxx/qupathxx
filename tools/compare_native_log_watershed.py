"""Compare native LoG nucleus labels against all frozen synthetic references.

Requires an installed qupathxx feature producer and the standalone native
qupathxx_log_watershed_tests executable. Does not accept a production detector.
"""
import argparse,gzip,hashlib,importlib.util,json,struct,subprocess,sys,time
from pathlib import Path
import numpy as np
import qupathxx as q
root=Path(__file__).resolve().parents[1]
source=root
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--executable',type=Path,required=True)
parser.add_argument('--output-directory',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
sys.path.insert(0,str(source/'tools'))
import qupath_cells_fixture as fixture
spec=importlib.util.spec_from_file_location('comparison',source/'tools/compare_qupath_cells.py');comparison=importlib.util.module_from_spec(spec);spec.loader.exec_module(comparison)
contract=fixture.read_contract();comparison.verify_capture(comparison.FIXTURES/'capture')
expected=dict(threshold=.4,sigma=1.,min_peak_radius=1.,min_seed_distance=4.,min_nucleus_pixels=16,max_nucleus_pixels=512)
assert all(contract['native'][k]==v for k,v in expected.items())
exe=args.executable.resolve()
environment=json.loads(subprocess.check_output([str(exe),'--environment']))
assert '+dirty' not in environment['git_revision']
out=args.output_directory.resolve();out.mkdir(parents=True,exist_ok=False)
reports=[]
for recipe in ['numpy-od-sum','native-channel-od-sum']:
 for case in contract['cases']:
  rgb=fixture.pixels(contract,case);h,w=rgb.shape[:2]
  if recipe=='numpy-od-sum':score=(-np.log10(np.maximum(rgb.astype(np.float64),1)/255.)).sum(axis=2).astype('<f4')
  else:
   image=q.dense_image_server(q.Image.from_numpy(rgb));pixels=image.read_region(q.RegionRequest(0,0,0,w,h))
   transformed=q.StainTransform(contract['vectors'],contract['white']).optical_densities(pixels)
   channels=np.frombuffer(transformed.data,dtype=np.float32).reshape(h,w,3)
   score=channels.astype(np.float64).sum(axis=2).astype('<f4')
  yy,xx=np.mgrid[:h,:w];xx=xx+.5;yy=yy+.5;domain=np.ones((h,w),dtype=np.uint8)
  if 'roi' in case:
   x,y,bw,bh=case['roi'];domain&=(xx>=x)&(xx<x+bw)&(yy>=y)&(yy<y+bh)
  if 'hole' in case:
   x,y,bw,bh=case['hole'];domain&=~((xx>=x)&(xx<x+bw)&(yy>=y)&(yy<y+bh))
  prefix=out/(recipe+'-'+case['name']);input=prefix.with_suffix('.qlp');output=prefix.with_suffix('.qlr')
  input.write_bytes(b'QLP1'+struct.pack('<II',w,h)+score.tobytes()+domain.tobytes())
  began=time.perf_counter();subprocess.run([str(exe),str(input),str(output)],check=True);elapsed=(time.perf_counter()-began)*1000
  result=output.read_bytes();assert result[:4]==b'QLR1';rw,rh,count=struct.unpack_from('<III',result,4);assert (rw,rh)==(w,h)
  labels=np.frombuffer(result,dtype='<u4',offset=16).reshape(h,w);assert set(np.unique(labels))==set(range(count+1))
  ref=np.frombuffer(gzip.decompress((comparison.FIXTURES/'capture'/case['name']/'nucleus.u32le.gz').read_bytes()),dtype='<u4').reshape(h,w)
  metrics=comparison.metrics(ref,labels,contract['comparison'])
  reports.append(dict(recipe=recipe,case=case['name'],native_count=count,metrics=metrics,input_sha256=hashlib.sha256(input.read_bytes()).hexdigest(),output_sha256=hashlib.sha256(result).hexdigest(),process_ms=elapsed))
  print(recipe,case['name'],count,metrics['foreground_iou'],[(m['iou'],m['centroid_error_px']) for m in metrics['matches']],flush=True)
r=dict(schema='qpx.native-log-watershed-comparison/1',production_integrated=False,scientific_accepted=False,native_environment=environment,feature_producer_git_revision=q.build_environment().git_revision,feature_producer_source_tree_sha256=q.build_environment().source_tree_sha256,native_executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),kernel_sha256=hashlib.sha256((source/'cpp/log_watershed.hpp').read_bytes()).hexdigest(),script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),cases=reports,scope='Native LoG/shape-watershed from feature raster to ROI-clipped nucleus labels on all nine frozen cases with two feature recipes. No production detector/API integration, body expansion, feature/archive/batch/UI, arbitrary-resampling or biology/performance acceptance. Process times include startup and file I/O.')
args.output.write_bytes((json.dumps(r,indent=2)+'\n').encode('utf-8'))
