from pathlib import Path
import hashlib,json,shutil,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2]
dest=r/'build/rawstreamfile224-initial-products';dest.mkdir(exist_ok=True)
targets=dict(pixel_features='pixel_feature',pixel_workflows='pixel_workflow',resampling='resampling',raw_image='raw_image',source_identity='source_identity',project='project',project_save='save',provenance='provenance',classifier='classifier')
records=[]
for host in ('windows','linux','training'):
    folder=r/f'build/rawstreamfile224-{host}'
    commands=json.loads((folder/'commands.json').read_text())
    assert [c['exit'] for c in commands]==[0,0,0]
    cases=list(ET.parse(folder/'tests.xml').iter('testcase'))
    expected={'pixel_features','pixel_workflows','classifier'} if host=='training' else set(targets)-{'classifier'}
    assert len(cases)==len(expected) and {c.attrib['name'] for c in cases}==expected
    assert {c.attrib['name'] for c in cases if c.find('failure') is not None}==set()
    assert all(c.find(tag) is None for c in cases for tag in ('error','skipped'))
    manifest=json.loads((folder/'compiler-inputs.json').read_text())
    cache=r/({'windows':'build/ci/gridfrontends167-package','linux':'build/ci/raw176-tiff-linux-native','training':'build/ci/rawtraining218-windows'}[host])
    out=dest/host;out.mkdir(exist_ok=True)
    for name in ('generated/build_manifest.hpp','CMakeCache.txt'):
        shutil.copy2(cache/name,out/Path(name).name)
    assert manifest['source'] in (out/'build_manifest.hpp').read_text()
    for case in cases:
        name=case.attrib['name'];p=cache/('' if host=='linux' else 'Release')/('qupathxx_'+targets[name]+'_tests'+('' if host=='linux' else '.exe'))
        target=out/p.name;shutil.copy2(p,target);assert target.read_bytes()==p.read_bytes()
        records.append(dict(host=host,suite=name,passed=case.find('failure') is None,source=manifest['source'],fingerprint=manifest['fingerprint'],path=target.relative_to(r).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
(dest/'products.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
print('Preserved 19 passing initial products and cache headers before conditional-commit hardening.')
