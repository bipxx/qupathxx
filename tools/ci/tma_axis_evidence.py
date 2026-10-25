"""Named TMA structural-edit evidence; never biological or release acceptance."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

spec=importlib.util.spec_from_file_location('qpx_tma_axis_common',Path(__file__).with_name('qptma_evidence.py'))
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
require=common.require
CONTRACT=json.loads(Path(__file__).with_name('tma-axis-cases.json').read_text(encoding='utf-8'))
SOURCES=['cpp/tma.cpp','cpp/tma_bindings.cpp','cpp/tma_internal.hpp','cpp/include/qupathxx/tma.hpp',
 'cpp/include/qupathxx/core.hpp','cpp/document_edits.cpp','cpp/tests/tma_tests.cpp',
 'cpp/include/qupathxx/viewer_session.hpp','cpp/viewer_session.cpp','cpp/viewer_bindings.cpp','cpp/tests/viewer_session_tests.cpp',
 'cpp/gui_tma.hpp','cpp/gui.cpp','cpp/gui_replay.hpp','cpp/bindings.cpp',
 'src/qupathxx/_native.pyi','src/qupathxx/__init__.py','src/qupathxx/__init__.pyi',
 'tests/test_tma_axes.py','tests/test_gui_tma.py','tests/test_tma_axis_evidence.py',
 'tests/typing/valid_api.py','tests/typing/tma_axes_invalid.py',
 'tools/ci/tma_axis_evidence.py','tools/ci/tma-axis-cases.json','tools/ci/qptma_evidence.py',
 'tools/ci/scientific_grid_evidence.py','tools/ci/accept_unified_engine.py',
 'tools/ci/accept_manylinux.py','tools/ci/accept_macos_native.py','tools/ci/run.py',
 'tools/ci/run_manylinux.py','tools/generate_stubs.py','CMakeLists.txt','.github/workflows/ci.yml']
TYPING_LOG='tma-axis-typing-negative.log'


def source_contract(root,commit):
    return common.source_contract(root,commit,SOURCES,label='TMA axis')


def native(cases):
    for suite,markers in CONTRACT['native'].items():
        selected=[c for c in cases if c.get('name')==suite]
        require(len(selected)==1,'TMA axis missing/duplicate native suite: '+suite)
        case=selected[0]
        require(all(case.find(t) is None for t in ['failure','error','skipped']),'TMA axis native suite did not pass: '+suite)
        lines=case.findtext('system-out','').splitlines()
        require(all(lines.count(m)==1 for m in markers),'TMA axis native semantics missing/duplicate: '+suite)
    return dict(suites=len(CONTRACT['native']),axis_edits_and_owner_commands=True)


def python(cases):
    return common.python(cases,CONTRACT['installed'],label='TMA axis installed cohort')


def desktop(cases,gui=False):
    for classname,names in CONTRACT['desktop'].items():
        selected=[c for c in cases if c.get('classname','').rsplit('.',1)[-1]==classname]
        require(sorted(c.get('name') for c in selected)==names,'TMA axis desktop cohort missing/duplicate/changed')
        for case in selected:
            require(all(case.find(t) is None for t in ['failure','error']),'TMA axis desktop case failed')
            skips=case.findall('skipped')
            if gui:require(not skips,'TMA axis required desktop case skipped')
            else:require(len(skips)==1 and skips[0].get('type')=='pytest.skip' and skips[0].get('message')=='opt-in TMA desktop','TMA axis headless desktop declaration differs')
    count=sum(map(len,CONTRACT['desktop'].values()))
    return dict(passed=count if gui else 0,declared_desktop_skips=0 if gui else count)


def typing(path):
    raw=Path(path).read_bytes();text=raw.decode('utf-8')
    errors=re.findall(r'tma_axes_invalid\.py:(\d+): error:',text)
    require(sorted(map(int,errors))==[4,5,6],'TMA axis negative typing errors differ')
    require(len(re.findall(r': error:',text))==3,'TMA axis unexpected typing diagnostics')
    return dict(expected_errors=3,sha256=hashlib.sha256(raw).hexdigest())
