from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parents[2]
work = root / 'build/ci/scientific-grid-159'
out = root / 'build/cell-processing-policy-302'
out.mkdir(exist_ok=True)
assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=work)
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=work, text=True).strip()
assert source.startswith('2b4786ed')
names = {'CMakeLists.txt', 'pyproject.toml'}
for folder, suffixes in [('cpp', {'.cpp', '.hpp'}), ('cmake', {'.cmake', '.in'}), ('src', {'.py'})]:
    names.update(p.relative_to(work).as_posix() for p in (work/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
files = {p: hashlib.sha256((work/p).read_bytes()).hexdigest() for p in sorted(names)}
fingerprint = hashlib.sha256(''.join(p+'='+h+'\n' for p,h in files.items()).encode()).hexdigest()
assert len(files) == 429
prior = json.loads((root/'build/reader-locale-297/final-products.json').read_text())
assert len(prior) == 71
for product in prior:
    assert hashlib.sha256(Path(product['retained']).read_bytes()).hexdigest() == product['sha256']
    assert hashlib.sha256((root/'build/ci/gridfrontends167-package'/product['path']).read_bytes()).hexdigest() == product['sha256']
(out/'prior-products.json').write_text(json.dumps(prior, indent=2)+'\n')
(out/'compiler-inputs-initial.json').write_text(json.dumps(dict(source=source, fingerprint=fingerprint, files=files), indent=2)+'\n')
backend = root/'build/ci/cell_policy302_backend.py'
assert not backend.exists()
backend.write_text((root/'build/ci/rgb_sdk289_backend.py').read_text().replace('rgb-nominal-sdk-289', 'cell-processing-policy-302'), encoding='utf-8')
args = [str(root/'.venv/Scripts/python.exe'), '-X', 'utf8', str(backend)]
with (out/'build.log').open('x', encoding='utf-8') as stream:
    result = subprocess.run(args, stdout=stream, stderr=subprocess.STDOUT)
(out/'build-exit.json').write_text(json.dumps(dict(argv=args, returncode=result.returncode), indent=2)+'\n')
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest() == h for p,h in files.items())
print('Cell policy302 build exit', result.returncode, flush=True)
assert result.returncode == 0
