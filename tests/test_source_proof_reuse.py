"""Invocation-local proof reuse must retain final byte and declaration checks."""
import ast
import hashlib
import importlib.util
from pathlib import Path
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qpx_source_reuse',ROOT/'tools/ci/qptma_evidence.py')
evidence=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,'exec',dont_inherit=True),evidence.__dict__)


def observed(data):
    return dict(git_blob_sha256=hashlib.sha256(data.replace(b'\r\n',b'\n')).hexdigest(),checked_out_sha256=hashlib.sha256(data).hexdigest())


def test_overlapping_source_declarations_recheck_once_without_git(tmp_path,monkeypatch):
    (tmp_path/'a').write_bytes(b'line\r\n');(tmp_path/'b').write_bytes(b'other')
    first={'a':observed(b'line\r\n')};second={'a':{**first['a'],'crlf_normalized':True},'b':observed(b'other')}
    def forbidden(*args,**kwargs): raise AssertionError('final recheck must not query Git')
    monkeypatch.setattr(evidence.subprocess,'run',forbidden)
    original=Path.read_bytes;reads=[]
    def read(path): reads.append(path);return original(path)
    monkeypatch.setattr(Path,'read_bytes',read)
    assert evidence.verify_source_snapshot(tmp_path,first,second)==2
    assert sorted(path.name for path in reads)==['a','b']


@pytest.mark.parametrize('change',['changed','missing','newline_drift','conflicting_git','conflicting_checkout','invalid_digest','invalid_path'])
def test_reused_proof_cannot_hide_drift_or_conflicting_declarations(tmp_path,change):
    path=tmp_path/'a';path.write_bytes(b'line\r\n');first={'a':observed(path.read_bytes())};second={'a':dict(first['a'])}
    if change=='changed':path.write_bytes(b'other')
    elif change=='missing':path.unlink()
    elif change=='newline_drift':path.write_bytes(b'line\n')
    elif change=='conflicting_git':second['a']['git_blob_sha256']='a'*64
    elif change=='conflicting_checkout':second['a']['checked_out_sha256']='a'*64
    elif change=='invalid_digest':second['a']['checked_out_sha256']='not-a-sha256'
    else:second={'../outside':dict(first['a'])}
    with pytest.raises((ValueError,OSError)):evidence.verify_source_snapshot(tmp_path,first,second)


@pytest.mark.parametrize('filename,jobs',[('accept_unified_engine.py',5),('accept_macos_native.py',2)])
def test_actual_auditor_reuses_proofs_and_copies_each_job_receipt(tmp_path,filename,jobs):
    tree=ast.parse((ROOT/'tools/ci'/filename).read_text(encoding='utf-8'))
    container=tree.body if filename=='accept_unified_engine.py' else next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='audit').body
    hoists=[n for n in container if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id in ('qptma_sources','resampling_sources') for t in n.targets)]
    assert len(hoists)==2
    calls=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and isinstance(n.func.value,ast.Name) and n.func.value.id in ('qptma','resampling') and n.func.attr=='source_contract']
    assert len(calls)==2,'checkout proof must run once per domain, outside job/architecture loop'
    entries=[n for n in ast.walk(tree) if isinstance(n,ast.Assign) and any(ast.unparse(t).endswith("['qptma_sources']") or ast.unparse(t).endswith("['resampling_sources']") for t in n.targets)]
    assert len(entries)==2
    path=tmp_path/'a';path.write_bytes(b'input');proof={'a':observed(path.read_bytes())};seen=[]
    def source_contract(root,commit):
        seen.append((root,commit));return {name:dict(value) for name,value in proof.items()}
    helper=SimpleNamespace(source_contract=source_contract,verify_source_snapshot=evidence.verify_source_snapshot)
    scope=dict(qptma=helper,resampling=helper,ROOT=tmp_path,root=tmp_path,COMMIT='c'*40,commit='c'*40)
    exec(compile(ast.Module(body=hoists,type_ignores=[]),filename,'exec'),scope)
    for _ in range(jobs):
        scope.update(entry={},entries=[{}])
        exec(compile(ast.Module(body=entries,type_ignores=[]),filename,'exec'),scope)
        emitted=scope['entry'] if filename=='accept_unified_engine.py' else scope['entries'][-1]
        assert emitted['qptma_sources']==proof and emitted['resampling_sources']==proof
        emitted['qptma_sources']['a']['checked_out_sha256']='mutated receipt copy'
        assert scope['qptma_sources']==proof
    assert seen==[(tmp_path,'c'*40)]*2
    assert evidence.verify_source_snapshot(tmp_path,scope['qptma_sources'],scope['resampling_sources'])==1


@pytest.mark.parametrize('filename',['accept_unified_engine.py','accept_macos_native.py','accept_manylinux.py'])
@pytest.mark.parametrize('mutated',[False,True])
def test_actual_final_boundary_rejects_source_change(filename,mutated,tmp_path):
    tree=ast.parse((ROOT/'tools/ci'/filename).read_text(encoding='utf-8'))
    guards=[n for n in ast.walk(tree) if isinstance(n,ast.Expr) and isinstance(n.value,ast.Call) and ast.unparse(n.value.func)=='qptma.verify_source_snapshot']
    assert len(guards)==1
    path=tmp_path/'a';path.write_bytes(b'input');proof={'a':observed(path.read_bytes())}
    if mutated:path.write_bytes(b'late mutation')
    scope=dict(qptma=evidence,ROOT=tmp_path,root=tmp_path,qptma_sources=proof,resampling_sources=proof,Path=Path,__file__=str(tmp_path/'tools/ci'/filename),report={'qptma_sources':proof,'resampling_sources':proof})
    code=compile(ast.Module(body=guards,type_ignores=[]),filename,'exec')
    if mutated:
        with pytest.raises(ValueError,match='changed before publication'):exec(code,scope)
    else:exec(code,scope)
