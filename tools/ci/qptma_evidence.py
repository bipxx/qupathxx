"""Require source-bound native and installed QPTMA correctness cohorts."""
import hashlib
import json
import re
from pathlib import Path
import subprocess
SOURCES=['cpp/project.cpp','cpp/include/qupathxx/project.hpp','cpp/qptma_project_cli.cpp','cpp/qptma_project_cli.hpp','cpp/batch_main.cpp','cpp/tests/qptma_project_tests.cpp','tests/test_qptma_project.py','CMakeLists.txt','cpp/qptma.cpp','cpp/include/qupathxx/qptma.hpp','cpp/tma_bindings.cpp','cpp/tests/qptma_tests.cpp',
 'src/qupathxx/__init__.py','src/qupathxx/__init__.pyi','src/qupathxx/_native.pyi','tests/test_qptma.py','tests/test_qptma_reference.py','tests/test_qptma_evidence.py',
 'tools/ci/accept_unified_engine.py','tools/ci/accept_manylinux.py','tools/ci/accept_macos_native.py',
 'cpp/gui_tma.hpp','cpp/gui.cpp','cpp/gui_replay.hpp','cpp/include/qupathxx/core.hpp','cpp/bindings.cpp','tests/test_gui_qptma.py','tests/test_qptma_desktop_evidence.py','tools/ci/qptma-gui-cases.json','.github/workflows/ci.yml','tools/ci/qptma_evidence.py','tools/ci/qptma-cases.json','tools/ci/run.py','tools/generate_stubs.py','tests/typing/qptma_invalid.py','tests/typing/valid_api.py','tools/upstream/qupath_tma_reference.groovy',
 *['tests/fixtures/qupath-tma-0.7.0/'+name for name in ['manifest.json','reference.json','fractional-import.qptma','qupath-export.qptma','capture.log']]]
CASES=json.loads(Path(__file__).with_name('qptma-cases.json').read_text(encoding='utf-8'))
MARKER='QPTMA native fixture, fractional/quoted wire, atomic history, quotas and 10000-core round trips passed'

def require(value,message):
    if not value:raise ValueError(message)

def source_contract(root,commit,sources=None,label="qptma",raw_prefixes=("tests/fixtures/",)):
    sources=SOURCES if sources is None else sources
    root=Path(root);sources=list(sources);hashes={}
    require(all('\n' not in name and '\r' not in name for name in sources),label+' invalid source path')
    request=''.join(commit+':'+name+'\n' for name in sources).encode('utf-8')
    saved=subprocess.run(['git','-C',str(root),'cat-file','--batch'],input=request,capture_output=True)
    require(saved.returncode==0,label+' source read failed')
    stream=saved.stdout;offset=0
    for relative in sources:
        end=stream.find(b'\n',offset);require(end>=offset,label+' source response truncated')
        header=stream[offset:end].split();require(len(header)==3 and header[1]==b'blob',label+' source missing: '+relative)
        size=int(header[2]);require(size>=0,label+' invalid source length')
        blob=stream[end+1:end+1+size]
        require(len(blob)==size and stream[end+1+size:end+2+size]==b'\n',label+' source response truncated')
        offset=end+2+size;raw=(root/relative).read_bytes();canonical=raw if relative.startswith(raw_prefixes) else raw.replace(b'\r\n',b'\n')
        require(canonical==blob,label+' source differs: '+relative)
        hashes[relative]=dict(git_blob_sha256=hashlib.sha256(blob).hexdigest(),checked_out_sha256=hashlib.sha256(raw).hexdigest())
    require(offset==len(stream),label+' source trailing response')
    return hashes


def native(cases):
    selected=[case for case in cases if case.get('name')=='qptma']
    require(len(selected)==1,'missing/duplicate native qptma suite');case=selected[0]
    require(all(case.find(tag) is None for tag in ['failure','error','skipped']),'native qptma did not pass')
    require(case.findtext('system-out','').splitlines().count(MARKER)==1,'native qptma cohort differs')
    return dict(passed=1,bounded_grid_cores=10000,atomic_document_and_file_checks=True)

def python(cases,contract=None,label="qptma installed cohort"):
    contract=CASES if contract is None else contract
    groups=[]
    for classname,names in contract.items():
        selected=[case for case in cases if case.get('classname','').rsplit('.',1)[-1]==classname]
        require(sorted(case.get('name') for case in selected)==names,label+' missing/duplicate/changed: '+classname)
        require(all(case.find(tag) is None for case in selected for tag in ['failure','error','skipped']),label+' failed or skipped')
        groups.append(dict(classname=classname,passed=len(selected)))
    return dict(passed=sum(group['passed'] for group in groups),groups=groups)


def typing_errors(path):
    text=Path(path).read_text(encoding='utf-8')
    require(sorted(map(int,re.findall(r'qptma_invalid\.py:(\d+): error:',text)))==[4,5,6], 'QPTMA negative typing errors differ')
    return dict(expected_errors=3,sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest())


PROJECT_MARKER='QPTMA project atomic new-file publication, budgets, guards, races, persistence and explicit-image CLI passed'
PROJECT_DISABLED='QPTMA project storage disabled in this configuration'

def project_native(cases,enabled=True):
    selected=[case for case in cases if case.get('name')=='qptma_project']
    require(len(selected)==1,'missing/duplicate QPTMA project suite');case=selected[0]
    require(all(case.find(tag) is None for tag in ['failure','error','skipped']),'QPTMA project suite did not pass')
    lines=case.findtext('system-out','').splitlines()
    require(lines.count(PROJECT_MARKER)==int(enabled) and lines.count(PROJECT_DISABLED)==int(not enabled),
        'QPTMA project native capability/coverage differs')
    return dict(passed=1,project_storage_enabled=enabled,atomic_persistence_and_cli_checked=enabled)


DESKTOP_CASES=json.loads(Path(__file__).with_name('qptma-gui-cases.json').read_text(encoding='utf-8'))
def desktop(cases):
    selected=[case for case in cases if case.get('classname','').rsplit('.',1)[-1]=='test_gui_qptma']
    require(sorted(case.get('name') for case in selected)==DESKTOP_CASES,'QPTMA desktop cohort missing/duplicate/changed')
    require(all(case.find(tag) is None for case in selected for tag in ['failure','error','skipped']),'QPTMA desktop case failed or skipped')
    return dict(passed=len(selected),cases=DESKTOP_CASES)
