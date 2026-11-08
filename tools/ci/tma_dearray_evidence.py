"""Named dearraying evidence, including calibration; no biological acceptance."""
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re

spec=importlib.util.spec_from_file_location('qpx_dearray_common',Path(__file__).with_name('qptma_evidence.py'))
common=importlib.util.module_from_spec(spec);exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),common.__dict__)
require=common.require
CONTRACT=json.loads(Path(__file__).with_name('tma-dearray-cases.json').read_text(encoding='utf-8'))
ROOT=Path(__file__).resolve().parents[2]
SOURCES=['cpp/tma_dearray.cpp','cpp/include/qupathxx/tma_dearray.hpp','cpp/tests/tma_dearray_tests.cpp',
 'cpp/tma_dearray_cli.cpp','cpp/tma_dearray_cli.hpp','cpp/batch_main.cpp','cpp/tma_bindings.cpp',
 'cpp/gui_tma.hpp','cpp/gui.cpp','cpp/gui_replay.hpp','cpp/bindings.cpp','cpp/include/qupathxx/core.hpp',
 'cpp/viewer_session.cpp','cpp/include/qupathxx/viewer_session.hpp','cpp/viewer_bindings.cpp',
 'cpp/core.cpp','cpp/document_edits.cpp','cpp/thumbnail.cpp','cpp/resampling.cpp','cpp/qptma.cpp',
 'src/qupathxx/__init__.py','src/qupathxx/__init__.pyi','src/qupathxx/_native.pyi',
 *['tests/'+name+'.py' for name in [*CONTRACT['installed'],*CONTRACT['desktop']]],
 'tests/test_tma_dearray_evidence.py','tests/typing/tma_dearray_valid.py',
 *['tests/typing/'+name for name in CONTRACT['typing_negative']],
 'tools/reference_tma_dearray.py','tools/assess_tma_dearray_reference.py',
 'tools/ci/tma_dearray_evidence.py','tools/ci/tma-dearray-cases.json','tools/ci/qptma_evidence.py',
 'tools/ci/run.py','tools/ci/accept_unified_engine.py','tools/ci/accept_macos_native.py',
 'tools/ci/accept_manylinux.py','tools/ci/run_manylinux.py','tools/generate_stubs.py',
 'CMakeLists.txt','.github/workflows/ci.yml',
 *sorted(p.relative_to(ROOT).as_posix() for p in (ROOT/'tests/fixtures/tma-dearray-v1').iterdir() if p.is_file())]


def source_contract(root,commit):
    return common.source_contract(root,commit,SOURCES,label='TMA dearray')


def native(cases,project=True):
    contract=CONTRACT['native'];selected=[c for c in cases if c.get('name')==contract['suite']]
    require(len(selected)==1,'TMA dearray missing/duplicate native suite');case=selected[0]
    require(all(case.find(t) is None for t in ['failure','error','skipped']),'TMA dearray native suite did not pass')
    lines=case.findtext('system-out','').splitlines();fixture_lines=[line for line in lines if line.startswith('TMA_DEARRAY_FIXTURE ')]
    require(len(fixture_lines)==len(contract['fixtures']),'TMA dearray fixture coverage differs');observed={}
    for line in fixture_lines:
        match=re.fullmatch(r'TMA_DEARRAY_FIXTURE (\w+) max_center_error_pixels=([-+.\deE]+) missing_flags=match',line)
        require(match is not None,'TMA dearray malformed geometry/missing flags')
        name,error=match.group(1),float(match.group(2))
        require(name not in observed and name in contract['fixtures'],'TMA dearray duplicate/unknown geometry fixture')
        require(math.isfinite(error) and 0<=error<=contract['center_tolerance_pixels'],'TMA dearray independent geometry tolerance exceeded');observed[name]=error
    require(set(observed)==set(contract['fixtures']),'TMA dearray geometry fixture set differs')
    require(all(lines.count(marker)==1 for marker in contract['markers']),'TMA dearray native semantics missing/duplicate')
    if project:
        require(all(lines.count(marker)==1 for marker in contract['project_markers']) and contract['project_disabled_marker'] not in lines,'TMA dearray required owner/CLI semantics missing/disabled/duplicate')
    else:
        require(lines.count(contract['project_disabled_marker'])==1 and all(marker not in lines for marker in contract['project_markers']),'TMA dearray disabled-project declaration differs')
    return dict(fixture_cases=len(observed),center_tolerance_pixels=contract['center_tolerance_pixels'],maximum_center_error_pixels=max(observed.values()),missing_flags_match=True,physical_units=True,pixel_recipe_preserved=True,project_workflows_covered=project)


def python(cases):
    return common.python(cases,CONTRACT['installed'],label='TMA dearray installed cohort')


def desktop(cases,gui=False):
    for module,names in CONTRACT['desktop'].items():
        selected=[c for c in cases if c.get('classname','').rsplit('.',1)[-1]==module]
        require(sorted(c.get('name') for c in selected)==names,'TMA dearray desktop cohort missing/duplicate/changed')
        for case in selected:
            require(all(case.find(t) is None for t in ['failure','error']),'TMA dearray desktop case failed')
            skips=case.findall('skipped')
            if gui:require(not skips,'TMA dearray required desktop case skipped')
            else:require(len(skips)==1 and dict(skips[0].attrib)==CONTRACT['headless_skip'],'TMA dearray headless declaration differs')
    count=sum(map(len,CONTRACT['desktop'].values()))
    return dict(passed=count if gui else 0,declared_desktop_skips=0 if gui else count)


def typing(folder):
    folder=Path(folder);positive=folder/CONTRACT['typing_positive_log'];text=positive.read_text(encoding='utf-8')
    require(len(re.findall(r'^Success: no issues found in 1 source file\s*$',text,re.M))==1 and ': error:' not in text,'TMA dearray positive typing differs')
    hashes={positive.name:hashlib.sha256(positive.read_bytes()).hexdigest()};total=0
    for filename,contract in CONTRACT['typing_negative'].items():
        path=folder/contract['log'];text=path.read_text(encoding='utf-8')
        errors=list(map(int,re.findall(r'(?:^|[\\/])'+re.escape(filename)+r':(\d+): error:',text,re.M)))
        require(sorted(errors)==contract['lines'] and len(re.findall(r': error:',text))==len(errors),'TMA dearray negative typing differs: '+filename)
        hashes[path.name]=hashlib.sha256(path.read_bytes()).hexdigest();total+=len(errors)
    return dict(positive_files=1,intended_negative_errors=total,log_sha256=hashes)
