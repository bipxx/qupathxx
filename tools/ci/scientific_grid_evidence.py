"""Named scientific-grid engineering cohorts; no biological/release acceptance."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

spec=importlib.util.spec_from_file_location('qpx_grid_common',Path(__file__).with_name('qptma_evidence.py'))
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
require=common.require
CASES=json.loads(Path(__file__).with_name('scientific-grid-cases.json').read_text(encoding='utf-8'))
GUI_CASES={
 'test_grid_gui_detects_renders_and_publishes_with_axis_geometry':('requires geometry desktop','opt-in desktop replay'),
 'test_grid_python_producer_retains_exports_and_saves':('requires managed project desktop','opt-in managed desktop'),
}
NEGATIVE={
 'scientific-grid-typing-negative.log':('scientific_grid_invalid.py',[4,6,7,8,9,10]),
 'scientific-channels-typing-negative.log':('scientific_channels_invalid.py',[4,6,7,8,9]),
 'grid-cells-typing-negative.log':('grid_cells_invalid.py',[3,3,4,5,6,7,8]),
 'scientific-raw-typing-negative.log':('raw_grid_invalid.py',[5,6,7]),
}
POSITIVE=['tests/typing/raw_grid_valid.py','tests/typing/valid_api.py','tests/typing/hierarchy_valid.py','tests/typing/grid_cells_valid.py','examples/live_workflow.py','examples/region_workflow.py']
BASE={
 'resampling':['Native scientific grid exact geometry, rounding, context and admission passed','Native scientific RGB float64 exact means, support, reader and cancellation checks passed','Native exact typed weighted means, subnormal/cancellation, atomic rejection and nearest-even rounding passed','Native typed scientific raw sampling, per-channel finite/support, source-bit provenance and pre-IO admission passed','Typed raw hash-only identity, lower memory admission and pre-decoder metadata epoch passed','188 independent derived raw grid statistics references, ROI/core support, hash-only digest and native jobs passed','Independent Python struct/hashlib QRA1 and QRA2 bytes, mixed typed CSV and job replay passed','QRA2 raw grid retention, canonical re-encoding, semantic corruption and fresh replay passed'],
 'stains':['Native scientific float64 OD/stains, validity, captured basis and admission passed','Legacy byte/float32 stain transforms preserve frozen arithmetic across 262144 RGB probes'],
 'log_cell_recipe':['Native physical grid metrics, weighted distances, Gaussian support and admission passed'],
 'tissue_contours':['Native explicit-grid raster eligibility, per-axis metadata, precision and legacy isolation passed'],
 'cells':['Native grid cell request, float64 LoG, metric shape/body, calibrated features and one-read detection passed','32 exact metric LoG signs and 128 rational-distance body expansion references passed'],
 'cell_records':['Independent Python struct/hashlib QCL5 bytes decode and canonical re-encode passed','QCL5 canonical encoding, semantic rejection, captured calibration, record authority and replay passed'],
 'cell_tables':['Independent Python QCR1/QCA5 typed grid source bytes decode and canonical re-encode passed'],
 'viewer_session':['Grid viewer producer copies, owner calibration capture, retention, cancellation and stale delivery passed'],
}
CONDITIONAL=[
 ('project_batch','project','Grid ROI planning, typed batch recipe, per-image calibration, interrupted resume and QCL5/CSV checkpoints passed'),
 ('project_batch','no_project','Grid ROI count planning and tiny scientific centers passed; batch storage unavailable'),
 ('cell_records','project','QCL5 SQLite save, reopen, provenance attachment, replay and second save passed'),
 ('tissue_contours','geos','GEOS explicit-grid edges, ROI holes and anisotropic areas passed'),
 ('cell_records','geos','QCL5 analytic ellipse, polygon hole and winding-policy geometry persistence passed'),
 ('cell_records','no_geos','QCL5 analytic ellipse and winding-policy geometry persistence passed; GEOS polygon case unavailable'),
 ('cell_tables','geos','Grid cell CSV schema 5/6, mixed/manual rows, captured units, QCA5 provenance and upsampled source limits passed'),
 ('cell_tables','no_geos','Grid cell CSV units, mixed natural records and captured calibration passed; classifier publication requires GEOS'),
 ('cell_tables','both','QCA5 mixed grid/natural classifier sources and CSV project save/reopen passed'),
]
# Bind the committed file inventory, including every reference vector/header and
# consumer. Fixture bytes remain raw; code permits only checkout CRLF changes.
FIXTURES=tuple('tests/fixtures/'+name+'/' for name in ('scientific-grid-v1','physical-grid-v1','scientific-channels-v1','grid-cells-v1','grid-cell-archive-v1','grid-cell-consumers-v1','scientific-raster-v1','qupath-cells-0.7.0','qupath-cells-roi-debug-0.7.0','qupath-cells-resampled-debug-0.7.0','raw-scientific-means-v1','raw-scientific-sampling-v1','raw-grid-statistics-v1','raw-grid-archive-v1'))
EXTRA=('docs/scientific-grid-contract.md','tools/ci/run.py','tools/ci/run_manylinux.py','tools/ci/requirements.txt','tools/ci/scientific_grid_evidence.py','tools/ci/scientific-grid-cases.json','tools/ci/accept_unified_engine.py','tools/ci/accept_manylinux.py','tools/ci/accept_macos_native.py','.github/workflows/ci.yml','tools/generate_stubs.py','tests/test_scientific_grid_evidence.py','tests/test_stub_signature_guard.py','tests/test_public_stubs.py',*POSITIVE,*('tests/typing/'+v[0] for v in NEGATIVE.values()))
EXTRA+=('tools/compare_qupath_grid_cells.py','tools/compare_qupath_cells.py',
        'tools/qupath_cells_fixture.py','tools/upstream/qupath_cells_reference.groovy',
        'tests/fixtures/qupath-0.7.0/contract.json')
EXTRA+=('tools/qupath_roi_cell_debug.py','tools/fetch_qupath.py',
        'tools/upstream/qupath_cell_debug_probe.groovy')
EXTRA+=('tools/qupath_resampled_cell_debug.py','tools/ci/run_macos_native.py')

def source_contract(root,commit):
    root=Path(root)
    tree=[name.decode('utf-8') for name in subprocess.check_output(['git','-C',str(root),'ls-tree','-rz','--name-only',commit]).split(b'\0') if name]
    names=sorted(set(EXTRA)|{n for n in tree if n.startswith(('cpp/','cmake/','src/qupathxx/',*FIXTURES)) or n in ('CMakeLists.txt','pyproject.toml') or n in ('tests/'+v+'.py' for v in CASES) or (n.startswith('tools/reference_') and n.endswith('.py'))})
    request=''.join(commit+':'+n+'\n' for n in names).encode()
    stream=subprocess.run(['git','-C',str(root),'cat-file','--batch'],input=request,stdout=subprocess.PIPE,check=True).stdout
    offset=0;hashes={}
    for name in names:
        end=stream.find(b'\n',offset);require(end>=offset,'scientific source response truncated')
        header=stream[offset:end].split();require(len(header)==3 and header[1]==b'blob','scientific source missing: '+name)
        size=int(header[2]);blob=stream[end+1:end+1+size];require(len(blob)==size and stream[end+1+size:end+2+size]==b'\n','scientific source response truncated')
        offset=end+2+size;raw=(root/name).read_bytes();canonical=raw if name.startswith('tests/fixtures/') else raw.replace(b'\r\n',b'\n')
        require(canonical==blob,'scientific source differs: '+name)
        hashes[name]=dict(git_blob_sha256=hashlib.sha256(blob).hexdigest(),checked_out_sha256=hashlib.sha256(raw).hexdigest())
    require(offset==len(stream),'scientific source trailing response')
    return hashes

def native(cases,project=True,geos=True):
    flags=dict(project=project,no_project=not project,geos=geos,no_geos=not geos,both=project and geos)
    contracts={name:[(m,1) for m in markers] for name,markers in BASE.items()}
    for suite,flag,marker in CONDITIONAL: contracts.setdefault(suite,[]).append((marker,int(flags[flag])))
    for suite,markers in contracts.items():
        selected=[c for c in cases if c.get('name')==suite]
        require(len(selected)==1,'scientific native suite missing/duplicate: '+suite)
        case=selected[0];require(all(case.find(t) is None for t in ('failure','error','skipped')),'scientific native suite nonpassing: '+suite)
        lines=case.findtext('system-out','').splitlines()
        for marker,count in markers: require(lines.count(marker)==count,'scientific native capability/marker differs: '+suite+': '+marker)
    return dict(suites=len(contracts),project=project,geos=geos,scientific_acceptance=False)

def python(cases,gui=False):
    passed=skipped=0
    for module,names in CASES.items():
        selected=[c for c in cases if c.get('classname','').rsplit('.',1)[-1]==module]
        require(sorted(c.get('name') for c in selected)==names,'scientific installed cohort missing/duplicate/changed: '+module)
        for c in selected:
            require(all(c.find(t) is None for t in ('failure','error')),'scientific installed case nonpassing')
            skip=c.find('skipped');name=c.get('name')
            if module=='test_grid_cell_frontends' and name in GUI_CASES and not gui:
                require(skip is not None and skip.get('type')=='pytest.skip' and skip.get('message') in GUI_CASES[name],'scientific headless GUI declaration differs')
                skipped+=1
            else:
                require(skip is None,'scientific installed case skipped: '+name);passed+=1
    return dict(passed=passed,declared_headless_gui_skips=skipped,gui=gui,scientific_acceptance=False)

def positive_typing(path):
    raw=Path(path).read_bytes();text=raw.decode('utf-8')
    expected='Success: no issues found in '+str(len(POSITIVE))+' source files'
    require(text.strip()==expected,'positive typing check missing or differs')
    return dict(files=len(POSITIVE),sha256=hashlib.sha256(raw).hexdigest())

def negative_typing(path,filename,lines):
    raw=Path(path).read_bytes();text=raw.decode('utf-8')
    actual=re.findall(r'(\w+\.py):(\d+): error:',text)
    require(sorted((name,int(line)) for name,line in actual)==sorted((filename,line) for line in lines),'scientific negative typing errors differ: '+filename)
    return dict(expected_errors=len(lines),sha256=hashlib.sha256(raw).hexdigest())

def typing(folder):
    folder=Path(folder)
    return dict(positive=positive_typing(folder/'typing.log'),negative={name:negative_typing(folder/name,*spec) for name,spec in NEGATIVE.items()})
