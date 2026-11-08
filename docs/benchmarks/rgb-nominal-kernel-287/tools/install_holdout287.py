from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
fixture=work/'tests/fixtures/qupath-rgb-resampling-holdout-0.7.0'
assert not fixture.exists()
shutil.copytree(root/'build/rgb-nominal-287/holdout',fixture)
files={p.relative_to(fixture).as_posix():dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
       for p in sorted(fixture.rglob('*')) if p.is_file()}
assert len(files)==72
generator=(work/'tools/upstream/qupath_rgb_resampling.groovy').read_text(encoding='utf-8').encode()
manifest=dict(schema='qpx.qupath-rgb-holdout-manifest/1',
    upstream=json.loads((work/'tests/fixtures/qupath-0.7.0/contract.json').read_text())['upstream'],
    generator_sha256=hashlib.sha256(generator).hexdigest(),
    contract_sha256=hashlib.sha256((fixture/'contract.json').read_bytes()).hexdigest(),files=files)
(fixture/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())
spec=importlib.util.spec_from_file_location('reference',work/'tools/reference_rgb_nominal.py')
reference=importlib.util.module_from_spec(spec)
spec.loader.exec_module(reference)
value=reference.generate(work/'tests/fixtures/rgb-nominal-v1')
print('Installed63 held-out QuPath reads and99 independent native golden cases;',len(value['files']),'native payload files')
