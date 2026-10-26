"""Exact multi-file OME engineering evidence; no biological/release acceptance."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re

spec=importlib.util.spec_from_file_location('qpx_multifile_common',Path(__file__).with_name('qptma_evidence.py'))
common=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,"exec",dont_inherit=True),common.__dict__)
require=common.require
ROOT=Path(__file__).resolve().parents[2]
CONTRACT=json.loads(Path(__file__).with_name('multifile-ome-cases.json').read_text(encoding='utf-8'))
SOURCES=[
 'cpp/tiff_server.cpp','cpp/raw_image.cpp','cpp/include/qupathxx/raw_image.hpp',
 'cpp/source_identity.cpp','cpp/include/qupathxx/source_identity.hpp','cpp/reader_file_guard.hpp',
 'cpp/file_fingerprint.cpp','cpp/include/qupathxx/file_fingerprint.hpp','cpp/provenance_codec.cpp',
 'cpp/raw_analysis.cpp','cpp/raw_analysis_records.cpp','cpp/raw_analysis_archive.cpp',
 'cpp/include/qupathxx/raw_analysis.hpp','cpp/project.cpp','cpp/project_save.cpp','cpp/project_schema.hpp',
 'cpp/project_batch.cpp','cpp/batch_main.cpp','cpp/region_main.cpp','cpp/raw_tables.cpp',
 'cpp/raw_image_bindings.cpp','cpp/raw_analysis_bindings.cpp','cpp/provenance_bindings.cpp',
 'cpp/gui_raw.hpp','cpp/gui.cpp','cpp/viewer_session.cpp','cpp/viewer_bindings.cpp',
 'cpp/tests/raw_image_tests.cpp','cpp/tests/source_identity_tests.cpp','cpp/tests/scientific_raw_sampling_tests.cpp',
 'src/qupathxx/__init__.py','src/qupathxx/_native.pyi','src/qupathxx/__init__.pyi',
 'tools/reference_multifile_ome.py','tests/test_multifile_ome_evidence.py',
 *['tests/'+name+'.py' for name in CONTRACT['installed']],
 'tests/typing/source_companions_valid.py','tests/typing/source_companions_invalid.py',
 'tools/ci/multifile_ome_evidence.py','tools/ci/multifile-ome-cases.json','tools/ci/qptma_evidence.py',
 'tools/ci/run.py','tools/ci/run_manylinux.py','tools/ci/accept_unified_engine.py',
 'tools/ci/accept_manylinux.py','tools/ci/accept_macos_native.py',
 'CMakeLists.txt','cmake/TiffReader.cmake','.github/workflows/ci.yml','docs/reader-license-policy.md',
 *sorted(p.relative_to(ROOT).as_posix() for p in (ROOT/'docs/benchmarks/multifile-ome-preparation/fixture').rglob('*') if p.is_file())]


def source_contract(root,commit):
    return common.source_contract(root,commit,SOURCES,label='multi-file OME',
        raw_prefixes=('tests/fixtures/','docs/benchmarks/multifile-ome-preparation/fixture/'))


def native(cases,tiff=True):
    expected={**CONTRACT['native'],CONTRACT['tiff_suite']:[CONTRACT['tiff_marker'] if tiff else CONTRACT['tiff_disabled_marker']]}
    for suite,markers in expected.items():
        selected=[case for case in cases if case.get('name')==suite]
        require(len(selected)==1,'multi-file OME missing/duplicate native suite: '+suite)
        case=selected[0]
        require(all(case.find(tag) is None for tag in ['failure','error','skipped']),'multi-file OME native suite did not pass: '+suite)
        lines=case.findtext('system-out','').splitlines()
        require(all(lines.count(marker)==1 for marker in markers),'multi-file OME native semantics missing/duplicate: '+suite)
        if suite==CONTRACT['tiff_suite']:
            opposite=CONTRACT['tiff_disabled_marker'] if tiff else CONTRACT['tiff_marker']
            require(opposite not in lines,'multi-file OME native capability contradicts configuration')
    return dict(suites=len(expected),tiff_enabled=tiff,compound_provenance_codecs=True)


def desktop_member(case):
    module=case.get('classname','').rsplit('.',1)[-1]
    return case.get('name') in CONTRACT['desktop'].get(module,[])


def python(cases):
    # Installed and desktop tests intentionally share a module. Remove only the
    # exact declared desktop identities; unknown substitutions still reject.
    return common.python([case for case in cases if not desktop_member(case)],
        CONTRACT['installed'],label='multi-file OME installed cohort')


def desktop(cases,gui=False):
    selected=[case for case in cases if desktop_member(case)]
    expected=sorted((module,name) for module,names in CONTRACT['desktop'].items() for name in names)
    actual=sorted((case.get('classname','').rsplit('.',1)[-1],case.get('name')) for case in selected)
    require(actual==expected,'multi-file OME desktop cohort missing/duplicate/changed')
    for case in selected:
        require(all(case.find(tag) is None for tag in ['failure','error']),'multi-file OME desktop case failed')
        skips=case.findall('skipped')
        if gui:require(not skips,'multi-file OME required desktop case skipped')
        else:require(len(skips)==1 and dict(skips[0].attrib)==CONTRACT['headless_skip'],'multi-file OME headless declaration differs')
    return dict(passed=len(selected) if gui else 0,declared_desktop_skips=0 if gui else len(selected))


def typing(folder):
    folder=Path(folder);positive=folder/CONTRACT['typing_positive_log'];text=positive.read_text(encoding='utf-8')
    require(len(re.findall(r'^Success: no issues found in 1 source file\s*$',text,re.M))==1 and ': error:' not in text,'multi-file OME positive typing differs')
    hashes={positive.name:hashlib.sha256(positive.read_bytes()).hexdigest()};total=0
    for filename,contract in CONTRACT['typing_negative'].items():
        path=folder/contract['log'];text=path.read_text(encoding='utf-8')
        errors=list(map(int,re.findall(r'(?:^|[\\/])'+re.escape(filename)+r':(\d+): error:',text,re.M)))
        require(sorted(errors)==contract['lines'] and len(re.findall(r': error:',text))==len(errors),'multi-file OME negative typing differs: '+filename)
        hashes[path.name]=hashlib.sha256(path.read_bytes()).hexdigest();total+=len(errors)
    return dict(positive_files=1,intended_negative_errors=total,log_sha256=hashes)
