"""Audit downloaded builder/consumer evidence; never infer release readiness.

Download a completed run with gh run download RUN --dir ARTIFACTS, and save
gh run view RUN --json headSha,url,status,conclusion,jobs as --run-json.
This audits a trusted CI run's evidence, not signatures or untrusted artifacts.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ASSETS = Path(__file__).with_name('manylinux-assets.json')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def junit(path, minimum):
    root = ET.parse(path).getroot()
    cases = list(root.iter('testcase'))
    require(len(cases)>=minimum,'test evidence is too small: '+str(path))
    require(not any(t.find('failure') is not None or t.find('error') is not None for t in cases),
            'failed test evidence: '+str(path))
    skipped = [t for t in cases if t.find('skipped') is not None]
    xfailed = [t for t in skipped if t.find('skipped').get('type')=='pytest.xfail']
    for case in xfailed:
        require(case.get('classname','').endswith('test_qupath_cells_reference') and
                case.get('name','').split('[')[0] in ['test_isolated_cell_upstream_acceptance_gate',
                                                     'test_resampled_cell_shared_roi_acceptance_gate'],
                'unreviewed expected failure: '+case.get('name',''))
    passed = len(cases)-len(skipped)
    require(passed>=minimum,'too few passing tests: '+str(path))
    return dict(tests=len(cases),passed=passed,skipped=len(skipped)-len(xfailed),
                expected_scientific_failures=len(xfailed),sha256=sha(path),
                skip_reasons=dict(Counter(t.find('skipped').get('message','') for t in skipped)))


def successful_run(run, commit):
    require(re.fullmatch('[0-9a-f]{40}',commit) is not None,'expected a full source commit')
    require(run['headSha']==commit and run['status']=='completed' and run['conclusion']=='success',
            'run is not successful at the requested commit')
    jobs = {j['name']:j for j in run['jobs']}
    expected = {f'{role} Python {version}' for role in ['Manylinux build','Debian consumer']
                for version in ['3.11','3.12']}
    require(expected.issubset(jobs),'missing builder or consumer job')
    require(all(jobs[name]['status']=='completed' and jobs[name]['conclusion']=='success' for name in expected),
            'builder or consumer job did not pass')
    require(all(j['conclusion'] in ['success','skipped'] for j in run['jobs']),
            'run contains a failed or unfinished job')


def audit_version(folder, version, commit, assets):
    build = folder/('manylinux-'+version)
    consumer = folder/('manylinux-consumer-'+version)
    qualification = read(build/'qualification.json')
    require(qualification['schema']=='qpx.manylinux-qualification/1' and qualification['qualified'] is True,
            'wheel did not qualify')
    require(qualification['engine_commit']==commit and qualification['policy']==assets['policy'],
            'qualification source or policy differs')
    require(qualification['abi_errors']=={} and qualification['runtime_payloads_unchanged'] is True,
            'ABI errors or modified runtime payloads')
    require(qualification['tools']=={k:assets[k] for k in ['auditwheel','patchelf','packaging','pyelftools']},
            'qualification tool versions differ')
    tag = 'cp'+version.replace('.','')
    require(qualification['python_tags']==[tag],'qualification Python ABI differs')
    wheel_name = qualification['qualified_wheel']
    require(PurePosixPath(wheel_name).name==wheel_name and '\\' not in wheel_name,'invalid wheel filename')
    require(wheel_name.endswith('-'+tag+'-'+tag+'-'+assets['policy']+'.whl'),'wheel tag differs')
    wheel = build/'qualified'/wheel_name
    require(sha(wheel)==qualification['qualified_wheel_sha256'],'qualified wheel hash differs')
    with zipfile.ZipFile(wheel) as archive:
        names=archive.namelist()
        require(len(names)==len(set(names)),'duplicate wheel entries')
        payloads={n:hashlib.sha256(archive.read(n)).hexdigest() for n in names
                  if not n.endswith('/') and '.dist-info/' not in n}
        require(payloads==qualification['runtime_payload_sha256'],'runtime/notices payload hashes differ')
        require(all(name in payloads and payloads[name]==entry['sha256']
                    for name,entry in qualification['elf_payloads'].items()),'ELF manifest differs')
        actual_elf={name for name in payloads if archive.read(name).startswith(b'\x7fELF')}
        require(actual_elf==set(qualification['elf_payloads']),'ELF manifest omits payloads')
        required={'qupathxx/qupathxx_region','qupathxx/qupathxx_batch','qupathxx/libgeos_c.so.1','qupathxx/libopenslide.so.1'}
        native=[name for name in actual_elf if name.startswith('qupathxx/_native.')]
        require(required.issubset(actual_elf) and len(native)==1 and
                any(PurePosixPath(n).name.startswith('libgeos.so.') for n in actual_elf),'full native payload missing')
        metadata=[n for n in names if n.endswith('.dist-info/WHEEL')]
        require(len(metadata)==1,'wheel metadata missing or ambiguous')
        tags=[line[5:] for line in archive.read(metadata[0]).decode().splitlines() if line.startswith('Tag: ')]
        require(tags==[tag+'-'+tag+'-'+assets['policy']],'advertised wheel tag differs')
        for notice in ['geos/COPYING','openslide-sdk/OpenSlide/COPYING.LESSER','opencv/LICENSE','libtiff/LICENSE.md']:
            require(len(archive.read('qupathxx/licenses/'+notice))>100,'missing runtime notice')
    image=read(build/'build-image.json')
    require(image['image']==assets['builder']['image'] and image['libc']=='glibc 2.28' and
            image['engine_commit']==commit and image['python'].startswith(version+'.'),'build image or interpreter differs')
    installed=read(build/'qualified-installed.json')
    original=read(build/'installed-workflow.json')
    runtime=read(build/'qualified-runtime.json')
    probe=read(consumer/'consumer-probe.json')
    result=read(build/'result.json')
    require(result['mode']=='headless' and result['wheel_sha256']==qualification['input_wheel_sha256'],
            'original tested wheel identity differs')
    require(runtime['schema']=='qpx.linux-runtime/1','unexpected relocated runtime schema')
    require(probe['schema']=='qpx.manylinux-consumer/1' and probe['engine_commit']==commit and
            probe['policy']==assets['policy'] and probe['wheel_sha256']==sha(wheel),'consumer artifact identity differs')
    source=installed['source_tree_sha256']
    require(re.fullmatch('[0-9a-f]{64}',source) is not None,'invalid source fingerprint')
    for report in [original,installed,runtime,probe]:
        require(report['source_tree_sha256']==source and report['native_sha256']==payloads[native[0]],
                'installed/relocated/consumer native identity differs')
    for report in [original,installed]:
        require(report['gui'] is False and report['geos'] is True and report['openslide'] is True,
                'installed workflow capabilities differ')
    require(probe['python'].startswith(version+'.') and probe['libc']==['glibc','2.36'],
            'fresh Debian interpreter or libc differs from pinned consumer')
    for key in ['mapped_from_package','geos_license_matches_sdk','relocated_python_geometry',
                'relocated_region_pixels','relocated_batch']:
        require(runtime[key] is True,'failed relocated runtime check: '+key)
    require(runtime['ld_library_path_used'] is False and probe['loader_overrides_used'] is False,
            'loader overrides used')
    for name,digest in runtime['runtimes'].items():
        require(payloads.get('qupathxx/'+name)==digest,'runtime mapping identity differs')
    require(set(runtime['runtimes'])==set(probe['mapped_runtimes']) and len(probe['mapped_runtimes'])==3,
            'consumer runtime mappings differ')
    require(probe['geos_union_area']==150,'consumer geometry reference differs')
    for key in ['unicode_annotation_reopened','retained_analysis_reopened','measurement_csv',
                'native_region_exact','native_batch_checkpoint','native_batch_csv']:
        require(probe[key] is True,'consumer workflow failed: '+key)
    root=Path(__file__).resolve().parents[2]
    fixtures=read(root/'tests/fixtures/manifest.json')
    fixture=next(f for f in fixtures['downloads'] if f['filename']=='CMU-1-Small-Region.svs')
    require(probe['slide_sha256']==fixture['sha256'],'consumer fixture hash differs')
    require('Public stubs match' in (build/'stubs.log').read_text(encoding='utf-8'),'stub check missing')
    require('Success: no issues found in 3 source files' in (build/'typing.log').read_text(encoding='utf-8'),
            'positive typing check missing')
    negative=(build/'typing-negative.log').read_text(encoding='utf-8')
    require(sorted(map(int,re.findall(r'invalid_api.py:(\d+): error:',negative)))==[6,7,8,9,11,13,14],
            'negative typing checks differ')
    tests=dict(native=junit(build/'native-tests.xml',40),builder=junit(build/'python-tests.xml',1500),
               consumer=junit(consumer/'consumer-tests.xml',1500))
    require(tests['native']['skipped']==tests['native']['expected_scientific_failures']==0,'native tests skipped')
    return dict(python=version,source_tree_sha256=source,wheel=wheel_name,wheel_sha256=sha(wheel),
        native_sha256=payloads[native[0]],runtimes=runtime['runtimes'],fixture_sha256=probe['slide_sha256'],
        builder_image=image['image'],consumer_image=assets['consumers'][version]['image'],tests=tests,
        evidence={str(path.relative_to(folder)):sha(path) for base in [build,consumer]
                  for path in sorted(base.glob('*')) if path.is_file() and path.suffix in ['.json','.log','.xml']},
        runtime_payloads_unchanged=True,fresh_consumer_workflows=True)


def audit(folder, run, commit):
    successful_run(run,commit)
    assets=read(ASSETS)
    versions=[audit_version(Path(folder),version,commit,assets) for version in ['3.11','3.12']]
    require(versions[0]['source_tree_sha256']==versions[1]['source_tree_sha256'],'builder source trees differ')
    require(versions[0]['fixture_sha256']==versions[1]['fixture_sha256'],'consumer fixtures differ')
    return dict(schema='qpx.manylinux-acceptance/1',commit=commit,run=run['url'],policy=assets['policy'],
        state='build-and-fresh-consumer-accepted',release_ready=False,versions=versions,
        assets_sha256=sha(ASSETS),auditor_sha256=sha(__file__),
        scope='Declared headless CPython ABI and fresh Debian consumer workflows only. Project license, supported release, GUI/macOS, scientific and hardware acceptance remain separate gates.')


def unified_cohorts(folder):
    """Require the LoG/parser/cache cohorts in builders and fresh consumers."""
    report=[]
    desktop={'test_log_gui_and_copied_recipe','test_log_managed_viewer_captures_detector_recipe','test_log_gui_batch_captures_recipe_and_checkpoint'}
    contract=read(Path(__file__).resolve().parents[2]/'tests/fixtures/qupath-cells-0.7.0/contract.json')
    reference={f"test_log_installed_nucleus_reference[{tile}-{case['name']}]" for tile in contract['tile_sizes'] for case in contract['cases']}
    for version in ['3.11','3.12']:
        build=Path(folder)/('manylinux-'+version)
        native=ET.parse(build/'native-tests.xml').getroot().findall('.//testcase')
        require({'decimal_quantities','log_watershed','log_cell_recipe','geometry'}<={t.get('name') for t in native},'unified native suites missing')
        for cohort,path in [('builder',build/'python-tests.xml'),('consumer',Path(folder)/('manylinux-consumer-'+version)/'consumer-tests.xml')]:
            cases=ET.parse(path).getroot().findall('.//testcase')
            log=[t for t in cases if t.get('classname','').endswith('test_log_cells')]
            require(len(log)==50,'unified LoG cohort must contain 50 cases')
            require(reference<={t.get('name') for t in log},'pinned LoG reference/tile cases missing')
            require(all(t.find('failure') is None and t.find('error') is None for t in log),'LoG case failed')
            skipped=[t for t in log if t.find('skipped') is not None]
            require({t.get('name') for t in skipped}==desktop,'unexpected skipped LoG cases')
            require(all(t.find('skipped').get('type')!='pytest.xfail' for t in skipped),'LoG case expected to fail')
            cache=[t for t in cases if t.get('classname','').endswith('test_sampling_acceleration')]
            decimals=[t for t in cases if t.get('classname','').endswith('test_tiff_reader') and t.get('name','').startswith('test_ome_decimal_quantity_contract[')]
            require(len(cache)==9 and len(decimals)==12,'unified cache/decimal cases missing')
            require(all(all(t.find(k) is None for k in ['failure','error','skipped']) for t in cache+decimals),'cache/decimal case did not pass')
            report.append(dict(python=version,cohort=cohort,log_passed=47,log_desktop_skipped=3,
                log_cases=[t.get('name') for t in log],cache_cases=[t.get('name') for t in cache],decimal_cases=[t.get('name') for t in decimals],junit_sha256=sha(path)))
    return report


def ndpi_cohorts(folder):
    """Real NDPI runs in builders; fresh consumers exercise native link capture."""
    report=[]
    expected={'test_ndpi_metadata_and_anisotropic_calibration','test_ndpi_complete_overview_and_source_budget',
        'test_ndpi_scheduler_cache_and_pending_close','test_ndpi_log_detection_tile_invariance_and_saved_replay',
        'test_ndpi_truncated_container_rejects','test_ndpi_desktop_pyramid_settles'}
    expected.update(f'test_ndpi_all_levels_padding_and_owned_pixels[{i}]' for i in range(9))
    expected.update(f'test_ndpi_installed_region_cli[{i}]' for i in range(9))
    for version in ['3.11','3.12']:
        build=Path(folder)/('manylinux-'+version)
        native=ET.parse(build/'native-tests.xml').getroot().findall('.//testcase')
        project=[t for t in native if t.get('name')=='project']
        require(len(project)==1 and 'Native NDPI inferred link, LoG archive replay and calibration passed' in project[0].findtext('system-out',''),'native NDPI project replay was not exercised')
        for cohort,path in [('builder',build/'python-tests.xml'),('consumer',Path(folder)/('manylinux-consumer-'+version)/'consumer-tests.xml')]:
            cases=ET.parse(path).getroot().findall('.//testcase')
            readers=[t for t in cases if t.get('classname','').endswith('test_ndpi_reader')]
            require(len(readers)==24 and {t.get('name') for t in readers}==expected,'NDPI cohort differs')
            require(all(t.find('failure') is None and t.find('error') is None for t in readers),'NDPI case failed')
            skips=[t for t in readers if t.find('skipped') is not None]
            require({t.get('name') for t in skips}==({'test_ndpi_desktop_pyramid_settles'} if cohort=='builder' else expected),'NDPI fixture/desktop scope differs')
            require(all(t.find('skipped').get('type')!='pytest.xfail' for t in skips),'NDPI case expected to fail')
            links=[t for t in cases if t.get('name') in {'test_opened_slide_document_defaults_to_its_native_link','test_opened_tiff_document_defaults_to_native_link_and_current_view'}]
            require(len(links)==2 and all(all(t.find(k) is None for k in ['failure','error','skipped']) for t in links),'default native-link regressions missing')
            report.append(dict(python=version,cohort=cohort,ndpi_passed=24-len(skips),ndpi_skipped=len(skips),
                default_link_passed=2,ndpi_cases=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in readers],junit_sha256=sha(path)))
    return report


def watershed_body_cohorts(folder):
    """Require all captured body recipes in builders and fresh consumers."""
    contract=read(Path(__file__).resolve().parents[2]/'tests/fixtures/qupath-cells-0.7.0/contract.json')
    desktop={'test_body_gui_captured_recipe_and_published_pixels',
             'test_body_managed_viewer_copies_recipe_and_reopens_retained_result',
             'test_body_gui_batch_keeps_distinct_recipe'}
    expected={f"test_production_body_mode_pinned_labels_and_measurements[{tile}-{case['name']}]"
              for tile in contract['tile_sizes'] for case in contract['cases']}
    expected.update(f'test_body_worker_rejects_stale_authority_and_cancellation[{change}]'
                    for change in ['selection','rename','close','authority','cancel'])
    expected.update(f'test_body_mode_natural_level_identity_and_work_admission[{level}]' for level in range(3))
    expected.update(desktop|{'test_body_archive_replay_classifier_and_legacy_records',
                            'test_body_batch_mode_has_distinct_resume_and_cli_recipe',
                            'test_body_candidate_limit_failure_keeps_document_and_worker_reusable',
                            'test_body_expansion_respects_transparent_source_domain'})
    report=[]
    for version in ['3.11','3.12']:
        build=Path(folder)/('manylinux-'+version)
        native=list(ET.parse(build/'native-tests.xml').getroot().iter('testcase'))
        expansion=[t for t in native if t.get('name')=='cell_expansion']
        require(len(expansion)==1 and all(expansion[0].find(k) is None for k in ['failure','error','skipped']),
                'native cell expansion suite missing or did not pass')
        for cohort,path in [('builder',build/'python-tests.xml'),
                            ('consumer',Path(folder)/('manylinux-consumer-'+version)/'consumer-tests.xml')]:
            cases=[t for t in ET.parse(path).getroot().iter('testcase')
                   if t.get('classname','').endswith('test_log_body_cells')]
            require(len(cases)==42 and {t.get('name') for t in cases}==expected,
                    'production watershed-body cohort differs')
            require(all(t.find('failure') is None and t.find('error') is None for t in cases),
                    'production watershed-body case failed')
            skipped=[t for t in cases if t.find('skipped') is not None]
            require({t.get('name') for t in skipped}==desktop and
                    all(t.find('skipped').get('type')!='pytest.xfail' for t in skipped),
                    'unexpected skipped or expected-failing watershed-body cases')
            report.append(dict(python=version,cohort=cohort,passed=39,desktop_skipped=3,
                               cases=[t.get('name') for t in cases],junit_sha256=sha(path)))
    return report


def sampled_cell_cohorts(folder, commit):
    root=Path(__file__).resolve().parents[2]
    contract='tests/fixtures/imagej-shape-1.54p/sampled-cell-workflows.json'
    require((root/contract).read_bytes()==subprocess.check_output(['git','-C',str(root),'show',commit+':'+contract]),
            'sampled workflow contract differs from dispatched source')
    expected=read(root/contract)
    require(len(expected['cases'])==53 and len(expected['desktop_cases'])==3,'invalid sampled workflow contract')
    desktop=set(expected['desktop_cases'])
    real={name for name in expected['cases'] if name.startswith('test_sampled_real_slide_keeps_characterization_and_exact_saved_replay[')}
    require(len(real)==2,'missing real-slide sampled contexts')
    report=[]
    for version in ['3.11','3.12']:
        build=Path(folder)/('manylinux-'+version)
        native=list(ET.parse(build/'native-tests.xml').getroot().iter('testcase'))
        for name,marker in [('log_watershed','Sampled shape 26 frozen masks and maxima passed'),
                            ('log_cell_recipe','Sampled LoG QCL4, explicit seed preset, typed recipe identities, replay and QCL1/2/3 preservation passed'),
                            ('log_cell_recipe','Sampled clipped components without in-ROI markers retain measurements and replay labels')]:
            selected=[t for t in native if t.get('name')==name]
            require(len(selected)==1 and marker in selected[0].findtext('system-out',''),'sampled native recipe/conformance marker missing')
        for cohort,path in [('builder',build/'python-tests.xml'),
                            ('consumer',Path(folder)/('manylinux-consumer-'+version)/'consumer-tests.xml')]:
            cases=[t for t in ET.parse(path).getroot().iter('testcase')
                   if t.get('classname','').rsplit('.',1)[-1]==expected['classname']]
            require(sorted(t.get('name') for t in cases)==expected['cases'],'sampled production workflow cohort differs')
            require(all(t.find('failure') is None and t.find('error') is None for t in cases),'sampled production workflow failed')
            skipped=[t for t in cases if t.find('skipped') is not None]
            require({t.get('name') for t in skipped}==(desktop if cohort=='builder' else desktop|real),
                    'sampled fixture/desktop skip scope differs')
            require(all(t.find('skipped').get('type')!='pytest.xfail' for t in skipped),'sampled production case expected to fail')
            report.append(dict(python=version,cohort=cohort,passed=53-len(skipped),
                               desktop_skipped=3,real_slide_skipped=0 if cohort=='builder' else 2,
                               cases=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in cases],
                               cohort_contract_sha256=sha(root/contract),junit_sha256=sha(path)))
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts',type=Path,required=True)
    parser.add_argument('--run-json',type=Path,required=True)
    parser.add_argument('--commit',required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--unified-engine',action='store_true',help='Also require the explicit LoG, decimal parser and cache cohorts in all four Python runs')
    parser.add_argument('--ndpi-readers',action='store_true',help='Also require NDPI builder and default native-link cases; consumers retain their small-fixture scope')
    parser.add_argument('--watershed-bodies',action='store_true',help='Also require all production body recipes and native expansion in both builders/consumers')
    parser.add_argument('--sampled-cells',action='store_true',help='Require frozen shape/QCL4 native and sampled production builder/consumer cohorts; implies earlier engine/body/NDPI gates')
    args=parser.parse_args()
    if args.sampled_cells:
        args.unified_engine=args.ndpi_readers=args.watershed_bodies=True
    report=audit(args.artifacts,read(args.run_json),args.commit)
    if args.unified_engine or args.ndpi_readers or args.watershed_bodies:
        report['unified_engine_cohorts']=unified_cohorts(args.artifacts)
    if args.ndpi_readers:
        report['ndpi_reader_cohorts']=ndpi_cohorts(args.artifacts)
    if args.watershed_bodies:
        report['watershed_body_cohorts']=watershed_body_cohorts(args.artifacts)
    if args.sampled_cells:
        report['sampled_cell_cohorts']=sampled_cell_cohorts(args.artifacts,args.commit)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(state=report['state'],commit=report['commit'],release_ready=False)))


if __name__=='__main__':
    main()
