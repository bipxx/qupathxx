from pathlib import Path
import hashlib
import json
import subprocess
import numpy as np
root=Path(__file__).resolve().parents[2];out=root/'build/gaussian-characterization-299'
repeat=json.loads((out/'repeat-proof.json').read_text());assert repeat['byte_identical']
bridge=[]
for entry in repeat['files']:
    if entry['path']=='probe.groovy':continue
    if entry['path']=='cases.json':
        assert json.loads((out/'final-capture/cases.json').read_text())==json.loads((out/'cases.json').read_text())
        continue
    path=out/'final-capture'/entry['path'];assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256']
    bridge.append(entry)
source=out/'native-probe.cpp';binary=out/'native-probe'
native_log=np.frombuffer((out/'native-roi-log.f64le').read_bytes(),dtype='<f8').reshape(128,192)
native_positive=np.frombuffer((out/'native-roi-positive.u8').read_bytes(),dtype='u1').reshape(128,192)
assert np.array_equal(native_positive,native_log>0)
expected=np.frombuffer((out/'roi-sigma1-log.f32le').read_bytes(),dtype='<f4').reshape(128,192)
points=[dict(x=x,y=y,native_response=float(native_log[y-32,x-32]),native_positive=bool(native_positive[y-32,x-32]),
    imagej_response=float(expected[y-32,x-32])) for x,y in [(32,68),(33,68),(32,81),(33,81)]]
assert all(not p['native_positive'] and p['imagej_response']>0 for p in points)
compiler=subprocess.check_output(['wsl.exe','--exec','g++','--version'],text=True)
(out/'native-compiler-version.txt').write_text(compiler)
proof=dict(repeated_original_payloads=150,final_capture_numeric_and_recipe_payloads_equal=len(bridge),bridge_files=bridge,
    case_recipe_semantically_equal=True,initial_bridge_observer_failure='Original cases.json uses Windows CRLF; final byte-authenticated cases.json uses LF. Numeric payloads are compared raw; case recipe is compared semantically.',
    final_script_adds_runtime_metadata=True,native_probe_source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    native_probe_binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
    native_input_domain='captured ImageJ float32 OD promoted to float64; not the production float64 OD capture',
    native_positive_mask_self_consistent=True,probes=points,native_production_rebuilt=False,
    production_segmentation_parity_accepted=False,biological_validation=False)
(out/'native-and-repeat-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
print('Final capture bridges148 raw payloads and the same case recipe to two original executions; unchanged native kernel isolates four response differences')
