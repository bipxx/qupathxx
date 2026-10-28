"""Named scientific-grid engineering cohorts; no biological/release acceptance."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess

spec=importlib.util.spec_from_file_location('qpx_grid_common',Path(__file__).with_name('qptma_evidence.py'))
common=importlib.util.module_from_spec(spec);exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),common.__dict__)
require=common.require
CASES=json.loads(Path(__file__).with_name('scientific-grid-cases.json').read_text(encoding='utf-8'))
GUI_CASES={
 'test_rgb_nominal_preview_renders_native_pixels_in_imgui':('opt-in RGB nominal preview',),
 'test_reduced_budget_imgui_measure_keep_and_render':('requires quantitative project desktop','opt-in raw grid desktop'),
 'test_reduced_budget_managed_viewer_retains_verifies_and_saves':('requires quantitative project desktop','opt-in raw grid desktop'),
 'test_raw_grid_desktop_measure_keep_csv_and_render':('requires quantitative project desktop','opt-in raw grid desktop'),
 'test_raw_grid_managed_producer_retains_verifies_and_saves':('requires quantitative project desktop','opt-in raw grid desktop'),
 'test_raw_grid_desktop_batch_matches_native_recipe_and_resume':('requires quantitative project desktop','opt-in raw grid desktop'),
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
POSITIVE.append('tests/typing/rgb_nominal_valid.py')
POSITIVE.append('examples/rgb_nominal_preview.py')
POSITIVE.append('tests/typing/nominal_cells_valid.py')
NEGATIVE['rgb-nominal-typing-negative.log']=('rgb_nominal_invalid.py',[4,5,7])
NEGATIVE['nominal-cells-typing-negative.log']=('nominal_cells_invalid.py',[4,5,6,7,7,7])
BASE={
 'resampling':['Streamed raw materialization exact means, pinned chunk bits/support, cancellation, admission and dense fallback passed','Streamed raw digest canonical HWC/validity hashes, bounded chunks, metadata epoch, late faults and admission passed','Scientific raster precision admission agrees with exhaustive binary64 sample checks','Native scientific grid exact geometry, rounding, context and admission passed','Native scientific RGB float64 exact means, support, reader and cancellation checks passed','Native exact typed weighted means, subnormal/cancellation, atomic rejection and nearest-even rounding passed','Native typed scientific raw sampling, per-channel finite/support, source-bit provenance and pre-IO admission passed','Typed raw hash-only identity, lower memory admission and pre-decoder metadata epoch passed','188 independent derived raw grid statistics references, ROI/core support, hash-only digest and native jobs passed','Independent Python struct/hashlib QRA1 and QRA2 bytes, mixed typed CSV and job replay passed','QRA2 raw grid retention, canonical re-encoding, semantic corruption and fresh replay passed','Shared ROI grid raw count/context planning and pre-IO admission passed','Raw grid captured ROI/context workspace, exact budget boundary, hash-only lower admission and single metadata epoch passed','QRA2 private eligible-count authority, changed-mask rejection, checksum-valid count corruption and historical budget compatibility passed'],
 'stains':['Native scientific float64 OD/stains, validity, captured basis and admission passed','Legacy byte/float32 stain transforms preserve frozen arithmetic across 262144 RGB probes'],
 'log_cell_recipe':['Native physical grid metrics, weighted distances, Gaussian support and admission passed'],
 'tissue_contours':['Native explicit-grid raster eligibility, per-axis metadata, precision and legacy isolation passed'],
 'cells':['Native grid cell request, float64 LoG, metric shape/body, calibrated features and one-read detection passed','32 exact metric LoG signs and 128 rational-distance body expansion references passed'],
 'cell_records':['Independent Python struct/hashlib QCL5 bytes decode and canonical re-encode passed','QCL5 canonical encoding, semantic rejection, captured calibration, record authority and replay passed'],
 'cell_tables':['Independent Python QCR1/QCA5 typed grid source bytes decode and canonical re-encode passed'],
 'viewer_session':['Grid viewer producer copies, owner calibration capture, retention, cancellation and stale delivery passed'],
}
BASE['resampling'].append('99 pinned RGB nominal reads, fixed-point bytes, global crop context, admission and cancellation passed')
BASE['resampling'].append('99 public RGB nominal reader hashes, exact halo IO, closed-reader ownership and decoder/cancellation rejection passed')
BASE['resampling'].append('99 RGB nominal captured outputs stitch at three tile sizes with unchanged context cores; six negative-context translations and admission passed')
BASE['cells'].append('Original-source nominal cell captured RGB/masks/features, QCL6 canonical history, replay, calibration, budgets and cancellation passed')
BASE['cells'].append('Native nominal ROI bounds factory and versioned nominal cell CSV descriptors passed')
CONDITIONAL=[
 ('raw_image','tiff','TIFF interleaved block reuse without LRU, separate-plane selection and padded values passed'),
 ('viewer_session','tiff','Raw grid viewer copied counts/options, owner calibration, retention/replay, cancellation and stale delivery passed'),
 ('raw_image','tiff','Actual UInt16 TIFF scientific grid analysis, ROI upsampling and hash-only digest passed'),
 ('raw_image','tiff','Independent deflate TIFF typed grid analysis through prefixed zlib passed'),
 ('raw_image','raw_project','QRA2 TIFF grid SQLite save/reopen, explicit replay and second save passed'),
 ('raw_image','raw_project','Raw grid TIFF batch distinct recipe, interrupted checkpoint resume and hash-only typed CSV passed'),
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
FIXTURES=tuple('tests/fixtures/'+name+'/' for name in ('scientific-grid-v1','physical-grid-v1','scientific-channels-v1','grid-cells-v1','grid-cell-archive-v1','grid-cell-consumers-v1','scientific-raster-v1','qupath-cells-0.7.0','qupath-cells-roi-debug-0.7.0','qupath-cells-resampled-debug-0.7.0','qupath-rgb-resampling-0.7.0','qupath-rgb-resampling-holdout-0.7.0','rgb-nominal-v1','nominal-cells-v1','raw-scientific-means-v1','raw-scientific-sampling-v1','raw-grid-statistics-v1','raw-grid-archive-v1'))
EXTRA=('docs/scientific-grid-contract.md','tools/ci/run.py','tools/ci/run_manylinux.py','tools/ci/requirements.txt','tools/ci/scientific_grid_evidence.py','tools/ci/scientific-grid-cases.json','tools/ci/accept_unified_engine.py','tools/ci/accept_manylinux.py','tools/ci/accept_macos_native.py','.github/workflows/ci.yml','tools/generate_stubs.py','tests/test_scientific_grid_evidence.py','tests/test_stub_signature_guard.py','tests/test_public_stubs.py',*POSITIVE,*('tests/typing/'+v[0] for v in NEGATIVE.values()))
EXTRA+=('tools/compare_qupath_grid_cells.py','tools/compare_qupath_cells.py',
        'tools/qupath_cells_fixture.py','tools/upstream/qupath_cells_reference.groovy',
        'tests/fixtures/qupath-0.7.0/contract.json')
EXTRA+=('tools/qupath_roi_cell_debug.py','tools/fetch_qupath.py',
        'tools/upstream/qupath_cell_debug_probe.groovy')
EXTRA+=('tools/qupath_resampled_cell_debug.py','tools/ci/run_macos_native.py')
EXTRA+=('tests/test_object_classifier_archives.py','tests/test_object_classifier.py')
EXTRA+=('tools/qupath_rgb_resampling.py','tools/upstream/qupath_rgb_resampling.groovy')

EXTRA+=('tools/ci/tma_axis_evidence.py','tools/ci/tma-axis-cases.json','tests/test_tma_axis_evidence.py','tests/test_tma_axes.py','tests/test_gui_tma.py','tests/typing/tma_axes_invalid.py','tools/ci/qptma_evidence.py','tests/test_qptma_evidence.py')

def source_contract(root,commit):
    root=Path(root)
    tree=[name.decode('utf-8') for name in subprocess.check_output(['git','-C',str(root),'ls-tree','-rz','--name-only',commit]).split(b'\0') if name]
    names=sorted(set(EXTRA)|{n for n in tree if n.startswith(('cpp/','cmake/','src/qupathxx/',*FIXTURES)) or n in ('CMakeLists.txt','pyproject.toml') or n in ('tests/'+v+'.py' for v in CASES) or (n.startswith('tools/reference_') and n.endswith('.py'))})
    return common.source_contract(root,commit,names,label='scientific')


def native(cases,project=True,geos=True,tiff=True):
    flags=dict(project=project,no_project=not project,geos=geos,no_geos=not geos,both=project and geos,tiff=tiff,raw_project=project and tiff)
    contracts={name:[(m,1) for m in markers] for name,markers in BASE.items()}
    for suite,flag,marker in CONDITIONAL: contracts.setdefault(suite,[]).append((marker,int(flags[flag])))
    for suite,markers in contracts.items():
        selected=[c for c in cases if c.get('name')==suite]
        require(len(selected)==1,'scientific native suite missing/duplicate: '+suite)
        case=selected[0];require(all(case.find(t) is None for t in ('failure','error','skipped')),'scientific native suite nonpassing: '+suite)
        lines=case.findtext('system-out','').splitlines()
        for marker,count in markers: require(lines.count(marker)==count,'scientific native capability/marker differs: '+suite+': '+marker)
    return dict(suites=len(contracts),project=project,geos=geos,tiff=tiff,scientific_acceptance=False)

def python(cases,gui=False):
    passed=skipped=0
    for module,names in CASES.items():
        selected=[c for c in cases if c.get('classname','').rsplit('.',1)[-1]==module]
        require(sorted(c.get('name') for c in selected)==names,'scientific installed cohort missing/duplicate/changed: '+module)
        for c in selected:
            require(all(c.find(t) is None for t in ('failure','error')),'scientific installed case nonpassing')
            skip=c.find('skipped');name=c.get('name')
            if name in GUI_CASES and not gui:
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
