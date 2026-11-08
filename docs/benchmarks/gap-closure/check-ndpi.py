"""Independent exact partition/RGB check for a new anisotropic NDPI crop."""
from pathlib import Path
import importlib.util,json,hashlib
import numpy as np
ROOT=Path(__file__).resolve().parents[3]
import sys
w=Path(sys.argv[1])
p=ROOT/'docs/benchmarks/candidate5-performance/summarize.py'
spec=importlib.util.spec_from_file_location('oracle',p);o=importlib.util.module_from_spec(spec);spec.loader.exec_module(o)
n=w/'ndpi-native/0';r=w/'ndpi-qupath/0'
a=[np.fromfile(n/(k+'.u32'),dtype='<u4').reshape(256,256) for k in ('nucleus','body')]
b=[np.fromfile(r/(k+'.u32'),dtype='<u4').reshape(256,256) for k in ('nucleus','body')]
reports={k:json.loads((w/k/'report.json').read_text()) for k in ('ndpi-native','ndpi-qupath')}
checks=dict(rgb_exact=(n/'rgb.u8').read_bytes()==(r/'rgb.u8').read_bytes(),canonical_partitions_exact=all(np.array_equal(x,y) for x,y in zip(o.canonical(*a),o.canonical(*b))))
rows=[v['runs'][-1] for v in reports.values()]
checks['counts_exact']=rows[0]['cells']==rows[1]['cells']
checks['calibration_exact']=all(rows[0][k]==rows[1][k] for k in ('pixel_width_um','pixel_height_um','area_mm2'))
result=dict(checks=checks,roi=[11392,17792,256,256],cells=rows[0]['cells'],calibration_um=[rows[0]['pixel_width_um'],rows[0]['pixel_height_um']],source_tree=reports['ndpi-native']['source_tree_sha256'],independent_compartment_rgb_arithmetic=True,biological_validation=False)
(w/'ndpi-checks.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
assert all(checks.values()),checks
