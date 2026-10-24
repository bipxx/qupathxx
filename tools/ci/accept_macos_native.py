"""Audit exact-source macOS native CI records; no Python or package acceptance."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


def require(value, message):
    if not value:
        raise ValueError(message)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audit(artifacts, run, commit, sampled_cells=False, small_frontier=False, tma_cases=False, file_export=False, cohort=False):
    file_export=file_export or cohort
    tma_cases=tma_cases or file_export
    small_frontier=small_frontier or tma_cases
    sampled_cells=sampled_cells or small_frontier
    if small_frontier:
        spec=importlib.util.spec_from_file_location('qpx_frontier_evidence',Path(__file__).with_name('frontier_evidence.py'))
        frontier=importlib.util.module_from_spec(spec);spec.loader.exec_module(frontier)
    if tma_cases:
        spec=importlib.util.spec_from_file_location('qpx_tma_case_evidence',Path(__file__).with_name('tma_case_evidence.py'))
        tma=importlib.util.module_from_spec(spec);spec.loader.exec_module(tma)
    require(run['headSha']==commit and run['status']=='completed' and run['conclusion']=='success',
            'run source or completion status differs')
    jobs=[j for j in run['jobs'] if j['conclusion']!='skipped']
    require({j['name'] for j in jobs}=={'macOS native arm64','macOS native x86_64'} and len(jobs)==2,
            'both native architectures are required')
    require(all(j['status']=='completed' and j['conclusion']=='success' for j in jobs),'native job failed')
    entries=[]
    for arch in ['arm64','x86_64']:
        folder=Path(artifacts)/('macos-native-'+arch)
        data=json.loads((folder/'result.json').read_text())
        require(data['commit']==commit and data['architecture']==arch,'native record source/architecture differs')
        xml=ET.parse(folder/'native-tests.xml').getroot()
        cases=list(xml.iter('testcase'))
        require(len(cases)==data['tests']['passed']==46,'native suite count differs')
        require(not any(t.find(k) is not None for t in cases for k in ['failure','error','skipped']),
                'native test failed or skipped')
        require(sorted(t.get('name') for t in cases)==sorted(data['tests']['names']),'native suite ledger differs')
        require(sha(folder/'native-tests.xml')==data['tests']['junit_sha256'],'native JUnit digest differs')
        markers=[('log_watershed','Sampled shape 26 frozen masks and maxima passed')]
        if sampled_cells:
            markers.extend([
                ('log_cell_recipe','Sampled LoG QCL4, explicit seed preset, typed recipe identities, replay and QCL1/2/3 preservation passed'),
                ('log_cell_recipe','Sampled clipped components without in-ROI markers retain measurements and replay labels')])
        for name,marker in markers:
            selected=[t for t in cases if t.get('name')==name]
            require(len(selected)==1 and marker in selected[0].findtext('system-out',''),
                    'missing native conformance marker: '+marker)
        require('Mach-O 64-bit executable '+arch in (folder/'binary-architecture.log').read_text(),
                'native architecture log differs')
        require(data['region_cli_exact'] and not data['loader_overrides_used'] and
                not data['python_gui_openslide_validated'] and not data['release_ready'],'native CLI/scope record differs')
        flags=data['flags']
        require(all(flags['QUPATHXX_BUILD_'+name]=='OFF' for name in ['PYTHON','GUI','OPENSLIDE']),
                'unexpected build scope')
        require(all(flags['QUPATHXX_BUILD_'+name]=='ON' for name in
                    ['GEOS','TIFF','PROJECT','INTERCHANGE','CLASSIFIER_TRAINING','TESTS']),
                'required native feature disabled')
        require(flags['CMAKE_OSX_ARCHITECTURES']==arch,'CMake architecture differs')
        require(set(data['dependencies'])=={'geos','opencv'},'dependency ledger differs')
        require(all(len(v)==64 and set(v)<=set('0123456789abcdef')
                    for dependency in data['dependencies'].values() for v in dependency.values()),
                'invalid dependency digest')
        require(len(data['native_batch_sha256'])==64 and set(data['native_batch_sha256'])<=set('0123456789abcdef'),
                'invalid executable digest')
        links=(folder/'batch-runtime-links.log').read_text()
        require('@rpath/libgeos_c.' in links and 'libSystem.B.dylib' in links,'native runtime link record differs')
        entries.append(dict(architecture=arch,native_passed=46,frozen_shape_masks_and_maxima=26,
            sampled_production_native=sampled_cells,
            evidence_sha256={p.name:sha(p) for p in sorted(folder.iterdir()) if p.is_file()},ci_result=data))
        if small_frontier:entries[-1]['small_frontier']=frontier.native(cases)
        if tma_cases:entries[-1]['tma_cases']=tma.native(cases,file_export=file_export,cohort=cohort)
    return dict(schema='qpx.macos-native-audit/1',source=commit,run=run['url'],jobs=entries,
        auditor_sha256=sha(Path(__file__)),native_engineering_accepted=True,
        production_sampled_native_required=sampled_cells,python_gui_openslide_validated=False,
        small_frontier_required=small_frontier,
        frontier_helper_sha256=sha(Path(__file__).with_name('frontier_evidence.py')) if small_frontier else None,
        tma_cohorts_required=cohort,tma_file_export_required=file_export,tma_cases_required=tma_cases,tma_case_helper_sha256=sha(Path(__file__).with_name('tma_case_evidence.py')) if tma_cases else None,
        portable_package_validated=False,performance_accepted=False,release_ready=False,
        scope='Exact-source CI native tests, frozen shape conformance, architecture and region CLI records. '
              'SDK and executable bytes were not uploaded; dependency and executable digests are CI-generated evidence. '
              'No relocated package, Python, OpenSlide, GUI, signing, biological or supported-release acceptance.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run')
    parser.add_argument('commit')
    parser.add_argument('--artifacts',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--sampled-cells',action='store_true')
    parser.add_argument('--small-frontier',action='store_true')
    parser.add_argument('--tma-cases',action='store_true')
    parser.add_argument('--tma-file-export',action='store_true')
    parser.add_argument('--tma-cohorts',action='store_true')
    args=parser.parse_args()
    run=json.loads(subprocess.check_output(['gh','run','view',args.run,'--repo','bipxx/qupathxx',
        '--json','headSha,url,status,conclusion,jobs']))
    report=audit(args.artifacts,run,args.commit,args.sampled_cells,args.small_frontier,args.tma_cases,args.tma_file_export,args.tma_cohorts)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_bytes((json.dumps(report,indent=2)+'\n').encode())
    print(json.dumps(dict(native_engineering_accepted=True,source=args.commit,
                         production_sampled_native_required=report['production_sampled_native_required'],release_ready=False)))


if __name__=='__main__':
    main()
