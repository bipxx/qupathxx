"""Exact-source hierarchy/export/recovery cohorts; reuse existing small checks."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

spec=importlib.util.spec_from_file_location('qpx_hierarchy_common',Path(__file__).with_name('qptma_evidence.py'))
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
require=common.require
CASES=json.loads(Path(__file__).with_name('hierarchy-cases.json').read_text(encoding='utf-8'))
GUI='test_gui_hierarchy_migration'
MARKER='Native nested GeoJSON hierarchy, nucleus, flat compatibility and depth bounds passed'
PATH_MARKER='Native physical path adaptation preserves logical names and UNC/drive policy passed'
RECOVERY_MARKER='Native async snapshot isolation, primary preservation and restoration passed'
SOURCES=['cpp/interchange.cpp','cpp/include/qupathxx/interchange.hpp','cpp/filesystem_access.hpp',
 'cpp/project.cpp','cpp/project_recovery.cpp','cpp/project_batch.cpp','cpp/include/qupathxx/project.hpp',
 'cpp/include/qupathxx/project_recovery.hpp','cpp/include/qupathxx/core.hpp','cpp/gui.cpp','cpp/bindings.cpp',
 'cpp/tests/interchange_tests.cpp','cpp/tests/recovery_tests.cpp','CMakeLists.txt',
 'src/qupathxx/_native.pyi','src/qupathxx/__init__.py','src/qupathxx/__init__.pyi',
 'tools/upstream/qupath_hierarchy_reference.groovy','tools/compare_qupath_hierarchy.py','tools/fetch_qupath.py',
 'tools/ci/hierarchy_evidence.py','tools/ci/hierarchy-cases.json','tools/ci/qptma_evidence.py',
 'tools/ci/accept_unified_engine.py','tools/ci/accept_macos_native.py','tools/ci/accept_manylinux.py',
 'tools/ci/run.py','tools/generate_stubs.py','.github/workflows/ci.yml',
 'tests/typing/hierarchy_valid.py','tests/typing/hierarchy_invalid.py',
 'docs/benchmarks/annotation-migration-recovery-local-windows.json',
 *['docs/benchmarks/annotation-migration-recovery-local/python-roundtrip/'+name for name in ['native-nested.geojson','report.json','qupath/reference.json','qupath.log']],
 *['tests/'+name+'.py' for name in CASES],
 *['tests/fixtures/qupath-hierarchy-0.7.0/'+name for name in ['manifest.json','contract.json',
   'hierarchy.json','nested.geojson','flat.geojson','losses.geojson','metadata-raw.geojson','reference.json',
   'capture.log','native-export.geojson','native-roundtrip.json','native-roundtrip.log']]]


def source_contract(root,commit):
    return common.source_contract(root,commit,SOURCES,'hierarchy',('tests/fixtures/','docs/benchmarks/annotation-migration-recovery-local/'))


def native(cases,enabled=True):
    for name,markers in [('interchange',[MARKER]),('recovery',[PATH_MARKER,RECOVERY_MARKER])]:
        found=[c for c in cases if c.get('name')==name]
        require(len(found)==1,'hierarchy native suite missing/duplicate: '+name)
        case=found[0]
        require(all(case.find(t) is None for t in ['failure','error','skipped']),'hierarchy native suite failed or skipped: '+name)
        lines=case.findtext('system-out','').splitlines()
        for marker in markers:
            # Path adaptation is tested even in the intentional minimal build.
            expected=1 if enabled or marker==PATH_MARKER else 0
            require(lines.count(marker)==expected,'hierarchy native capability/marker differs: '+name)
    return dict(hierarchy_enabled=enabled,hierarchy_qualified=int(enabled),physical_path_policy=True,
                recovery_storage_qualified=enabled)


def python(cases,gui=False):
    contract={name:values for name,values in CASES.items() if gui or name!=GUI}
    return common.python(cases,contract,'hierarchy installed cohort')


def typing_errors(path):
    data=Path(path).read_bytes()
    require(sorted(map(int,re.findall(r'hierarchy_invalid\.py:(\d+): error:',data.decode('utf-8'))))==[4,5,7],
            'hierarchy negative typing errors differ')
    return dict(expected_errors=3,sha256=hashlib.sha256(data).hexdigest())
