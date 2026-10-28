from pathlib import Path
import json
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
cases=ET.parse(root/'build/qupath-rgb-resampling-286/installed2.xml').findall('.//testcase')
names=sorted(c.get('name') for c in cases if c.get('classname','').rsplit('.',1)[-1]=='test_qupath_rgb_resampling')
assert len(names)==len(set(names))==82
path=work/'tools/ci/scientific-grid-cases.json'
value=json.loads(path.read_text())
assert 'test_qupath_rgb_resampling' not in value
value['test_qupath_rgb_resampling']=names
path.write_text(json.dumps(value,indent=2,sort_keys=True)+'\n',encoding='utf-8')
with (work/'.gitattributes').open('a',encoding='utf-8') as stream:
    stream.write('\ntests/fixtures/qupath-rgb-resampling-0.7.0/** -text -whitespace\n')
    for name in ('tools/qupath_rgb_resampling.py','tools/upstream/qupath_rgb_resampling.groovy','tests/test_qupath_rgb_resampling.py'):
        stream.write(name+' text eol=lf\n')
repeat=root/'build/qupath-rgb-resampling-286/capture3'
original=work/'tests/fixtures/qupath-rgb-resampling-0.7.0'
payloads=[p for p in original.rglob('*') if p.is_file()]
assert len(payloads)==43
for p in payloads:
    assert p.read_bytes()==(repeat/p.relative_to(original)).read_bytes(),p
(root/'build/qupath-rgb-resampling-286/repeat-proof.json').write_text(json.dumps(dict(
    byte_identical_payloads_including_manifest=43,actual_captures=2,
    runtime_logs_excluded=True,registered_unique_passing_cases=82),indent=2)+'\n')
print('Registered82 exact cases;43 repeated payload/manifest bytes match')
