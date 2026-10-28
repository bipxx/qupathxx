from pathlib import Path
import json
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
cases=ET.parse(root/'build/rgb-nominal-287/reference-tests.xml').findall('.//testcase')
names=sorted(v.get('name') for v in cases)
assert len(names)==len(set(names))==12
path=work/'tools/ci/scientific-grid-cases.json'
data=json.loads(path.read_text())
assert 'test_rgb_nominal_reference' not in data
data['test_rgb_nominal_reference']=names
path.write_text(json.dumps(data,indent=2,sort_keys=True)+'\n',encoding='utf-8')
with (work/'.gitattributes').open('a',encoding='utf-8') as stream:
    for prefix in ('tests/fixtures/qupath-rgb-resampling-holdout-0.7.0','tests/fixtures/rgb-nominal-v1'):
        stream.write('\n'+prefix+'/** -text -whitespace\n')
    stream.write('tools/reference_rgb_nominal.py text eol=lf\ntests/test_rgb_nominal_reference.py text eol=lf\n')
print('Registered12 independent reference checks and immutable held-out/native golden fixtures')
