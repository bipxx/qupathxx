"""Require exact-source native and installed arbitrary-grid resampling cohorts."""
import importlib.util
from pathlib import Path

spec=importlib.util.spec_from_file_location('qpx_resampling_common',Path(__file__).with_name('qptma_evidence.py'))
common=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),common.__dict__)

SOURCES=['CMakeLists.txt','cpp/resampling.cpp','cpp/include/qupathxx/resampling.hpp',
         'cpp/thumbnail.cpp','cpp/region_main.cpp','cpp/bindings.cpp','cpp/tests/resampling_tests.cpp',
         'src/qupathxx/__init__.py','src/qupathxx/__init__.pyi','src/qupathxx/_native.pyi',
         'tests/test_resampling.py','tests/test_resampling_evidence.py','tools/ci/resampling_evidence.py',
         'tools/ci/accept_unified_engine.py','tools/ci/accept_manylinux.py','tools/ci/accept_macos_native.py',
         'tools/ci/qptma_evidence.py']
PYTHON_NAMES=sorted([
    *[f'test_fractional_downsample_enlargement_anisotropic_and_identity[source_shape{i}-output{i}]' for i in range(5)],
    'test_padded_region_and_buffer_outlives_closed_server',
    *[f'test_invalid_output_rejects[{pair}]' for pair in ['0-1','1--1','67108865-1','4611686018427387904-4611686018427387904']],
    'test_excessive_cross_axis_work_rejects_before_huge_decode','test_closed_server_and_invalid_level',
    'test_standalone_cli_resampling_pixels_header_and_input_protection'])
MARKER='2205 independent oracle shapes and admission/alpha/reader checks passed'


def require(value,message):
    if not value:raise ValueError(message)


def source_contract(root,commit):
    hashes=common.source_contract(root,commit,SOURCES,label='resampling')
    # Preserve the existing receipt shape while sharing strict framed reads.
    for observation in hashes.values():
        observation['crlf_normalized']=observation['git_blob_sha256']!=observation['checked_out_sha256']
    return hashes


def native(cases):
    selected=[case for case in cases if case.get('name')=='resampling']
    require(len(selected)==1,'missing/duplicate native resampling suite')
    case=selected[0]
    require(all(case.find(tag) is None for tag in ['failure','error','skipped']),'native resampling did not pass')
    require(case.findtext('system-out','').splitlines().count(MARKER)==1,'native resampling oracle cohort differs')
    return dict(passed=1,independent_oracle_shapes=2205)


def python(cases):
    selected=[case for case in cases if case.get('classname','').rsplit('.',1)[-1]=='test_resampling']
    require(sorted(case.get('name') for case in selected)==PYTHON_NAMES,'installed resampling cohort missing/duplicate/changed')
    require(all(case.find(tag) is None for case in selected for tag in ['failure','error','skipped']),
            'installed resampling case failed or skipped')
    return dict(passed=len(selected),cases=PYTHON_NAMES)
