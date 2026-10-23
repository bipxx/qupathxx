"""Artifact acceptance rejects incomplete runs, corruption and mixed identities."""
import hashlib
import importlib.util
import json
from pathlib import Path
import zipfile

import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qpx_manylinux_acceptance',ROOT/'tools/ci/accept_manylinux.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
COMMIT='a'*40


def run():
    return dict(headSha=COMMIT,url='https://github.com/bipxx/qupathxx/actions/runs/123',status='completed',conclusion='success',
        jobs=[dict(name=f'{role} Python {version}',status='completed',conclusion='success')
              for role in ['Manylinux build','Debian consumer'] for version in ['3.11','3.12']])


@pytest.mark.parametrize('change',['pending','wrong_commit','missing_consumer','failed_consumer','extra_failure'])
def test_incomplete_or_mixed_run_is_not_accepted(change):
    metadata=run()
    if change=='pending':metadata['status']='in_progress'
    elif change=='wrong_commit':metadata['headSha']='b'*40
    elif change=='missing_consumer':metadata['jobs'].pop()
    elif change=='failed_consumer':metadata['jobs'][-1]['conclusion']='failure'
    else:metadata['jobs'].append(dict(name='extra',status='completed',conclusion='failure'))
    with pytest.raises(ValueError):module.successful_run(metadata,COMMIT)


def write(folder,name,data):
    destination=folder/name;destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(json.dumps(data),encoding='utf-8')


def junit(path,count):
    path.write_text('<testsuite>'+''.join(f'<testcase name="case{i}"/>' for i in range(count))+'</testsuite>',encoding='utf-8')


def cohort(folder, version):
    assets=module.read(module.ASSETS);policy=assets['policy'];tag='cp'+version.replace('.','')
    build=folder/('manylinux-'+version);consumer=folder/('manylinux-consumer-'+version)
    build.mkdir();consumer.mkdir();(build/'qualified').mkdir()
    wheel_name=f'qupathxx-0.1.0-{tag}-{tag}-{policy}.whl'
    wheel=build/'qualified'/wheel_name
    # This unit fixture exercises evidence binding. The real qualification tool
    # parses ELF architecture/versions; these bytes are deliberately not real ELF.
    names=['_native.fake.so','qupathxx_region','qupathxx_batch','libgeos_c.so.1','libgeos.so.3.15.0','libopenslide.so.1']
    payloads={'qupathxx/'+name:b'\x7fELF'+name.encode() for name in names}
    for notice in ['geos/COPYING','openslide-sdk/OpenSlide/COPYING.LESSER','opencv/LICENSE','libtiff/LICENSE.md']:
        payloads['qupathxx/licenses/'+notice]=b'notice '*30
    with zipfile.ZipFile(wheel,'w') as archive:
        for name,data in payloads.items():archive.writestr(name,data)
        archive.writestr('qupathxx-0.1.0.dist-info/WHEEL','Tag: '+tag+'-'+tag+'-'+policy+'\n')
    hashes={name:hashlib.sha256(data).hexdigest() for name,data in payloads.items()}
    qualification=dict(schema='qpx.manylinux-qualification/1',qualified=True,engine_commit=COMMIT,policy=policy,
        abi_errors={},runtime_payloads_unchanged=True,tools={key:assets[key] for key in ['auditwheel','patchelf','packaging','pyelftools']},
        python_tags=[tag],qualified_wheel=wheel_name,qualified_wheel_sha256=module.sha(wheel),input_wheel_sha256='f'*64,
        runtime_payload_sha256=hashes,elf_payloads={name:dict(sha256=hashes[name]) for name in hashes if name in ['qupathxx/'+n for n in names]})
    write(build,'qualification.json',qualification)
    write(build,'build-image.json',dict(image=assets['builder']['image'],libc='glibc 2.28',engine_commit=COMMIT,python=version+'.9'))
    identity=dict(source_tree_sha256='c'*64,native_sha256=hashes['qupathxx/_native.fake.so'])
    for name in ['installed-workflow.json','qualified-installed.json']:
        write(build,name,dict(**identity,gui=False,geos=True,openslide=True))
    runtimes={name:hashes['qupathxx/'+name] for name in names if name.startswith('lib')}
    write(build,'qualified-runtime.json',dict(**identity,schema='qpx.linux-runtime/1',runtimes=runtimes,
        mapped_from_package=True,geos_license_matches_sdk=True,relocated_python_geometry=True,
        relocated_region_pixels=True,relocated_batch=True,ld_library_path_used=False))
    write(build,'result.json',dict(mode='headless',wheel_sha256='f'*64))
    fixture=module.read(ROOT/'tests/fixtures/manifest.json')['downloads'][0]
    write(consumer,'consumer-probe.json',dict(**identity,schema='qpx.manylinux-consumer/1',engine_commit=COMMIT,
        policy=policy,wheel_sha256=module.sha(wheel),python=version+'.9',libc=['glibc','2.36'],
        mapped_runtimes=list(runtimes),loader_overrides_used=False,geos_union_area=150,slide_sha256=fixture['sha256'],
        unicode_annotation_reopened=True,retained_analysis_reopened=True,measurement_csv=True,
        native_region_exact=True,native_batch_checkpoint=True,native_batch_csv=True))
    junit(build/'native-tests.xml',40);junit(build/'python-tests.xml',1500);junit(consumer/'consumer-tests.xml',1500)
    (build/'stubs.log').write_text('Public stubs match',encoding='utf-8')
    (build/'typing.log').write_text('Success: no issues found in 3 source files',encoding='utf-8')
    (build/'typing-negative.log').write_text('\n'.join(f'invalid_api.py:{i}: error: fixture' for i in [6,7,8,9,11,13,14]),encoding='utf-8')
    return build,consumer


def test_complete_synthetic_cohort_binds_both_abis_but_never_accepts_release(tmp_path):
    for version in ['3.11','3.12']:cohort(tmp_path,version)
    report=module.audit(tmp_path,run(),COMMIT)
    assert report['state']=='build-and-fresh-consumer-accepted' and report['release_ready'] is False
    assert [v['python'] for v in report['versions']]==['3.11','3.12']


@pytest.mark.parametrize('change',['unqualified','abi_errors','wrong_tool','wheel_corruption','native_mismatch',
                                  'wrong_fixture','loader_override','failed_workflow','failed_test','missing_typing'])
def test_candidate_corruption_and_incomplete_workflows_are_rejected(tmp_path, change):
    build,consumer=cohort(tmp_path,'3.11')
    if change in ['unqualified','abi_errors','wrong_tool']:
        record=module.read(build/'qualification.json')
        if change=='unqualified':record['qualified']=False
        elif change=='abi_errors':record['abi_errors']={'native':['GLIBC too new']}
        else:record['tools']['auditwheel']='wrong'
        write(build,'qualification.json',record)
    elif change=='wheel_corruption':
        with next((build/'qualified').glob('*.whl')).open('ab') as stream:stream.write(b'changed')
    elif change in ['native_mismatch','wrong_fixture','loader_override','failed_workflow']:
        record=module.read(consumer/'consumer-probe.json')
        if change=='native_mismatch':record['native_sha256']='0'*64
        elif change=='wrong_fixture':record['slide_sha256']='0'*64
        elif change=='loader_override':record['loader_overrides_used']=True
        else:record['native_batch_csv']=False
        write(consumer,'consumer-probe.json',record)
    elif change=='failed_test':
        path=consumer/'consumer-tests.xml'
        path.write_text(path.read_text(encoding='utf-8').replace('<testcase name="case0"/>','<testcase name="case0"><failure/></testcase>'),encoding='utf-8')
    else:(build/'typing.log').unlink()
    with pytest.raises((ValueError,FileNotFoundError)):
        module.audit_version(tmp_path,'3.11',COMMIT,module.read(module.ASSETS))


def test_scientific_expected_failure_does_not_turn_into_release_acceptance(tmp_path):
    path=tmp_path/'tests.xml'
    path.write_text('<testsuite><testcase name="ok"/><testcase classname="test_qupath_cells_reference" '
        'name="test_isolated_cell_upstream_acceptance_gate[baseline_count]"><skipped type="pytest.xfail"/></testcase></testsuite>',encoding='utf-8')
    assert module.junit(path,1)['expected_scientific_failures']==1
    path.write_text(path.read_text(encoding='utf-8').replace('test_qupath_cells_reference','test_unknown'),encoding='utf-8')
    with pytest.raises(ValueError,match='unreviewed expected failure'):module.junit(path,1)
