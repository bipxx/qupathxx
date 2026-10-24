"""Audit the exact-source unified native/installed Windows/Linux CI artifacts.

Download all five named artifacts into --artifacts. This accepts correctness
and engineering evidence only; warm/cold timings and release gates are separate.
"""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import math
import xml.etree.ElementTree as ET
import zipfile

ROOT=Path(__file__).resolve().parents[2]
if not __debug__:
    raise RuntimeError('artifact audit requires Python assertions enabled')
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('run');parser.add_argument('commit')
parser.add_argument('--artifacts',type=Path,default=ROOT/'build/ci/unified121-hosted')
parser.add_argument('--output',type=Path,default=ROOT/'build/unified-ci-windows-linux.json')
parser.add_argument('--ndpi-readers',action='store_true',help='Also require real nine-level NDPI and default native project-link regressions')
parser.add_argument('--rendered-loop',action='store_true',help='Require ordinary, startup and bounded long-session rendered-loop timing cases')
parser.add_argument('--gui-stack',action='store_true',help='Require repeated native project startup on the default Windows executable stack')
parser.add_argument('--watershed-bodies',action='store_true',help='Require strict-radius production body recipes, native expansion and pinned comparison')
parser.add_argument('--sampled-shape',action='store_true',help='Also require all 26 frozen native masks/maxima and 53 capture/cohort checks; implies watershed bodies')
parser.add_argument('--sampled-cells',action='store_true',help='Require QCL4 production recipe, 53 sampled workflow cases and independent comparison; implies sampled shape')
parser.add_argument('--small-frontier',action='store_true',help='Require 1344 native queue comparisons and eight strict unit-radius workflow cases; implies sampled cells')
parser.add_argument('--tma-cases',action='store_true',help='Require exact case aggregation, numeric regression and desktop export cohorts; implies small frontier')
parser.add_argument('--linux-only',action='store_true',help='Audit only the three successful Linux jobs; never accepts the full matrix')
args=parser.parse_args();RUN=args.run;COMMIT=args.commit
args.small_frontier=args.small_frontier or args.tma_cases
args.sampled_cells=args.sampled_cells or args.small_frontier
args.sampled_shape=args.sampled_shape or args.sampled_cells
args.watershed_bodies=args.watershed_bodies or args.sampled_shape
if args.small_frontier:
    spec=importlib.util.spec_from_file_location('qpx_frontier_evidence',Path(__file__).with_name('frontier_evidence.py'))
    frontier=importlib.util.module_from_spec(spec);spec.loader.exec_module(frontier)
if args.tma_cases:
    spec=importlib.util.spec_from_file_location('qpx_tma_case_evidence',Path(__file__).with_name('tma_case_evidence.py'))
    tma_cases=importlib.util.module_from_spec(spec);spec.loader.exec_module(tma_cases)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def junit(path):
    root=ET.parse(path).getroot()
    suites=[root] if root.tag=='testsuite' else root.findall('testsuite')
    result={k:sum(int(s.get(k,0)) for s in suites) for k in ['tests','failures','errors','skipped']}
    result['passed']=result['tests']-result['failures']-result['errors']-result['skipped']
    result['seconds']=sum(float(s.get('time',0)) for s in suites)
    result['sha256']=sha(path)
    skips=Counter(n.get('message','') for n in root.iter('skipped'))
    if skips:result['skip_reasons']=dict(sorted(skips.items()))
    assert result['failures']==result['errors']==0
    return result
run=json.loads(subprocess.check_output(['gh','run','view',RUN,'--repo','bipxx/qupathxx','--json','headSha,url,jobs,status,conclusion,createdAt,updatedAt']))
assert run['headSha']==COMMIT
if not args.linux_only:
    assert run['status']=='completed' and run['conclusion']=='success'
run['skipped_jobs']=[j['name'] for j in run['jobs'] if j['conclusion']=='skipped']
run['jobs']=[j for j in run['jobs'] if j['conclusion']!='skipped']
assert len(run['jobs'])==5
if args.linux_only:
    run['jobs']=[j for j in run['jobs'] if j['name'].startswith('Linux ')]
assert len(run['jobs'])==(3 if args.linux_only else 5)
assert all(j['status']=='completed' and j['conclusion']=='success' for j in run['jobs'])
folders={'Linux minimal native':'Linux-minimal-py3.12','Linux ASan UBSan native':'Linux-sanitize-py3.12','Linux headless Python 3.12 NumPy 2':'Linux-headless-py3.12',
         'Windows headless Python 3.11 NumPy 1':'Windows-headless-py3.11','Windows GUI Python 3.12 software GL':'Windows-gui-py3.12'}
report=dict(schema='qpx.ci-validation/1',commit=COMMIT,engine_commit=COMMIT,run=run['url'],
    started_utc=run['createdAt'],observed_updated_utc=run['updatedAt'],overall_status=run['status'],overall_conclusion=run['conclusion'],
    scope='Combined native LoG production mode, C++17 decimal calibration parser, Darwin type fix and lazy conservative polygon boundary cache. Full installed regressions and inherited reader/analysis/archive/classifier/batch/runtime/desktop gates are audited at the exact dispatched source. Fifty LoG cases cover pinned nuclei, independent compartment measurements, context and automatic-level admission, transparency, physical limits, archive/classifier-source replay, CLI/batches, cancellation and desktop/viewer capture. Legacy strict scientific failures remain explicit. A Linux-only observation does not accept Windows or the overall matrix. Performance/hardware, resampling/body parity, broader biology, macOS, portable ABI/consumers, license and supported release remain separate gates.',jobs=[])

def canonical_source_fingerprint():
    names=subprocess.check_output(['git','-C',str(ROOT),'ls-tree','-r','--name-only',COMMIT]).decode().splitlines()
    names=sorted(n for n in names if n in ['CMakeLists.txt','pyproject.toml'] or
        (n.startswith('cpp/') and n.endswith(('.cpp','.hpp'))) or
        (n.startswith('cmake/') and n.endswith(('.cmake','.in'))) or (n.startswith('src/') and n.endswith('.py')))
    stream=subprocess.check_output(['git','-C',str(ROOT),'cat-file','--batch'],input=''.join(COMMIT+':'+n+'\n' for n in names).encode())
    offset=0;manifest=[]
    for name in names:
        end=stream.index(b'\n',offset);header=stream[offset:end].split()
        assert len(header)==3 and header[1]==b'blob'
        size=int(header[2]);blob=stream[end+1:end+size+1];offset=end+size+2
        manifest.append(name+'='+hashlib.sha256(blob).hexdigest()+'\n')
    return hashlib.sha256(''.join(manifest).encode()).hexdigest()
expected_source=canonical_source_fingerprint()
report['git_lf_source_tree_sha256']=expected_source

local=json.loads((ROOT/'docs/benchmarks/classifier-engine-windows.json').read_text(encoding='utf8'))
for job in run['jobs']:
    folder=args.artifacts/folders[job['name']]
    entry=dict(name=job['name'],url=job['url'],status=job['status'],conclusion=job['conclusion'],native=junit(folder/'native-tests.xml'))
    assert entry['native']['passed']==(46 if 'GUI' in job['name'] else 45)+int(args.watershed_bodies)
    native_cases=list(ET.parse(folder/'native-tests.xml').getroot().iter('testcase'))
    if args.sampled_shape:
        selected=[t for t in native_cases if t.get('name')=='log_watershed']
        assert len(selected)==1 and 'Sampled shape 26 frozen masks and maxima passed' in selected[0].findtext('system-out',''), 'missing frozen sampled-shape native conformance'
        fixture='tests/fixtures/imagej-shape-1.54p/capture/manifest.json'
        assert (ROOT/fixture).read_bytes()==subprocess.check_output(['git','-C',str(ROOT),'show',COMMIT+':'+fixture]), 'shape reference differs from dispatched source'
        entry['sampled_shape']=dict(native_frozen_masks_and_maxima=26,capture_manifest_sha256=sha(ROOT/fixture),production_integrated=False)
        if (folder/'python-tests.xml').exists():
            expected=json.loads((ROOT/'tests/fixtures/imagej-shape-1.54p/verification-cohorts.json').read_text())
            entry['sampled_shape']['cohort_contract_sha256']=sha(ROOT/'tests/fixtures/imagej-shape-1.54p/verification-cohorts.json')
            all_cases=list(ET.parse(folder/'python-tests.xml').getroot().iter('testcase'))
            counts={}
            for classname,names in expected.items():
                cases=[t for t in all_cases if t.get('classname','').rsplit('.',1)[-1]==classname]
                assert len(cases)==len(names) and sorted(t.get('name') for t in cases)==names, 'sampled-shape verification cohort differs: '+classname
                assert all(all(t.find(k) is None for k in ['failure','error','skipped']) for t in cases), 'sampled-shape verification case did not pass'
                counts[classname]=len(cases)
            assert sum(counts.values())==53
            entry['sampled_shape']['python_cohorts']=counts
    assert {'masked_gaussian','log_watershed','log_cell_recipe','decimal_quantities','cells','geometry'} <= {t.get('name') for t in native_cases}
    assert entry['native']['skipped']==0
    if (folder/'python-tests.xml').exists():
        entry['python']=junit(folder/'python-tests.xml')
        contours=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_pixel_contours')]
        assert len(contours)==24 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in contours)
        entry['pixel_contour_cases']=[t.get('name') for t in contours]
        if 'GUI' in job['name']:
            widgets=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_pixel_classifier')]
            assert len(widgets)==20 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in widgets)
            entry['pixel_widget_cases']=[t.get('name') for t in widgets]
            saved=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_pixel_archives')]
            assert len(saved)==8 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in saved)
            entry['saved_pixel_widget_cases']=[t.get('name') for t in saved]
            windows=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_pixel_windows')]
            assert len(windows)==3 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in windows)
            entry['window_pixel_widget_cases']=[t.get('name') for t in windows]
            wheel=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_wheel_replay')]
            assert len(wheel)==1 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in wheel)
            entry['wheel_isolation_cases']=[t.get('name') for t in wheel]
            stream_widgets=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_pixel_stream')]
            assert len(stream_widgets)==7 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in stream_widgets)
            entry['stream_export_widget_cases']=[t.get('name') for t in stream_widgets]
            browser=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_pixel_stream_browser')]
            assert len(browser)==9 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in browser)
            entry['stream_browser_widget_cases']=[t.get('name') for t in browser]
            br=json.loads((folder/'gui-pixel-stream-read-benchmark.json').read_text(encoding='utf8'))
            iw=json.loads((folder/'installed-workflow.json').read_text(encoding='utf8'))
            assert br['native_sha256']==iw['native_sha256'] and br['environment']['source_tree_sha256']==iw['source_tree_sha256'] and br['environment']['git_revision']==COMMIT
            assert br['blocks']==64 and br['objects']==10000 and br['verified'] and br['gpu_bytes']==65536 and br['peak_gpu_bytes']<=1024*1024 and br['main_mask_gpu_bytes']==br['catalog_inspected']==br['ui_overflow']==0
            assert br['rendered_color_samples']==6241
            entry['gui_pixel_stream_read_benchmark']=br

            bg=json.loads((folder/'gui-pixel-stream-benchmark.json').read_text(encoding='utf8'))
            installed=json.loads((folder/'installed-workflow.json').read_text(encoding='utf8'))
            assert bg['native_sha256']==installed['native_sha256'] and bg['environment']['source_tree_sha256']==installed['source_tree_sha256'] and bg['environment']['git_revision']==COMMIT
            assert bg['blocks']==64 and bg['objects']==10000 and bg['catalog_max_step']<=1024 and bg['gpu_bytes']==bg['peak_gpu_bytes']==0 and bg['ui_overflow']==0
            assert bg['estimated_producer_work_bytes']<=8*1024*1024 and bg['max_block_array_bytes']==147456
            entry['gui_pixel_stream_benchmark']=bg


            window=json.loads((folder/'gui-pixel-window-benchmark.json').read_text(encoding='utf8'))
            wf=json.loads((folder/'installed-workflow.json').read_text(encoding='utf8'))
            assert window['native_sha256']==wf['native_sha256'] and window['environment']['source_tree_sha256']==wf['source_tree_sha256']
            assert window['environment']['git_revision']==COMMIT and window['objects']==10000 and window['windowed'] and window['estimated_work_bytes']<=8*1024*1024
            assert window['gpu_bytes']==window['mask_width']*window['mask_height']*4 and window['mask_width']<1024 and window['mask_height']<1024 and window['ui_overflow']==0
            entry['gui_pixel_window_benchmark']=window

            retention=json.loads((folder/'gui-pixel-archive-benchmark.json').read_text(encoding='utf8'))
            assert retention['objects']==10000 and retention['size']==1024 and retention['catalog_max_step']<=1024 and retention['record_catalog_max_step']<=1024 and retention['gpu_bytes']==4194304 and retention['ui_overflow']==0
            entry['gui_pixel_archive_benchmark']=retention

            contour_gui=json.loads((folder/'gui-pixel-contour-benchmark.json').read_text(encoding='utf8'))
            assert contour_gui['native_sha256']==json.loads((folder/'installed-workflow.json').read_text(encoding='utf8'))['native_sha256']
            assert contour_gui['objects']==10000 and contour_gui['polygon_parts']==1024 and contour_gui['gpu_bytes']==0 and contour_gui['peak_gpu_bytes']==1048576 and contour_gui['catalog_max_step']<=1024 and contour_gui['ui_overflow']==0
            entry['gui_pixel_contour_benchmark']=contour_gui


        for suffix,count in [('test_pixel_archives',51),('test_pixel_archive_portable',4),('test_pixel_prediction_windows',22),('test_pixel_stream',24),('test_pixel_stream_file',38),('test_pixel_stream_file_portable',2),('test_pixel_stream_export_job',14),('test_pixel_stream_read_job',21),('test_pixel_stream_cli',46),('test_pixel_stream_records',34),('test_pixel_input_digest',25),('test_project_pixel_batch',25),('test_tma',23),('test_tiff_reader',63),('test_tiff_project',7 if args.ndpi_readers else 6),('test_tiff_cli',10),('test_raw_analysis',21),('test_raw_records',12),('test_raw_tables',3),('test_raw_batch',4)]:
            cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith(suffix)]
            assert len(cases)==count and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in cases), (job['name'], suffix, count, len(cases), [(t.get('name'),[k for k in ['skipped','failure','error'] if t.find(k) is not None]) for t in cases if any(t.find(k) is not None for k in ['skipped','failure','error'])])
            entry[suffix+'_cases']=[t.get('name') for t in cases]

        if 'GUI' in job['name']:
            records=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_pixel_stream_records')]
            assert len(records)==6 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in records)
            entry['pixel_stream_record_widget_cases']=[t.get('name') for t in records]
        for record_name in ['pixel-stream-record-benchmark.json','gui-pixel-stream-record-benchmark.json']:
            record_path=folder/record_name
            if not record_path.exists():
                assert record_name.startswith('gui-') and 'GUI' not in job['name']
                continue
            records=json.loads(record_path.read_text(encoding='utf8'))
            iw=json.loads((folder/'installed-workflow.json').read_text(encoding='utf8'))
            assert records['native_sha256']==iw['native_sha256'] and records['environment']['source_tree_sha256']==iw['source_tree_sha256'] and records['environment']['git_revision']==COMMIT
            assert records['records']==10000 and records['project_schema']==13 and records['max_encoded_descriptor_bytes']<=131072 and records['total_encoded_descriptor_bytes']<=64*1024*1024
            if record_name.startswith('gui-'):
                assert records['gui']['document_unchanged'] and records['gui']['catalog_complete'] and records['gui']['catalog_max_step']<=1024 and records['gui']['ui_overflow']==0 and records['gui']['gpu_bytes']==65536
            entry[record_name]=records

        digest=json.loads((folder/'pixel-input-digest-benchmark.json').read_text(encoding='utf8'))
        iw=json.loads((folder/'installed-workflow.json').read_text(encoding='utf8'))
        assert digest['native_sha256']==iw['native_sha256'] and digest['environment']['source_tree_sha256']==iw['source_tree_sha256'] and digest['environment']['git_revision']==COMMIT
        local_digest=json.loads((ROOT/'docs/benchmarks/pixel-input-digest-1024-windows.json').read_text(encoding='utf8'))
        assert digest['input_sha256']==local_digest['input_sha256'] and digest['model_sha256']==local_digest['model_sha256']
        assert digest['size']==1024 and digest['blocks']==64 and digest['max_block_array_bytes']==147456 and digest['estimated_producer_work_bytes']<=digest['budget_bytes']
        entry['pixel_input_digest_benchmark']=digest

        workflow=json.loads((folder/'installed-workflow.json').read_text(encoding='utf8'))
        entry['workflow']={k:v for k,v in workflow.items() if k not in ['native','executable']}
        entry['artifact']=json.loads((folder/'result.json').read_text(encoding='utf8'))
        wheel=folder/'wheels'/entry['artifact']['wheel']
        assert sha(wheel)==entry['artifact']['wheel_sha256']
        with zipfile.ZipFile(wheel) as source:
            native=next(n for n in source.namelist() if Path(n).name.startswith('_native.') and Path(n).suffix in ['.pyd','.so'])
            assert hashlib.sha256(source.read(native)).hexdigest()==workflow['native_sha256']
            notices=[n for n in source.namelist() if '/licenses/opencv/' in n]
            assert len(notices)==4
            entry['opencv_notices']={n:hashlib.sha256(source.read(n)).hexdigest() for n in notices}
        benchmark=json.loads((folder/'classifier-benchmark.json').read_text(encoding='utf8'))
        assert benchmark['native_sha256']==workflow['native_sha256'] and benchmark['environment']['source_tree_sha256']==workflow['source_tree_sha256']
        assert benchmark['environment']['git_revision']==COMMIT
        for key in ['model_sha256','training_sha256','input_sha256','model_bytes','nodes','fixture_agreement']:
            assert benchmark[key]==local[key],(job['name'],key)
        entry['classifier_benchmark']=benchmark
        object_benchmark=json.loads((folder/'object-classifier-benchmark.json').read_text(encoding='utf8'))
        assert object_benchmark['native_sha256']==workflow['native_sha256']
        assert object_benchmark['environment']['source_tree_sha256']==workflow['source_tree_sha256']
        assert object_benchmark['environment']['git_revision']==COMMIT
        object_local=json.loads((ROOT/'docs/benchmarks/object-classifier-10000-windows.json').read_text(encoding='utf8'))
        for key in ['model_sha256','table_sha256','changed_objects']:
            assert object_benchmark[key]==object_local[key],(job['name'],key)
        entry['object_classifier_benchmark']=object_benchmark
        archive_benchmark=json.loads((folder/'classifier-archive-benchmark.json').read_text(encoding='utf8'))
        assert archive_benchmark['native_sha256']==workflow['native_sha256']
        assert archive_benchmark['environment']['source_tree_sha256']==workflow['source_tree_sha256']
        assert archive_benchmark['environment']['git_revision']==COMMIT
        archive_local=json.loads((ROOT/'docs/benchmarks/classifier-archive-10000-windows.json').read_text(encoding='utf8'))
        for key in ['model_sha256','table_sha256','prediction_rows','classifier_records']:
            assert archive_benchmark[key]==archive_local[key],(job['name'],key)
        entry['classifier_archive_benchmark']=archive_benchmark
        pixel_archive=json.loads((folder/'pixel-archive-benchmark.json').read_text(encoding='utf8'))
        assert pixel_archive['native_sha256']==workflow['native_sha256'] and pixel_archive['source_tree_sha256']==workflow['source_tree_sha256']
        assert pixel_archive['rows']==4000 and pixel_archive['features']==6 and pixel_archive['size']==512
        assert pixel_archive['prediction_encoded_bytes']>512*512*9 and pixel_archive['save_snapshot_bytes']>pixel_archive['prediction_charged_payload_bytes']
        entry['pixel_archive_benchmark']=pixel_archive

        pixel=json.loads((folder/'pixel-classifier-benchmark.json').read_text(encoding='utf8'))
        assert pixel['native_sha256']==workflow['native_sha256'] and pixel['environment']['source_tree_sha256']==workflow['source_tree_sha256']
        assert pixel['environment']['git_revision']==COMMIT
        pixel_local=json.loads((ROOT/'docs/benchmarks/pixel-classifier-512-windows.json').read_text(encoding='utf8'))
        for key in ['model_sha256','input_sha256','full_sha256','feature_count','halo','tiles']:
            assert pixel[key]==pixel_local[key],(job['name'],key,pixel[key],pixel_local[key])
        entry['pixel_classifier_benchmark']=pixel
        source=json.loads((folder/'pixel-workflow-benchmark.json').read_text(encoding='utf8'))
        assert source['native_sha256']==workflow['native_sha256'] and source['environment']['source_tree_sha256']==workflow['source_tree_sha256']
        assert source['environment']['git_revision']==COMMIT
        assert source['training_rows']==4000 and source['sampled_class_counts']==[2000,2000] and len(source['verified'])==6
        source_local=json.loads((ROOT/'docs/benchmarks/pixel-workflow-512-windows.json').read_text(encoding='utf8'))
        assert source['table_sha256']==source_local['table_sha256']
        # The remaining allowance is serialized into forest parameters and is
        # charged against source/build provenance; no cross-host model-file or
        # prediction-identity equality is inferred for this high-level fixture.
        entry['pixel_workflow_benchmark']=source
        producer=json.loads((folder/'pixel-stream-benchmark.json').read_text(encoding='utf8'))
        assert producer['native_sha256']==workflow['native_sha256'] and producer['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and producer['environment']['git_revision']==COMMIT
        assert producer['size']==1024 and producer['blocks']==64 and producer['budget_bytes']==8*1024*1024 and producer['estimated_producer_work_bytes']<=producer['budget_bytes']
        assert producer['max_block_array_bytes']==147456 and producer['valid_pixels']==1024*1024 and len(producer['verified'])==4
        entry['pixel_stream_benchmark']=producer
        read=json.loads((folder/'pixel-stream-read-benchmark.json').read_text(encoding='utf8'))
        assert read['native_sha256']==workflow['native_sha256'] and read['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and read['environment']['git_revision']==COMMIT
        assert read['blocks']==64 and read['valid_pixels']==1048576 and read['max_block_array_bytes']==147456 and read['retained_poll_samples']<=256
        entry['pixel_stream_read_benchmark']=read
        cli=json.loads((folder/'pixel-stream-cli-benchmark.json').read_text(encoding='utf8'))
        assert cli['native_sha256']==workflow['native_sha256'] and cli['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and cli['environment']['git_revision']==COMMIT
        assert cli['size']==1024 and cli['blocks']==64 and cli['estimated_producer_work_bytes']<=8*1024*1024 and cli['max_block_array_bytes']==147456 and cli['valid_pixels']==1048576
        with zipfile.ZipFile(wheel) as cli_wheel:
            exe_name='qupathxx/qupathxx_batch'+('.exe' if 'Windows' in job['name'] else '')
            assert hashlib.sha256(cli_wheel.read(exe_name)).hexdigest()==cli['executable_sha256']
        entry['pixel_stream_cli_benchmark']=cli


        for name in ['pixel-stream-file-benchmark.json','pixel-stream-job-benchmark.json']:
            b=json.loads((folder/name).read_text(encoding='utf8'))
            assert b['native_sha256']==workflow['native_sha256'] and b['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and b['environment']['git_revision']==COMMIT
            assert b['size']==1024 and b['blocks']==64 and b['max_block_array_bytes']==147456 and b['valid_pixels']==1048576 and b['estimated_producer_work_bytes']<=8*1024*1024
            if name=='pixel-stream-job-benchmark.json':assert b['job']['state']=='saved' and 0<b['job']['retained_poll_samples']<=256
            entry[name]=b





        batch=json.loads((folder/'project-pixel-batch-benchmark.json').read_text(encoding='utf8'))
        assert batch['native_sha256']==workflow['native_sha256'] and batch['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and batch['environment']['git_revision']==COMMIT
        assert batch['size']==1024 and batch['images']==2 and len(batch['verified'])==2
        assert batch['estimated_producer_work_bytes']<=batch['producer_budget'] and batch['max_block_array_bytes']==147456
        local_batch=json.loads((ROOT/'docs/benchmarks/project-pixel-batch-1024-windows.json').read_text(encoding='utf8'))
        assert [v['input_sha256'] for v in batch['verified']]==[v['input_sha256'] for v in local_batch['verified']]
        assert all(v['blocks']==64 and v['inferred_on_resume']==0 for v in batch['verified'])
        entry['project_pixel_batch_benchmark']=batch
        interrupt=json.loads((folder/'pixel-batch-interrupt.json').read_text(encoding='utf8'))
        assert interrupt['native_sha256']==workflow['native_sha256'] and interrupt['source_tree_sha256']==workflow['source_tree_sha256']
        assert interrupt['exit_code']==130 and interrupt['observed_stream_temporary_before_signal'] and interrupt['no_incomplete_outputs'] and interrupt['source_and_model_unchanged']
        with zipfile.ZipFile(wheel) as contents:
            assert hashlib.sha256(contents.read('qupathxx/qupathxx_batch'+('.exe' if 'Windows' in job['name'] else ''))).hexdigest()==interrupt['executable_sha256']
        entry['pixel_batch_interrupt']=interrupt
        if 'GUI' in job['name']:
            cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_pixel_batch')]
            assert len(cases)==6 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in cases)
            entry['pixel_batch_gui_cases']=[t.get('name') for t in cases]

        records=json.loads((folder/'raw-records-benchmark.json').read_text(encoding='utf-8'))
        assert records['source_tree_sha256']==workflow['source_tree_sha256']
        assert records['record_context']=='child -> parent -> TMA core' and records['distinct_result_instances']==10000
        assert records['retained_input_sha256']==json.loads((ROOT/'docs/benchmarks/raw-records-windows.json').read_text(encoding='utf-8'))['retained_input_sha256']
        assert records['records']==records['reopened_records']==10000 and records['channels']==4 and records['csv_rows']==40000
        assert records['page_size']==64 and records['decoded_blocks']==64 and not records['warm_scans_add_decoded_blocks']
        assert records['table_snapshot_charged_bytes']<128*1024*1024 and records['save_snapshot_bytes']<256*1024*1024
        assert records['input_sha256']==json.loads((ROOT/'docs/benchmarks/raw-records-windows.json').read_text(encoding='utf-8'))['input_sha256']
        for key in ['cold_analysis_ms','warm_analysis_median_ms','warm_input_digest_median_ms','keep_all_ms','page_max_ms','table_capture_ms','csv_ms','save_capture_ms','save_write_ms']:
            assert math.isfinite(records[key]) and records[key]>=0
        assert workflow['raw_archive_records']==1 and workflow['raw_replay_verified'] and workflow['raw_batch_reused']
        entry['raw_records_benchmark']=records
        if 'GUI' in job['name']:
            cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_raw_records')]
            assert len(cases)==5 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in cases)
            entry['raw_records_gui_cases']=[t.get('name') for t in cases]
        raw=json.loads((folder/'tiff-reader-benchmark.json').read_text(encoding='utf8'))
        assert raw['native_sha256']==workflow['native_sha256'] and raw['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and raw['environment']['git_revision']==COMMIT
        assert raw['decoded_blocks']==64 and raw['cache_peak_bytes']<=raw['cache_budget_bytes']
        assert all(raw[key] for key in ['exact_raw_verified','preview_verified','view_cache_reuse_verified','distinct_channels_verified','independent_close_verified'])
        entry['tiff_reader_benchmark']=raw
        with zipfile.ZipFile(wheel) as archive:
            entry['tiff_notices']={name:hashlib.sha256(archive.read('qupathxx/licenses/'+name)).hexdigest() for name in ['libtiff/LICENSE.md','tiff-zlib/LICENSE','tinyxml2/LICENSE.txt']}
        if 'GUI' in job['name']:
            cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_raw')]
            assert len(cases)==7 and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in cases)
            entry['raw_gui_cases']=[t.get('name') for t in cases]
            b=json.loads((folder/'gui-tiff-reader-benchmark.json').read_text(encoding='utf8'))
            assert b['native_sha256']==workflow['native_sha256'] and b['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and b['environment']['git_revision']==COMMIT
            assert b['decoded_blocks']==64 and b['cache_peak_bytes']<=b['cache_budget_bytes'] and b['distinct_channels_verified'] and b['gui']['tiff_state']=='ready' and b['gui']['ui_overflow']==0
            entry['raw_gui_benchmark']=b
        tma=json.loads((folder/'tma-benchmark.json').read_text(encoding='utf8'))
        assert tma['native_sha256']==workflow['native_sha256'] and tma['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and tma['environment']['git_revision']==COMMIT
        assert tma['cores']==10000 and tma['missing']==1000 and tma['immutable_snapshot_verified'] and tma['unrelated_edit_kept_tma_revision'] and tma['snapshot_bytes']<64*1024*1024
        entry['tma_benchmark']=tma
        if 'GUI' in job['name']:
            cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_tma')]
            assert len(cases)==(12 if args.tma_cases else 8) and all(not any(t.find(k) is not None for k in ['skipped','failure','error']) for t in cases)
            entry['tma_gui_cases']=[t.get('name') for t in cases]
            b=json.loads((folder/'gui-tma-benchmark.json').read_text(encoding='utf8'))
            assert b['native_sha256']==workflow['native_sha256'] and b['environment']['source_tree_sha256']==workflow['source_tree_sha256'] and b['environment']['git_revision']==COMMIT
            assert b['cores']==10000 and b['missing']==1000 and 0<b['gui']['catalog_max_step']<=512 and b['gui']['ui_overflow']==0
            entry['tma_gui_benchmark']=b

        gui_path=folder/'gui-pixel-benchmark.json'
        if gui_path.exists():
            gui=json.loads(gui_path.read_text(encoding='utf8'))
            assert gui['native_sha256']==workflow['native_sha256'] and gui['environment']['source_tree_sha256']==workflow['source_tree_sha256']
            assert gui['objects']==10000 and gui['catalog_max_step']<=1024 and gui['ui_overflow']==0 and gui['gpu_bytes']==119040
            entry['gui_pixel_benchmark']=gui

        entry['checks']={p.name:sha(p) for p in folder.glob('*.log') if p.name in ['stubs.log','typing.log','typing-negative.log'] or p.name.startswith('standalone')}
        assert 'Public stubs match' in (folder/'stubs.log').read_text(encoding='utf8')
        assert 'Success: no issues found in 3 source files' in (folder/'typing.log').read_text(encoding='utf8')
        assert sorted(map(int,re.findall(r'invalid_api.py:(\d+): error:',(folder/'typing-negative.log').read_text(encoding='utf8'))))==[6,7,8,9,11,13,14]
        candidate=folder/'qupathxx-windows-x64-candidate.zip'
        if candidate.exists():
            with zipfile.ZipFile(candidate) as source:
                files={i.filename:hashlib.sha256(source.read(i)).hexdigest() for i in source.infolist() if not i.is_dir()}
            assert len([n for n in files if n.endswith('.exe')])==3
            assert not any(Path(n).suffix.lower() in ['.py','.pyi','.pyd'] or Path(n).name.lower().startswith('python') or 'opengl32' in n.lower() or 'libgallium' in n.lower() for n in files)
            with zipfile.ZipFile(wheel) as source:
                for name,digest in files.items():
                    if name!='README.txt':assert hashlib.sha256(source.read('qupathxx/'+name)).hexdigest()==digest
            entry['standalone']=dict(filename=candidate.name,bytes=candidate.stat().st_size,sha256=sha(candidate),files=files)
    if (folder/'python-tests.xml').exists():
        cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_openslide_extended')]
        assert len(cases)==14 and not any(t.find('failure') is not None or t.find('error') is not None for t in cases)
        assert sum(t.find('skipped') is not None for t in cases)==(0 if 'GUI' in job['name'] else 2)
        assert json.loads((folder/'result.json').read_text(encoding='utf-8'))['extended_readers'] is True
        vendor=json.loads((folder/'vendor-reader-benchmark.json').read_text(encoding='utf-8'))
        local_vendor=json.loads((ROOT/'docs/benchmarks/vendor-reader-windows.json').read_text(encoding='utf-8'))
        assert vendor['fixture_sha256']==local_vendor['fixture_sha256']
        assert vendor['overview_rgba_sha256']==local_vendor['overview_rgba_sha256']
        assert [v['rgba_sha256'] for v in vendor['regions']]==[v['rgba_sha256'] for v in local_vendor['regions']]
        assert vendor['native_sha256']==json.loads((folder/'installed-workflow.json').read_text(encoding='utf-8'))['native_sha256']
        assert vendor['scheduler_reads']==1 and vendor['scheduler_cache_hits']==9
        assert vendor['scheduler_cache_bytes']<=vendor['scheduler_cache_limit'] and vendor['overview_source_pixels']==5913875
        entry['extended_reader_cases']=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in cases]
        entry['vendor_reader_benchmark']=vendor
        thumbnails=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_thumbnail')]
        assert len(thumbnails)==24
        for t in thumbnails:
            if 'cli_' in t.get('name',''):assert not any(t.find(k) is not None for k in ['skipped','failure','error'])
        if 'GUI' in job['name']:
            overview_cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_thumbnail')]
            assert len(overview_cases)==12 and not any(any(t.find(k) is not None for k in ['skipped','failure','error']) for t in overview_cases)
            overview=json.loads((folder/'gui-overview-benchmark.json').read_text(encoding='utf-8'))
            assert overview['native_sha256']==json.loads((folder/'installed-workflow.json').read_text(encoding='utf-8'))['native_sha256']
            assert len(overview['cases'])==4 and [c['overview_gpu_bytes'] for c in overview['cases']]==[0,125400,125400,0]
            assert [c['source_pixel_limit'] for c in overview['cases']]==[4194304,8388608,8388608,4194304]
            assert overview['cases'][1]['overview_rgba_sha256']==overview['cases'][2]['overview_rgba_sha256']==vendor['overview_rgba_sha256']
            assert all(c['ui_overflow']==0 and c['canvas_peak_gpu_bytes']<=64*1024*1024 and c['overview_peak_gpu_bytes']<=150000 for c in overview['cases'])
            assert (folder/'gui-overview.png').stat().st_size>10000
            entry['overview_budget_cases']=[t.get('name') for t in overview_cases]
            entry['overview_budget_benchmark']=overview
            entry['overview_completed_frame_sha256']=sha(folder/'gui-overview.png')

    if (folder/'python-tests.xml').exists():
        cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_geos_runtime_bundle')]
        assert len(cases)==4
        is_linux=job['name']=='Linux headless Python 3.12 NumPy 2'
        assert all(not any(t.find(k) is not None for k in ['failure','error']) for t in cases)
        assert sum(t.find('skipped') is not None for t in cases)==(0 if is_linux else 4)
        entry['linux_bundle_configuration_cases']=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in cases]
        assert (folder/'linux-runtime.json').exists()==is_linux
        if is_linux:
            runtime=json.loads((folder/'linux-runtime.json').read_text(encoding='utf-8'))
            workflow=json.loads((folder/'installed-workflow.json').read_text(encoding='utf-8'))
            assert runtime['schema']=='qpx.linux-runtime/1'
            assert runtime['source_tree_sha256']==workflow['source_tree_sha256']
            assert runtime['native_sha256']==workflow['native_sha256']
            for key in ['mapped_from_package','geos_license_matches_sdk','relocated_python_geometry','relocated_region_pixels','relocated_batch']:
                assert runtime[key] is True
            assert runtime['ld_library_path_used'] is False
            child=runtime['relocated_child']
            assert child['boolean_area']==150 and len(child['mapped'])==3
            from pathlib import PurePosixPath
            package=PurePosixPath(child['native']).parent
            assert 'relocated package' in str(package)
            assert {PurePosixPath(p).name for p in child['mapped']}==set(runtime['runtimes'])
            assert all(PurePosixPath(p).parent==package for p in child['mapped'])
            with zipfile.ZipFile(wheel) as source:
                for name,digest in runtime['runtimes'].items():
                    assert hashlib.sha256(source.read('qupathxx/'+name)).hexdigest()==digest
                notice=source.read('qupathxx/licenses/geos/COPYING')
                assert len(notice)>20000 and b'GNU LESSER GENERAL PUBLIC LICENSE' in notice
                entry['geos_notice_sha256']=hashlib.sha256(notice).hexdigest()
            entry['linux_runtime']=runtime
            entry['linux_runtime_record_sha256']=sha(folder/'linux-runtime.json')
    if (folder/'python-tests.xml').exists():
        cells=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_qupath_cells_reference')]
        assert len(cells)==23
        expected={'test_isolated_cell_upstream_acceptance_gate[baseline_count]',
                  'test_isolated_cell_upstream_acceptance_gate[isolated_nucleus_iou]',
                  'test_isolated_cell_upstream_acceptance_gate[isolated_centroid]'}
        assert not any(t.find('failure') is not None or t.find('error') is not None for t in cells)
        skipped=[t for t in cells if t.find('skipped') is not None]
        assert {t.get('name') for t in skipped}==expected
        assert all(t.find('skipped').get('type')=='pytest.xfail' for t in skipped)
        entry['qupath_cell_characterization']=dict(passed=20,strict_expected_failures=3,
            unresolved_scientific_gates=sorted(expected),segmentation_parity_accepted=False,
            cases=[dict(name=t.get('name'),expected_failure=t.get('name') in expected) for t in cells])
    if (folder/'python-tests.xml').exists():
        sdk_cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_opencv_sdk_layout')]
        assert len(sdk_cases)==6 and all(not any(t.find(k) is not None for k in ['failure','error','skipped']) for t in sdk_cases)
        entry['opencv_sdk_layout_cases']=[t.get('name') for t in sdk_cases]
    if (folder/'python-tests.xml').exists():
        cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_geos_sdk_layout')]
        assert len(cases)==4 and all(not any(t.find(k) is not None for k in ['failure','error','skipped']) for t in cases)
        entry['geos_sdk_layout_cases']=[t.get('name') for t in cases]
    assert any(t.get('name')=='cell_calibration' for t in native_cases)
    if (folder/'python-tests.xml').exists():
        all_cases=list(ET.parse(folder/'python-tests.xml').getroot().iter('testcase'))
        calibration=[t for t in all_cases if t.get('classname','').endswith('test_cell_calibration')]
        assert len(calibration)==13 and all(not any(t.find(k) is not None for k in ['failure','error','skipped']) for t in calibration)
        entry['calibrated_cell_cases']=[t.get('name') for t in calibration]
        tables=[t for t in all_cases if t.get('classname','').endswith('test_gui_cell_tables')]
        assert len(tables)==8
        assert all(not any(t.find(k) is not None for k in ['failure','error']) for t in tables)
        assert sum(t.find('skipped') is not None for t in tables)==(0 if 'GUI' in job['name'] else 8)
        entry['gui_cell_table_cases']=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in tables]
    if (folder/'python-tests.xml').exists():
        cases=list(ET.parse(folder/'python-tests.xml').getroot().iter('testcase'))
        areas=[t for t in cases if t.get('classname','').endswith('test_cell_area_limits')]
        assert len(areas)==22 and all(not any(t.find(k) is not None for k in ['failure','error','skipped']) for t in areas)
        stale=[t for t in areas if t.get('name','').startswith('test_calibration_changes_stale_physical_capture_but_not_frozen_archive_replay')]
        assert len(stale)==3
        entry['physical_cell_area_cases']=[t.get('name') for t in areas]
        gui=[t for t in cases if t.get('classname','').endswith('test_gui_cell_area_limits')]
        assert len(gui)==3 and all(not any(t.find(k) is not None for k in ['failure','error']) for t in gui)
        assert sum(t.find('skipped') is not None for t in gui)==(0 if 'GUI' in job['name'] else 3)
        entry['gui_physical_cell_area_cases']=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in gui]
    if (folder/'python-tests.xml').exists():
        cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_viewer_cell_area_limits')]
        assert len(cases)==7 and all(not any(t.find(k) is not None for k in ['failure','error']) for t in cases)
        assert sum(t.find('skipped') is not None for t in cases)==(0 if 'GUI' in job['name'] else 2)
        entry['managed_viewer_physical_area_cases']=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in cases]
        assert any(t.get('name')=='viewer_session' for t in native_cases)
    if (folder/'python-tests.xml').exists():
        all_cases=list(ET.parse(folder/'python-tests.xml').getroot().iter('testcase'))
        sampling=[t for t in all_cases if t.get('classname','').endswith('test_sampling_policy')]
        assert len(sampling)==35 and all(not any(t.find(k) is not None for k in ['failure','error']) for t in sampling)
        assert sum(t.find('skipped') is not None for t in sampling)==(0 if 'GUI' in job['name'] else 3)
        cache=[t for t in all_cases if t.get('classname','').endswith('test_sampling_acceleration')]
        assert len(cache)==9 and all(not any(t.find(k) is not None for k in ['failure','error','skipped']) for t in cache)
        entry['sampling_cache_cases']=[t.get('name') for t in cache]
        entry['roi_sampling_policy_cases']=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in sampling]
        gate=[t for t in all_cases if t.get('name')=='test_resampled_cell_explicit_winding_sampling_acceptance_gate']
        assert len(gate)==1 and not any(gate[0].find(k) is not None for k in ['failure','error','skipped'])
        entry['explicit_winding_sampling_gate_passed']=True
        diagonal=[t for t in sampling if t.get('name','').startswith('test_exact_diagonal_boundary_uses_orientation_without_interpolation')]
        assert len(diagonal)==8 and all(t.find('skipped') is None for t in diagonal)
        long_diagonal=[t for t in sampling if t.get('name')=='test_long_diagonal_native_roi_and_tissue_counts']
        assert len(long_diagonal)==1 and long_diagonal[0].find('skipped') is None
        entry['robust_diagonal_cases']=[t.get('name') for t in diagonal+long_diagonal]

        assert any(t.get('name')=='geometry' for t in native_cases)
        with zipfile.ZipFile(wheel) as source:
            stubs=source.read('qupathxx/_native.pyi').decode('utf-8')
            assert 'class RoiSamplingPolicy(' in stubs and 'def analyze_with_sampling_policy(' in stubs
    if (folder/'python-tests.xml').exists():
        all_cases=list(ET.parse(folder/'python-tests.xml').getroot().iter('testcase'))
        log=[t for t in all_cases if t.get('classname','').endswith('test_log_cells')]
        contract=json.loads((ROOT/'tests/fixtures/qupath-cells-0.7.0/contract.json').read_text(encoding='utf-8'))
        expected={f"test_log_installed_nucleus_reference[{tile}-{case['name']}]" for tile in contract['tile_sizes'] for case in contract['cases']}
        expected.update({
            'test_detector_parameters_are_captured_and_inactive_options_reject',
            'test_log_saved_recipe_replay_tables_and_classifier_sources[False]',
            'test_log_saved_recipe_replay_tables_and_classifier_sources[True]',
            'test_log_batch_resume_and_cli_recipe','test_context_pixels_change_only_the_contextual_digest',
            'test_context_budget_is_admitted_before_launch','test_log_physical_area_limits_and_stale_calibration',
            'test_log_transparent_pixels_never_become_labels','test_log_natural_pyramid_context_hash[16]',
            'test_log_natural_pyramid_context_hash[64]','test_log_candidate_failure_is_atomic_and_worker_reusable',
            'test_mixed_detector_tables_and_classifier_sources_roundtrip'})
        expected.update(f'test_log_jobs_reject_stale_results_and_cancel[{change}]' for change in ['selection','rename','close','authority','cancel'])
        expected.update(f'test_log_auto_level_uses_expanded_context_budget[{level}]' for level in range(3))
        desktop={'test_log_gui_and_copied_recipe','test_log_managed_viewer_captures_detector_recipe','test_log_gui_batch_captures_recipe_and_checkpoint'}
        expected.update(desktop)
        assert len(expected)==50 and len(log)==50 and {t.get('name') for t in log}==expected
        assert not any(t.find('failure') is not None or t.find('error') is not None for t in log)
        skipped=[t for t in log if t.find('skipped') is not None]
        assert {t.get('name') for t in skipped}==(set() if 'GUI' in job['name'] else desktop)
        assert all(t.find('skipped').get('type')!='pytest.xfail' for t in skipped)
        entry['log_cells']=dict(passed=50-len(skipped),desktop_skipped=len(skipped),cases=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in log])
        decimals=[t for t in all_cases if t.get('classname','').endswith('test_tiff_reader') and t.get('name','').startswith('test_ome_decimal_quantity_contract[')]
        assert len(decimals)==12 and all(not any(t.find(k) is not None for k in ['failure','error','skipped']) for t in decimals)
        entry['decimal_quantity_cases']=[t.get('name') for t in decimals]
        with zipfile.ZipFile(wheel) as source:
            stubs=source.read('qupathxx/_native.pyi').decode('utf-8')
            assert 'class CellAlgorithm(' in stubs and 'LOG_WATERSHED' in stubs and 'def log_prominence(' in stubs and 'def context_padding(' in stubs
        if job['name'].startswith('Linux '):
            assert workflow['source_tree_sha256']==expected_source
    if args.ndpi_readers and (folder/'python-tests.xml').exists():
        cases=list(ET.parse(folder/'python-tests.xml').getroot().iter('testcase'))
        ndpi=[t for t in cases if t.get('classname','').endswith('test_ndpi_reader')]
        expected={'test_ndpi_metadata_and_anisotropic_calibration','test_ndpi_complete_overview_and_source_budget',
            'test_ndpi_scheduler_cache_and_pending_close','test_ndpi_log_detection_tile_invariance_and_saved_replay',
            'test_ndpi_truncated_container_rejects','test_ndpi_desktop_pyramid_settles'}
        expected.update(f'test_ndpi_all_levels_padding_and_owned_pixels[{i}]' for i in range(9))
        expected.update(f'test_ndpi_installed_region_cli[{i}]' for i in range(9))
        assert len(ndpi)==24 and {t.get('name') for t in ndpi}==expected
        assert not any(t.find('failure') is not None or t.find('error') is not None for t in ndpi)
        skipped=[t for t in ndpi if t.find('skipped') is not None]
        assert {t.get('name') for t in skipped}==(set() if 'GUI' in job['name'] else {'test_ndpi_desktop_pyramid_settles'})
        assert all(t.find('skipped').get('type')!='pytest.xfail' for t in skipped)
        attachments=[t for t in cases if t.get('name') in {
            'test_opened_slide_document_defaults_to_its_native_link',
            'test_opened_tiff_document_defaults_to_native_link_and_current_view'}]
        assert len(attachments)==2 and all(not any(t.find(k) is not None for k in ['failure','error','skipped']) for t in attachments)
        native_project=[t for t in native_cases if t.get('name')=='project']
        assert len(native_project)==1 and 'Native NDPI inferred link, LoG archive replay and calibration passed' in native_project[0].findtext('system-out','')
        fixture=next(e for e in json.loads((ROOT/'tests/fixtures/manifest.json').read_text(encoding='utf-8'))['downloads'] if e['filename']=='CMU-1.ndpi')
        benchmark_path=folder/'ndpi-reader-benchmark.json'
        if benchmark_path.exists():
            observation=json.loads(benchmark_path.read_text(encoding='utf-8'))
            assert observation['native_sha256']==workflow['native_sha256'] and observation['source_tree_sha256']==workflow['source_tree_sha256']
            assert observation['build_git_revision']==COMMIT and observation['fixture_sha256']==fixture['sha256']
            assert observation['vendor']=='hamamatsu' and observation['dimensions']==fixture['dimensions']
            assert observation['region_shape']==[193,257,4] and [r['level'] for r in observation['regions']]==list(range(9))
            assert observation['warm_iterations']==9 and all(len(r['warm_ms'])==9 for r in observation['regions'])
            assert observation['scheduler_reads']==1 and observation['scheduler_cache_hits']==9
            assert observation['scheduler_cache_bytes']<=observation['scheduler_cache_limit']==8*1024*1024
            assert observation['overview_source_pixels']==400*298 and observation['performance_accepted'] is False
            entry['ndpi_reader_observation']=observation
        entry['ndpi_readers']=dict(passed=24-len(skipped),desktop_skipped=len(skipped),fixture_sha256=fixture['sha256'],
            fixture_bytes=fixture['bytes'],dimensions=fixture['dimensions'],pixel_size_um=fixture['pixel_size_um'],
            native_project_save_replay_exercised=True,cases=[dict(name=t.get('name'),skipped=t.find('skipped') is not None) for t in ndpi],
            default_attachment_cases=[t.get('name') for t in attachments])
    if args.rendered_loop and (folder/'python-tests.xml').exists():
        required = {
            ('test_gui_tiles', 'test_real_tiled_render_and_cleanup'),
            ('test_gui_tiles', 'test_rendered_loop_timing_retains_bounded_recent_samples'),
            ('test_gui_startup', 'test_startup_validates_then_adopts_and_selects_image[project]'),
            ('test_gui_startup', 'test_startup_validates_then_adopts_and_selects_image[recovery]'),
        }
        cases = [t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase')
                 if (t.get('classname', '').rsplit('.', 1)[-1], t.get('name')) in required]
        assert len(cases) == len(required), 'missing rendered-loop timing contract cases'
        assert not any(t.find('failure') is not None or t.find('error') is not None for t in cases)
        desktop = 'GUI' in job['name']
        assert all((t.find('skipped') is None) == desktop for t in cases)
        entry['rendered_loop_timing'] = dict(
            passed=4 if desktop else 0, desktop_skipped=0 if desktop else 4,
            cases=[t.get('name') for t in cases], performance_accepted=False)
    if args.gui_stack and (folder/'python-tests.xml').exists():
        cases = [t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase')
                 if t.get('classname', '').endswith('test_gui_startup') and
                 t.get('name') == 'test_native_launcher_repeated_project_startup_with_default_stack']
        assert len(cases) == 1, 'missing default-stack native launcher regression'
        case = cases[0]
        assert case.find('failure') is None and case.find('error') is None
        desktop = 'GUI' in job['name']
        assert (case.find('skipped') is None) == desktop
        entry['gui_stack'] = dict(passed=int(desktop), desktop_skipped=int(not desktop),
                                 native_processes=8 if desktop else 0,
                                 default_executable_stack_bytes=1024*1024)
    if args.watershed_bodies:
        assert 'cell_expansion' in {t.get('name') for t in native_cases}
        if (folder/'python-tests.xml').exists():
            cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase')
                   if t.get('classname','').endswith('test_log_body_cells')]
            assert len(cases)==42, 'missing production body workflow cases'
            assert not any(t.find('failure') is not None or t.find('error') is not None for t in cases)
            desktop={'test_body_gui_captured_recipe_and_published_pixels',
                     'test_body_managed_viewer_copies_recipe_and_reopens_retained_result',
                     'test_body_gui_batch_keeps_distinct_recipe'}
            skipped={t.get('name') for t in cases if t.find('skipped') is not None}
            assert skipped==(set() if 'GUI' in job['name'] else desktop)
            observation=json.loads((folder/'qupath-cells-body-comparison.json').read_text(encoding='utf8'))
            assert observation['cell_algorithm']=='log-bodies' and observation['build_git_revision']==COMMIT
            assert observation['native_sha256']==entry['workflow']['native_sha256']
            assert observation['source_tree_sha256']==entry['workflow']['source_tree_sha256']
            assert observation['comparison_tool_sha256']==sha(ROOT/'tools/compare_qupath_cells.py')
            assert observation['explicit_winding_sampling_accepted'] and observation['explicit_winding_compartments']==42
            assert observation['segmentation_parity_accepted'] is False and observation['biological_validation'] is False
            assert len(observation['cases'])==9
            exact={'blank','isolated_three','touching_pair','gap_pair','size_filters','gradient_noise','anisotropic_level0'}
            for case in observation['cases']:
                assert case['tile_invariant']
                if case['case_name'] not in exact: continue
                for mask in case['segmentation'].values():
                    assert mask['foreground_iou']==1 and mask['unmatched_reference']==mask['unmatched_native']==0
                    assert all(match['iou']==1 and match['centroid_error_px']==0 for match in mask['matches'])
            entry['watershed_bodies']=dict(passed=42-len(skipped),desktop_skipped=len(skipped),
                exact_natural_grid_cases=sorted(exact),comparison_sha256=sha(folder/'qupath-cells-body-comparison.json'),
                broader_segmentation_parity_accepted=False)
    if args.sampled_cells:
        selected=[t for t in native_cases if t.get('name')=='log_cell_recipe']
        assert len(selected)==1
        native_output=selected[0].findtext('system-out','')
        assert 'Sampled LoG QCL4, explicit seed preset, typed recipe identities, replay and QCL1/2/3 preservation passed' in native_output
        assert 'Sampled clipped components without in-ROI markers retain measurements and replay labels' in native_output
        contract='tests/fixtures/imagej-shape-1.54p/sampled-cell-workflows.json'
        assert (ROOT/contract).read_bytes()==subprocess.check_output(['git','-C',str(ROOT),'show',COMMIT+':'+contract])
        expected=json.loads((ROOT/contract).read_text())
        assert len(expected['cases'])==53 and len(expected['desktop_cases'])==3
        accepted=dict(native_qcl4=True,clipped_measurements=True,cohort_contract_sha256=sha(ROOT/contract))
        if (folder/'python-tests.xml').exists():
            cases=[t for t in ET.parse(folder/'python-tests.xml').getroot().iter('testcase')
                   if t.get('classname','').rsplit('.',1)[-1]==expected['classname']]
            assert sorted(t.get('name') for t in cases)==expected['cases'], 'sampled production cohort differs'
            assert not any(t.find('failure') is not None or t.find('error') is not None for t in cases)
            skipped={t.get('name') for t in cases if t.find('skipped') is not None}
            assert skipped==(set() if 'GUI' in job['name'] else set(expected['desktop_cases']))
            observation=json.loads((folder/'qupath-cells-sampled-comparison.json').read_text())
            assert observation['cell_algorithm']=='log-sampled' and observation['build_git_revision']==COMMIT
            assert observation['native_sha256']==entry['workflow']['native_sha256']
            assert observation['source_tree_sha256']==entry['workflow']['source_tree_sha256']
            assert observation['comparison_tool_sha256']==sha(ROOT/'tools/compare_qupath_cells.py')
            assert observation['explicit_winding_sampling_accepted'] and observation['explicit_winding_compartments']==42
            assert not observation['segmentation_parity_accepted'] and not observation['biological_validation']
            assert len(observation['cases'])==9
            exact={'blank','isolated_three','touching_pair','gap_pair','size_filters','gradient_noise','anisotropic_level0'}
            for case in observation['cases']:
                assert case['tile_invariant']
                if case['case_name'] not in exact: continue
                for mask in case['segmentation'].values():
                    assert mask['foreground_iou']==1 and mask['unmatched_reference']==mask['unmatched_native']==0
                    assert all(match['iou']==1 and match['centroid_error_px']==0 for match in mask['matches'])
            accepted.update(passed=53-len(skipped),desktop_skipped=len(skipped),comparison_sha256=sha(folder/'qupath-cells-sampled-comparison.json'))
            if args.ndpi_readers:
                baseline_path='docs/benchmarks/sampled-shape-ndpi-windows.json'
                assert (ROOT/baseline_path).read_bytes()==subprocess.check_output(['git','-C',str(ROOT),'show',COMMIT+':'+baseline_path])
                baseline=json.loads((ROOT/baseline_path).read_text())
                contexts=[]
                for padding in [0,32]:
                    path=folder/f'qupath-ndpi-sampled-context{padding}.json'
                    real=json.loads(path.read_text())
                    assert real['build_git_revision']==COMMIT
                    assert real['native_sha256']==entry['workflow']['native_sha256']
                    assert real['source_tree_sha256']==entry['workflow']['source_tree_sha256']
                    assert real['tool_sha256']==sha(ROOT/'tools/compare_qupath_ndpi_cells.py')
                    assert real['source_pixels_equal'] and real['source_rgb_sha256']==real['qupath_rgb_sha256']
                    assert real['reference_count']==506 and not real['segmentation_parity_accepted']
                    assert not real['biological_validation'] and not real['performance_accepted']
                    selected=[r for r in real['runs'] if r['mode']=='log-sampled']
                    assert len(selected)==1 and selected[0]['context_padding']==padding
                    sampled=selected[0]
                    assert sampled['tile_invariant'] and sampled['independent_rgb_features_passed']
                    assert [r['tile_size'] for r in sampled['results']]==[16,64,256]
                    frozen=next(r for r in baseline['reports'] if r['context_padding']==padding)
                    wanted=next(r for r in frozen['results'] if r['mode']=='sampled_shape')
                    assert all(r['cells']==wanted['native_count'] and r['labels_sha256']['nucleus']==wanted['labels_sha256']
                               for r in sampled['results'])
                    contexts.append(dict(context_padding=padding,cells=wanted['native_count'],
                                         nucleus_labels_sha256=wanted['labels_sha256'],comparison_sha256=sha(path)))
                accepted['real_ndpi_contexts']=contexts
        entry['sampled_cells']=accepted
    if args.small_frontier:
        entry['small_frontier']=frontier.native(native_cases)
        if (folder/'python-tests.xml').exists():
            entry['small_frontier'].update(frontier.python(list(ET.parse(folder/'python-tests.xml').getroot().iter('testcase')),ROOT,COMMIT))
            if args.ndpi_readers:
                baseline_path='docs/benchmarks/sampled-cells-ndpi-windows.json'
                assert (ROOT/baseline_path).read_bytes()==subprocess.check_output(['git','-C',str(ROOT),'show',COMMIT+':'+baseline_path])
                baseline=json.loads((ROOT/baseline_path).read_text())['reports']
                preserved=[]
                for padding in [0,32]:
                    original=next(r for r in baseline if all(v['context_padding']==padding for v in r['runs']))
                    candidate=json.loads((folder/f'qupath-ndpi-sampled-context{padding}.json').read_text())
                    preserved.append(frontier.unchanged_real_masks(candidate,original,padding))
                entry['small_frontier']['real_slide_masks_preserved']=preserved
                entry['small_frontier']['real_slide_baseline_sha256']=sha(ROOT/baseline_path)
    if args.tma_cases:
        entry['tma_cases']=tma_cases.native(native_cases)
        if (folder/'python-tests.xml').exists():
            entry['tma_cases'].update(tma_cases.python(list(ET.parse(folder/'python-tests.xml').getroot().iter('testcase')),ROOT,COMMIT,gui='GUI' in job['name']))
    report['jobs'].append(entry)
report['fixture_retraining']=dict(local_and_audited_hosted_models_identical=True,
    model_sha256=local['model_sha256'],training_sha256=local['training_sha256'],
    caveat='Evidence for this seeded numeric fixture only; cross-platform retraining identity and biological accuracy are not general guarantees. Hosted hardware differs from local hardware.')
report['auditor']=dict(path='tools/ci/accept_unified_engine.py',sha256=sha(Path(__file__)))
report['windows_linux_engineering_scope_accepted']=not args.linux_only
report['linux_engineering_scope_accepted']=True
report['ndpi_readers_required']=args.ndpi_readers
report['gui_stack_required']=args.gui_stack
report['watershed_bodies_required']=args.watershed_bodies
report['sampled_shape_required']=args.sampled_shape
report['sampled_cells_required']=args.sampled_cells
report['small_frontier_required']=args.small_frontier
if args.small_frontier:report['frontier_helper_sha256']=sha(Path(__file__).with_name('frontier_evidence.py'))
report['tma_cases_required']=args.tma_cases
if args.tma_cases:report['tma_case_helper_sha256']=sha(Path(__file__).with_name('tma_case_evidence.py'))
report['performance_accepted']=False
report['macos_accepted']=False
report['release_ready']=False
report['overall_project_complete']=False
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_bytes((json.dumps(report,indent=2)+'\n').encode('utf-8'))
print(json.dumps([dict(name=j['name'],native=j['native']['passed'],python=j.get('python',{}).get('passed')) for j in report['jobs']],indent=2))
