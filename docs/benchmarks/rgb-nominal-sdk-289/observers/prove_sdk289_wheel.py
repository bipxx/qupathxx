from pathlib import Path
import base64
import csv
import hashlib
import io
import json
import zipfile
import qupathxx as q
from qupathxx import _native

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-sdk-289'
initial,=list((out/'initial-wheel').glob('*.whl'))
final,=list((out/'final-wheel').glob('*.whl'))
package=Path(q.__file__).parent
assert package.parent==Path(__import__('sys').prefix)/'Lib/site-packages'
manifest=json.loads((out/'compiler-inputs-initial.json').read_text())
assert q.build_environment().git_revision==manifest['source']
assert q.build_environment().source_tree_sha256==manifest['fingerprint']
assert q.has_gui and q.has_geos and q.has_tiff and q.has_project and q.has_classifier_training and not q.has_openslide
assert all(hasattr(q,n) and n in q.__all__ for n in ('RgbNominalRequest','read_rgb_nominal_region'))
with zipfile.ZipFile(initial) as before,zipfile.ZipFile(final) as after:
    assert before.testzip() is None and after.testzip() is None
    assert before.namelist()==after.namelist()
    changed=[p for p in before.namelist() if before.read(p)!=after.read(p)]
    record,=[n for n in after.namelist() if n.endswith('.dist-info/RECORD')]
    assert set(changed)=={'qupathxx/_native.pyi','qupathxx/__init__.pyi',record}
    rows=list(csv.reader(io.StringIO(after.read(record).decode())))
    assert len(rows)==43
    for name,checksum,size in rows:
        data=after.read(name)
        if checksum:
            assert checksum=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
            assert len(data)==int(size)
    entries=[]
    for name in after.namelist():
        if not name.startswith('qupathxx/') or name.endswith('/'):continue
        data=after.read(name)
        assert (package.parent/name).read_bytes()==data,name
        entries.append(dict(path=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
    assert len(entries)==39
    for name in ('_native.pyi','__init__.pyi'):
        assert (package/name).read_bytes()==(work/'src/qupathxx'/name).read_bytes()
result=dict(native_source=manifest['source'],fingerprint=manifest['fingerprint'],package=str(package),native=str(_native.__file__),
    initial_wheel_sha256=hashlib.sha256(initial.read_bytes()).hexdigest(),final_wheel_sha256=hashlib.sha256(final.read_bytes()).hexdigest(),
    changed_members=changed,wheel_records=43,package_members=entries,unchanged_native_and_font_bytes=True,
    initial_verifier_error='Reusing ZipInfo objects during writestr updated their cached header offsets; the completed output wheel is valid. Fresh independent readers verify both archives and all installed bytes.',
    binary_rebuilt=False,binary_relabelled=False,shared_test_dependencies=True)
(out/'stub-only-wheel-proof.json').write_text(json.dumps(result,indent=2)+'\n')
print('Fresh ZIP readers verify both archives, all39 installed members and43 records; only two stubs/RECORD changed',flush=True)
