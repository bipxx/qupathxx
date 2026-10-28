from pathlib import Path
import json
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-sdk-289'
cases=ET.parse(out/'preview.xml').findall('.//testcase')
assert len(cases)==3 and all(all(c.find(t) is None for t in ('failure','error','skipped')) for c in cases)
path=work/'tools/ci/scientific-grid-cases.json'
value=json.loads(path.read_text())
assert 'test_rgb_nominal_preview' not in value
value['test_rgb_nominal_preview']=sorted(c.get('name') for c in cases)
path.write_text(json.dumps(value,indent=2,sort_keys=True)+'\n',encoding='utf-8')
with (work/'.gitattributes').open('a',encoding='utf-8') as stream:
    stream.write('examples/rgb_nominal_preview.py text eol=lf\ntests/test_rgb_nominal_preview.py text eol=lf\n')
print('Registered actual native ImGui preview with nominal calibration and independent document lifetime')
