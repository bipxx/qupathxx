from pathlib import Path
import importlib.util
import json
import hashlib
import shutil
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304'
first=out/'basin-capture1';second=out/'basin-capture2';shared=sorted(p.name for p in first.iterdir() if p.is_file() and p.name not in ('manifest.json','driver.log'))
assert len(shared)==102
for name in shared:assert (first/name).read_bytes()==(second/name).read_bytes(),name
target=work/'tests/fixtures/qupath-response-basins-0.7.0';assert not target.exists();shutil.copytree(first,target)
spec=importlib.util.spec_from_file_location('response304_reference',work/'tools/reference_qupath_response.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
module.seal(target);module.HEADER.write_bytes(module.header(target))
(out/'response-repeat-proof.json').write_text(json.dumps(dict(shared_payloads=102,all_shared_payloads_byte_exact=True,original_captures_preserved=True,manifest_augmented_with_pinned_upstream=True,first_manifest_sha256=hashlib.sha256((first/'manifest.json').read_bytes()).hexdigest(),second_manifest_sha256=hashlib.sha256((second/'manifest.json').read_bytes()).hexdigest()),indent=2)+'\n',encoding='utf-8')
print('Promoted33 public basin vectors;102 repeated payloads unchanged')
