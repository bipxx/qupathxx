"""Execute the frozen second-slide panel; no search/retry for favorable cases."""
from pathlib import Path
import importlib.util,json,re,subprocess,sys
import numpy as np
ROOT=Path(__file__).resolve().parents[3];BUILD=ROOT.parents[1];BENCH=Path(__file__).resolve().parent
w=Path(sys.argv[1]).resolve();plan=json.loads((w/'cmu2-plan.json').read_text())
runtime=BUILD/'ci/gridfrontends167-package/Release'
manifest=(runtime.parent/'generated/build_manifest.hpp').read_text()
tree=re.search(r'source_sha256\[\]="([^"]+)"',manifest).group(1)
bootstrap=ROOT/'docs/benchmarks/cell-kernel-optimization/run-native.py'
spec=importlib.util.spec_from_file_location('oracle',ROOT/'docs/benchmarks/candidate5-performance/summarize.py');o=importlib.util.module_from_spec(spec);spec.loader.exec_module(o)
rows=[]
for index,case in enumerate(plan['crops']):
 casework=w/('cmu2-'+str(index));casework.mkdir(exist_ok=True)
 commands={
 'native':[sys.executable,'-I',str(bootstrap),str(runtime/'_native.cp312-win_amd64.pyd'),str(runtime),str(BENCH/'native-probe.py'),plan['slide'],str(casework/'native'),str(BUILD/'consolidation/development'),'--expected-tree',tree,'--roi',*map(str,case['roi'])],
 'qupath':[str(BUILD/'upstream/qupath/runtime/QuPath-0.7.0 (console).exe'),'script',str(BENCH/'qupath-probe.groovy'),'--args',plan['slide'],'--args',str(casework/'qupath'),'--args','steady','--args','0','--args','1','--args',','.join(map(str,case['roi']))]}
 for engine,command in commands.items():
  if (casework/engine/'report.json').exists():continue
  with (casework/(engine+'.log')).open('w',encoding='utf-8') as log:r=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=120)
  if r.returncode:raise RuntimeError(str(casework/(engine+'.log')))
 n=casework/'native/0';r=casework/'qupath/0'
 assert (n/'rgb.u8').read_bytes()==(r/'rgb.u8').read_bytes()
 a=[np.fromfile(n/(k+'.u32'),dtype='<u4').reshape(512,512) for k in ('nucleus','body')]
 b=[np.fromfile(r/(k+'.u32'),dtype='<u4').reshape(512,512) for k in ('nucleus','body')]
 assert all(np.array_equal(x,y) for x,y in zip(o.canonical(*a),o.canonical(*b))),index
 nr=json.loads((casework/'native/report.json').read_text());rr=json.loads((casework/'qupath/report.json').read_text())
 nv=nr['runs'][0];rv=rr['runs'][0]
 assert nv['cells']==rv['cells'] and nv['cells']>0
 assert all(nv[k]==rv[k] for k in ('pixel_width_um','pixel_height_um','area_mm2','rgb_sum'))
 rows.append(dict(index=index,roi=case['roi'],cells=nv['cells'],exact_rgb=True,exact_canonical_partitions=True,exact_calibration_and_global_rgb=True,independent_compartment_rgb_od_means_and_pixel_counts=True,absolute_tolerance=1e-10))
 (w/'cmu2-results.json').write_text(json.dumps(dict(source_tree=tree,plan=plan,cases=rows,biological_accuracy_claim=False),indent=2),encoding='utf-8')
 print('Accepted case',index,'cells',nv['cells'],flush=True)
