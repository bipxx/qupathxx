from pathlib import Path
import hashlib
import importlib.util
import json
import subprocess
import qupathxx as q
from qupathxx import _native

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/qupath-rgb-resampling-286'
commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert commit.startswith('83cd41df')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
assert q.build_environment().git_revision=='7231d95c25fbc9515d937036986e8c92b19da261'
manifest=json.loads((root/'build/gui-font-delivery-283/compiler-inputs-compatibility.json').read_text())
assert len(manifest['files'])==415
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in manifest['files'].items())
installed=json.loads((root/'build/gui-font-delivery-283/installed-proof.json').read_text())
entry,=[v for v in installed['wheel_members'] if v['path'].endswith('.pyd')]
assert hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest()==entry['sha256']
products=json.loads((root/'build/gui-font-delivery-283/final-products.json').read_text())
assert len(products)==71
assert all(hashlib.sha256(Path(v['retained']).read_bytes()).hexdigest()==v['sha256'] for v in products)
spec=importlib.util.spec_from_file_location('scientific',work/'tools/ci/scientific_grid_evidence.py')
science=importlib.util.module_from_spec(spec)
spec.loader.exec_module(science)
proof=science.source_contract(work,commit)
(out/'scientific-source-proof.json').write_text(json.dumps(proof,indent=2)+'\n')
fixture=work/'tests/fixtures/qupath-rgb-resampling-0.7.0'
repeat=out/'capture3'
entries=[]
for p in sorted(fixture.rglob('*')):
    if not p.is_file():continue
    other=repeat/p.relative_to(fixture)
    assert p.read_bytes()==other.read_bytes()
    entries.append(dict(path=p.relative_to(fixture).as_posix(),bytes=p.stat().st_size,
        sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
assert len(entries)==43
(out/'repeat-file-proof.json').write_text(json.dumps(dict(files=entries),indent=2)+'\n')
result=dict(comparison_source=commit,native_source=q.build_environment().git_revision,
    source_tree_sha256=q.build_environment().source_tree_sha256,
    native_path=str(_native.__file__),native_sha256=entry['sha256'],unchanged_compiler_inputs=415,
    unchanged_retained_native_products=71,authenticated_scientific_source_paths=len(proof),
    source_clean=True,product_rebuilt=False,local_binary_relabelled=False,
    platform_acceptance_transferred=False,shared_test_dependencies=True)
(out/'source-runtime-proof.json').write_text(json.dumps(result,indent=2)+'\n')
print('Scientific source paths',len(proof),';415 compiler inputs and71 retained products unchanged;43 repeated fixture files exact',flush=True)
