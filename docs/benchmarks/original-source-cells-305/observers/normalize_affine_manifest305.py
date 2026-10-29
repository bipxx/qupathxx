from pathlib import Path
import importlib.util
import json
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/original-source-305'
spec=importlib.util.spec_from_file_location('affine_manifest305',work/'tools/reference_rgb_affine.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
target=out/'portable-affine-fixture';module.generate(target)
before={p.relative_to(module.FIXTURE).as_posix():p.read_bytes() for p in module.FIXTURE.rglob('*') if p.is_file()}
after={p.relative_to(target).as_posix():p.read_bytes() for p in target.rglob('*') if p.is_file()}
assert before.keys()==after.keys()
assert [name for name in before if before[name]!=after[name]]==['manifest.json']
assert b'\r' not in after['manifest.json']
(module.FIXTURE/'manifest.json').write_bytes(after['manifest.json'])
with (out/'portable-reference-proof.json').open('x',encoding='utf-8') as stream:json.dump(dict(changed=['manifest.json'],unchanged_numerical_members=4,canonical_manifest_newline='LF',reference_generator_has_no_interpolation=True),stream,indent=2);stream.write('\n')
print('Normalized manifest serialization; all four numerical/index members unchanged')
