from pathlib import Path
import base64
import csv
import copy
import hashlib
import io
import json
import zipfile

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/original-source-305'
manifest=json.loads((out/'compiler-inputs-initial.json').read_text(encoding='utf-8'))
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in manifest['files'].items())
initial,=list((out/'initial-wheel').glob('*.whl'))
target=out/'final-wheel'/initial.name
target.parent.mkdir(exist_ok=False)
with zipfile.ZipFile(initial) as before:
    values={item.filename:before.read(item) for item in before.infolist()}
    changed=[]
    for name in ('_native.pyi','__init__.pyi'):
        key='qupathxx/'+name
        data=(work/'src/qupathxx'/name).read_bytes()
        assert data!=values[key]
        values[key]=data;changed.append(key)
    record,=[n for n in values if n.endswith('.dist-info/RECORD')]
    rows=list(csv.reader(io.StringIO(values[record].decode())))
    assert len(rows)==43
    for row in rows:
        name=row[0]
        if name==record:row[1:]=['','']
        else:
            data=values[name]
            row[1:]=['sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('='),str(len(data))]
    buffer=io.StringIO(newline='');csv.writer(buffer,lineterminator='\n').writerows(rows)
    values[record]=buffer.getvalue().encode();changed.append(record)
    with zipfile.ZipFile(target,'w') as after:
        for item in before.infolist():after.writestr(copy.copy(item),values[item.filename])
    with zipfile.ZipFile(target) as after:
        assert before.namelist()==after.namelist()
        actual_changes=[name for name in before.namelist() if before.read(name)!=after.read(name)]
        assert set(actual_changes)==set(changed)
(out/'stub-only-wheel-proof.json').write_text(json.dumps(dict(native_source=manifest['source'],
    initial_wheel_sha256=hashlib.sha256(initial.read_bytes()).hexdigest(),
    final_wheel_sha256=hashlib.sha256(target.read_bytes()).hexdigest(),changed_members=changed,
    unchanged_compiler_inputs=434,binary_rebuilt=False,binary_relabelled=False),indent=2)+'\n')
print('Updated two generated stubs and RECORD only; all native/font/notice bytes unchanged',flush=True)
