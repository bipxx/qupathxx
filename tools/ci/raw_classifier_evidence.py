"""Exact raw classifier consumer/native cohorts for T5 engineering qualification."""
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import re

spec=importlib.util.spec_from_file_location('qpx_raw_common',Path(__file__).with_name('qptma_evidence.py'))
common=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,'exec',dont_inherit=True),common.__dict__)
require=common.require
ROOT=Path(__file__).resolve().parents[2]
CONTRACT=json.loads(Path(__file__).with_name('raw-classifier-cases.json').read_text(encoding='utf-8'))


def source_paths():
    # All actual compiler inputs plus the transitive consumers and qualification
    # owners. Do not authenticate only the new panel while missing a shared job.
    paths={'CMakeLists.txt','pyproject.toml','.gitattributes','.github/workflows/ci.yml',
           'tools/generate_stubs.py','tools/build_geos_sdk.py','tools/build_opencv_sdk.py',
           # Filename launches/dynamic references are not AST import edges.
           'tests/raw_stream_contour_shutdown_child.py','tests/raw_file_contour_shutdown_child.py',
           'tools/reference_raw_contour_seams.py','tests/fixtures/raw-contour-seams-v1/reference.json',
           # The native spatial suite reads these by filename, outside AST imports.
           'tests/fixtures/qupath-spatial-0.7.0/contract.json','tests/fixtures/qupath-spatial-0.7.0/reference.json',
           'tests/fixtures/qupath-spatial-0.7.0/manifest.json','tools/upstream/qupath_spatial_reference.groovy',
           'tools/capture_qupath_spatial.py',
           'tests/test_spatial_python.py','tests/test_spatial_cli.py','tests/test_spatial_frontends.py','tests/test_document_job_waits.py','tests/test_gui_scale.py','tests/typing/spatial_valid.py','tests/typing/spatial_invalid.py',
           'tools/benchmark_pixel_stream_records.py',
           # The snapshot-reader controls load this binary by filename.
           'tests/data/pixel-streams/rectangle-v1-windows.qps',
           'tools/profile_raw_stream_contours.py','tools/check_tma_large_pyramid.py',
           'tests/test_raw_stream_resource_profile.py','tests/fixtures/raw-stream-resources-v1/contract.json',
           'src/qupathxx/_native.pyi','src/qupathxx/__init__.pyi',
           'tests/test_raw_classifier_evidence.py','tests/test_macos_native_failure_capture.py','tests/test_ci_execution_boundaries.py','tests/test_source_proof_reuse.py','tests/test_ci_installed_diagnostics.py','tests/fixtures/ci-installed-diagnostics-v1/legacy-command-contract.json','docs/reader-license-policy.md',
           'third_party/ProggyForever-LICENSE.txt',
           'tests/test_rgb_nominal_api.py','tests/typing/rgb_nominal_valid.py','tests/typing/rgb_nominal_invalid.py',
           'tests/test_nominal_cell_python.py','tests/test_object_classifier_archives.py','tests/test_object_classifier.py',
           'tests/typing/nominal_cells_valid.py','tests/typing/nominal_cells_invalid.py',
           'tests/test_reader_locale.py','tests/test_nominal_cell_batches.py','tests/test_nominal_cell_frontends.py','tools/compare_qupath_nominal_cells.py','tests/test_qupath_nominal_cells_reference.py','tests/typing/nominal_frontends_valid.py','tests/typing/nominal_frontends_invalid.py','tests/typing/nominal_batches_valid.py','tests/typing/nominal_batches_invalid.py',
           'tools/reference_nominal_cells.py',*('tests/fixtures/nominal-cells-v1/'+name for name in ('manifest.json','source.rgb','processed.rgb','nucleus.u32le','body.u32le'))}
    paths.update(('tools/qupath_gaussian.py','tools/upstream/qupath_gaussian_probe.groovy','tests/test_qupath_gaussian.py','tools/reference_imagej_gaussian.py'))
    paths.update(('tools/qupath_cell_inputs.py','tools/upstream/qupath_cell_inputs_probe.groovy','tests/test_qupath_cell_inputs.py'))
    paths.add('tests/test_imagej_cell_processing.py')
    paths.update(('tools/qupath_cell_policy_edges.py','tools/upstream/qupath_cell_policy_edges.groovy','tests/test_cell_policy_edges.py'))
    for fixture in ('qupath-gaussian-1.54p','qupath-cell-inputs-0.7.0','qupath-cell-policy-edges-0.7.0'):
        paths.update(p.relative_to(ROOT).as_posix() for p in (ROOT/'tests/fixtures'/fixture).rglob('*') if p.is_file())
    for folder,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'}),('tools/ci',{'.py','.json','.txt'})]:
        paths.update(p.relative_to(ROOT).as_posix() for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
    paths.update(CONTRACT['typing_positive']);paths.update('tests/typing/'+name for name in CONTRACT['typing_negative'])
    modules={p.stem:p.relative_to(ROOT).as_posix() for p in (ROOT/'tests').glob('*.py')}
    pending=['tests/'+name+'.py' for name in CONTRACT['installed']];visited=set()
    while pending:
        name=pending.pop()
        if name in visited:continue
        visited.add(name);paths.add(name)
        for node in ast.walk(ast.parse((ROOT/name).read_text(encoding='utf-8'))):
            names=[node.module.split('.')[0]] if isinstance(node,ast.ImportFrom) and node.module else [v.name.split('.')[0] for v in node.names] if isinstance(node,ast.Import) else []
            pending.extend(modules[n] for n in names if n in modules)
    return sorted(paths)


SOURCES=source_paths()


def source_contract(root,commit):
    return common.source_contract(root,commit,SOURCES,label='raw classifier',raw_prefixes=('tests/data/','tests/fixtures/qupath-spatial-0.7.0/','tests/fixtures/nominal-cells-v1/','tests/fixtures/qupath-gaussian-1.54p/','tests/fixtures/qupath-cell-inputs-0.7.0/','tests/fixtures/qupath-cell-policy-edges-0.7.0/'))


def _suite(cases,name,markers):
    selected=[c for c in cases if c.get('name')==name]
    require(len(selected)==1,'raw classifier missing/duplicate native suite: '+name)
    case=selected[0]
    require(all(case.find(tag) is None for tag in ['failure','error','skipped']),'raw classifier native suite failed: '+name)
    lines=case.findtext('system-out','').splitlines()
    require(all(lines.count(marker)==1 for marker in markers),'raw classifier native semantics missing/duplicate: '+name)
    return lines


def native(cases,full=True):
    require(type(full) is bool,'raw classifier capability must be bool')
    for name,markers in CONTRACT['native_base'].items():_suite(cases,name,markers)
    if full:
        for name,markers in CONTRACT['native_full'].items():
            lines=_suite(cases,name,markers)
            require(not any(m in lines for values in CONTRACT['native_disabled'].values() for m in values),'raw classifier enabled profile claims disabled capability')
    else:
        for name,markers in CONTRACT['native_disabled'].items():
            lines=_suite(cases,name,markers)
            require(not any(m in lines for m in CONTRACT['native_full'].get(name,[])),'raw classifier minimal profile claims full viewer behavior')
        lines=_suite(cases,'pixel_workflows',CONTRACT['native_base']['pixel_workflows'])
        require(not any(m in lines for m in CONTRACT['native_full']['pixel_workflows'] if m.startswith('Raw pixel backend training:')),'raw classifier minimal profile claims backend training')
    return dict(full=full,portable_owner_jobs=True,raw_file_training_contour_consumers=full)


def python(cases,gui=False):
    require(type(gui) is bool,'raw classifier GUI capability must be bool')
    groups=[];total_passed=total_skipped=0
    off=CONTRACT['training_off_skip']
    for module,names in CONTRACT['installed'].items():
        selected=[case for case in cases if case.get('classname')=='tests.'+module]
        require(sorted(case.get('name') for case in selected)==names,'raw classifier installed identities missing/duplicate/changed: '+module)
        passed=skipped=0
        for case in selected:
            require(all(case.find(tag) is None for tag in ['failure','error']),'raw classifier installed case failed: '+module)
            desktop=CONTRACT['gui_only'].get(module)
            expected=desktop['attributes'] if not gui and desktop and case.get('name') in desktop['names'] else off['attributes'] if module==off['module'] and case.get('name')==off['name'] else None
            skips=case.findall('skipped')
            if expected is None:require(not skips,'raw classifier required behavior skipped: '+str(case.get('name')));passed+=1
            else:require(len(skips)==1 and dict(skips[0].attrib)==expected,'raw classifier capability skip differs: '+str(case.get('name')));skipped+=1
        groups.append(dict(module=module,passed=passed,declared_skips=skipped));total_passed+=passed;total_skipped+=skipped
    return dict(passed=total_passed,declared_skips=total_skipped,groups=groups)


def typing(folder):
    folder=Path(folder);positive=folder/CONTRACT['typing_positive_log'];text=positive.read_text(encoding='utf-8')
    require(text.strip()=='Success: no issues found in '+str(len(CONTRACT['typing_positive']))+' source files','raw classifier positive typing missing/nonempty count differs')
    hashes={positive.name:hashlib.sha256(positive.read_bytes()).hexdigest()};errors=0
    for filename,contract in CONTRACT['typing_negative'].items():
        path=folder/contract['log'];text=path.read_text(encoding='utf-8')
        actual=sorted((name,int(line)) for name,line in re.findall(r'([A-Za-z0-9_]+\.py):(\d+): error:',text))
        require(actual==[(filename,line) for line in contract['lines']],'raw classifier negative typing identities differ: '+filename)
        summary='Found '+str(len(actual))+' errors in 1 file (checked 1 source file)'
        require(len(re.findall('^'+re.escape(summary)+'$',text,re.M))==1,'raw classifier negative typing analyzed-file count differs')
        hashes[path.name]=hashlib.sha256(path.read_bytes()).hexdigest();errors+=len(actual)
    return dict(positive_files=len(CONTRACT['typing_positive']),negative_files=len(CONTRACT['typing_negative']),expected_errors=errors,hashes=hashes)
