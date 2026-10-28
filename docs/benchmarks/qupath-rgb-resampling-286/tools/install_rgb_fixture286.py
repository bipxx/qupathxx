from pathlib import Path
import hashlib
import json
import shutil

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
original=root/'build/qupath-rgb-resampling-286/capture2'
fixture=work/'tests/fixtures/qupath-rgb-resampling-0.7.0'
assert not fixture.exists()
shutil.copytree(original,fixture)
contract=json.loads((fixture/'contract.json').read_text())
contract['schema']='qpx.qupath-rgb-resampling-contract/1'
(fixture/'contract.json').write_bytes((json.dumps(contract,indent=2)+'\n').encode('utf-8'))
generator=work/'tools/upstream/qupath_rgb_resampling.groovy'
assert not generator.exists()
shutil.copyfile(root/'build/qupath-rgb-resampling-286/reference.groovy',generator)
upstream=json.loads((work/'tests/fixtures/qupath-0.7.0/contract.json').read_text())['upstream']
files={p.relative_to(fixture).as_posix():dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
       for p in sorted(fixture.rglob('*')) if p.is_file()}
assert len(files)==42
manifest=dict(schema='qpx.qupath-rgb-resampling-manifest/1',upstream=upstream,
    generator_sha256=hashlib.sha256(generator.read_text(encoding='utf-8').encode('utf-8')).hexdigest(),
    contract_sha256=hashlib.sha256((fixture/'contract.json').read_bytes()).hexdigest(),files=files)
(fixture/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode('utf-8'))
print('Installed42 immutable public-API payloads plus manifest and independent generator')
