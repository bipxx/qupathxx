from pathlib import Path
import base64
import csv
import hashlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import zipfile

root = Path(__file__).resolve().parents[2]
work = root / 'build/ci/scientific-grid-159'
out = root / 'build/gui-font-delivery-283'
spec = importlib.util.spec_from_file_location('raw', work / 'tools/ci/raw_classifier_evidence.py')
raw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(raw)
commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=work, text=True).strip()
assert not subprocess.check_output(['git', 'status', '--porcelain'], cwd=work)
proof = raw.source_contract(work, commit)
(out / 'source-proof.json').write_text(json.dumps(proof, indent=2) + '\n')
manifest = json.loads((out / 'compiler-inputs-compatibility.json').read_text())
assert len(manifest['files']) == 415
assert all(hashlib.sha256((work / p).read_bytes()).hexdigest() == h for p, h in manifest['files'].items())
products = []
paths = [v['path'] for v in json.loads((out / 'prior-products.json').read_text())]
paths += ['Release/fonts/NotoSansCJKsc-Regular.otf', 'Release/fonts/NotoEmoji.ttf']
assert len(set(paths)) == 71
for relative in paths:
    source = root / 'build/ci/gridfrontends167-package' / relative
    target = out / 'final-products' / relative
    assert not target.exists()
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    assert target.read_bytes() == source.read_bytes()
    products.append(dict(path=relative, bytes=target.stat().st_size,
        sha256=hashlib.sha256(target.read_bytes()).hexdigest(), retained=str(target)))
(out / 'final-products.json').write_text(json.dumps(products, indent=2) + '\n')
wheel, = list((out / 'initial-wheel').glob('*.whl'))
installed = json.loads((out / 'installed-proof.json').read_text())
assert installed['source'] == manifest['source'] and len(installed['wheel_members']) == 39
assets = {
    'fonts/NotoSansCJKsc-Regular.otf': '2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b',
    'fonts/NotoEmoji.ttf': 'de6c18832938afc99caf132b39d6a30a19bac7f2e812e28db2535b4608d27551',
    'licenses/noto-cjk/OFL.txt': '6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2',
    'licenses/noto-cjk/NOTICE.md': 'aa229a2136c426e3deca957c305727e02415198116138d8c1721222b12e22131',
    'licenses/noto-emoji/OFL.txt': '500bb1ccf43df7bbb522112f9133a52b16e1c35e809632f5d8609b179152de5b',
    'licenses/imgui/ProggyForever-LICENSE.txt': '47d329848d81b5ad777bf5eca73509a4d1b730d6e2ca917f03e72010b2f59576',
}
with zipfile.ZipFile(wheel) as archive:
    for name, expected in assets.items():
        data = archive.read('qupathxx/' + name)
        assert hashlib.sha256(data).hexdigest() == expected, name
        assert (Path(installed['package']) / name).read_bytes() == data
    record, = [n for n in archive.namelist() if n.endswith('.dist-info/RECORD')]
    records = list(csv.reader(io.StringIO(archive.read(record).decode())))
    assert len(records) == 43
    for name, checksum, size in records:
        data = archive.read(name)
        if checksum:
            assert checksum == 'sha256=' + base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
            assert len(data) == int(size)
(out / 'wheel-record-font-proof.json').write_text(json.dumps(dict(entries=len(records),
    package_members=39, font_assets_and_notices=assets,
    wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest()), indent=2) + '\n')
python = root / 'build/ci/gui-font-delivery-283-env/Scripts/python.exe'
commands = [(raw.CONTRACT['typing_positive_log'], 0, [str(work / p) for p in raw.CONTRACT['typing_positive']])]
commands += [(v['log'], 1, [str(work / 'tests/typing' / name)]) for name, v in raw.CONTRACT['typing_negative'].items()]
results = []
environment = os.environ.copy()
environment.pop('MYPYPATH', None)
for name, expected, files in commands:
    args = [str(python), '-I', '-X', 'utf8', '-m', 'mypy', '--strict', '--no-incremental', *files]
    log = out / name
    assert not log.exists()
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run(args, cwd=root, env=environment, stdout=stream, stderr=subprocess.STDOUT)
    results.append(dict(argv=args, returncode=result.returncode))
    assert result.returncode == expected, log
(out / 'typing-commands.json').write_text(json.dumps(results, indent=2) + '\n')
(out / 'typing-admission.json').write_text(json.dumps(raw.typing(out), indent=2) + '\n')
print('Retained71 products;43 RECORD rows,39 package members, six font assets/notices,415 compiler inputs and7/5 typing controls verified', flush=True)
