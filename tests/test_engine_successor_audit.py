"""Exercise maintained auditor seams without starting CI, compiling or querying GitHub."""
import ast
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import xml.etree.ElementTree as ET

import pytest

ROOT=Path(__file__).resolve().parents[1]
AUDITOR=ROOT/'tools/ci/accept_unified_engine.py'
CONTRACT=json.loads((ROOT/'tools/ci/tma-axis-cases.json').read_text(encoding='utf-8'))


def statements():
    return ast.parse(AUDITOR.read_text(encoding='utf-8')).body


def test_optimized_cli_rejects_before_import_or_network():
    body=statements()
    first=next(node for node in body if not (isinstance(node,ast.Expr) and isinstance(node.value,ast.Constant)))
    assert isinstance(first,ast.If) and ast.unparse(first.test)=='not __debug__'
    done=subprocess.run([sys.executable,'-I','-O',str(AUDITOR),'--help'],capture_output=True,timeout=15)
    assert done.returncode!=0
    assert b'artifact audit requires Python assertions enabled' in done.stderr
    assert b'usage:' not in done.stdout


def gui_guard():
    nodes=[node for node in ast.walk(ast.Module(body=statements(),type_ignores=[]))
           if isinstance(node,ast.Assert) and "axis.CONTRACT['desktop']['test_gui_tma']" in ast.unparse(node)]
    assert len(nodes)==1
    return compile(ast.Module(body=nodes,type_ignores=[]),str(AUDITOR),'exec')


def cases():
    return [ET.Element('testcase',name=name) for name in CONTRACT['desktop']['test_gui_tma']]


def check_gui(items):
    exec(gui_guard(),dict(cases=items,axis=SimpleNamespace(CONTRACT=CONTRACT)))


def test_named_gui_contract_passes_with_all_structural_cases():
    items=cases()
    assert len(items)==24
    check_gui(items)


@pytest.mark.parametrize('change',['old_17','missing','duplicate','same_count_substitution','failure','error','skipped'])
def test_named_gui_contract_rejects_incomplete_or_failed_evidence(change):
    items=cases()
    if change=='old_17':items=items[:17]
    elif change=='missing':items.pop()
    elif change=='duplicate':items[-1]=items[0]
    elif change=='same_count_substitution':items[-1].set('name','unrelated_case')
    else:ET.SubElement(items[-1],change)
    with pytest.raises(AssertionError):check_gui(items)


@pytest.mark.parametrize('head,dirty,expected',[
    ('original','',True),('different','',False),('original',' M source.py\n',False)])
def test_successor_requires_original_clean_source(head,dirty,expected):
    node=next(node for node in statements() if isinstance(node,ast.If) and ast.unparse(node.test)=='args.source_root is not None')
    calls=[]
    def output(command,**kwargs):
        calls.append(command)
        return head+'\n' if 'rev-parse' in command else dirty
    scope=dict(args=SimpleNamespace(source_root=ROOT),COMMIT='original',
               subprocess=SimpleNamespace(check_output=output))
    if expected:
        exec(compile(ast.Module(body=[node],type_ignores=[]),str(AUDITOR),'exec'),scope)
        assert scope['ROOT']==ROOT.resolve()
    else:
        with pytest.raises(ValueError,match='clean original source checkout'):
            exec(compile(ast.Module(body=[node],type_ignores=[]),str(AUDITOR),'exec'),scope)
    assert calls


def test_junit_failure_refusal_survives_optimized_function_compilation(tmp_path):
    # The CLI rejects -O, but the external-data validator should also reject if
    # embedded by a caller compiling only this function.
    node=next(node for node in statements() if isinstance(node,ast.FunctionDef) and node.name=='junit')
    path=tmp_path/'failed.xml'
    path.write_text('<testsuite tests="1" failures="1" errors="0" skipped="0"/>',encoding='utf-8')
    from collections import Counter
    scope=dict(ET=ET,Counter=Counter,sha=lambda _: 'fixture')
    exec(compile(ast.Module(body=[node],type_ignores=[]),str(AUDITOR),'exec',optimize=1),scope)
    with pytest.raises(ValueError,match='JUnit failures'):
        scope['junit'](path)


def test_final_receipt_preserves_successor_and_original_source_identities():
    body=statements()
    assignments=[n for n in body if isinstance(n,ast.Assign) and
                 any(ast.unparse(t)=="report['auditor']" for t in n.targets)]
    assert len(assignments)==1, 'later receipt writer must not replace successor provenance'
    updates=[n for n in body if isinstance(n,ast.Expr) and isinstance(n.value,ast.Call)
             and ast.unparse(n.value.func)=="report['auditor'].update"]
    assert len(updates)==1
    auditor=dict(commit='new_auditor',original_source_root='original_checkout',
                 successor_audit=True,helper_sources={'helper.py':'digest'},dirty=False)
    scope=dict(report={'auditor':auditor.copy()},sha=lambda _: 'script_digest',
               Path=Path,__file__=str(AUDITOR))
    exec(compile(ast.Module(body=updates,type_ignores=[]),str(AUDITOR),'exec'),scope)
    assert scope['report']['auditor']==dict(auditor,path='tools/ci/accept_unified_engine.py',sha256='script_digest')


def legacy_record_guard():
    return next(node for parent in statements() for node in ast.walk(parent)
                if isinstance(node,ast.For) and ast.unparse(node.target)=='(suffix, count)'
                and isinstance(node.iter,ast.List)
                and ast.unparse(node.iter).startswith("[('test_pixel_archives'"))


def legacy_record_fixture(tmp_path):
    source=ast.parse((ROOT/'tests/test_pixel_stream_records.py').read_text(encoding='utf-8'))
    function=next(n for n in source.body if isinstance(n,ast.FunctionDef)
                  and n.name.startswith('test_legacy_schemas_load_and_transactionally_upgrade_to_schema'))
    parameter=next(d for d in function.decorator_list if isinstance(d,ast.Call)
                   and ast.unparse(d.func)=='pytest.mark.parametrize' and ast.literal_eval(d.args[0])=='version')
    versions=range(*(ast.literal_eval(v) for v in parameter.args[1].args))
    migrations=[function.name+'['+str(v)+']' for v in versions]
    root=ET.Element('testsuite');record_cases=[]
    required=eval(compile(ast.Expression(legacy_record_guard().iter),str(AUDITOR),'eval'),
                  dict(args=SimpleNamespace(ndpi_readers=True)))
    for module,count in required:
        names=migrations+['retained_record_case_'+str(i) for i in range(22)] if module=='test_pixel_stream_records' else ['case_'+str(i) for i in range(count)]
        for name in names:
            case=ET.SubElement(root,'testcase',classname='tests.'+module,name=name)
            if module=='test_pixel_stream_records':record_cases.append(case)
    return root,record_cases


def check_legacy_record_guard(tmp_path,root):
    ET.ElementTree(root).write(tmp_path/'python-tests.xml',encoding='utf-8')
    scope=dict(ET=ET,folder=tmp_path,args=SimpleNamespace(ndpi_readers=True),
               job={'name':'source-bound fixture'},entry={})
    exec(compile(ast.Module(body=[legacy_record_guard()],type_ignores=[]),str(AUDITOR),'exec'),scope)
    return scope['entry']


def test_legacy_record_gate_accepts_schema14_source_migrations(tmp_path):
    root,cases=legacy_record_fixture(tmp_path)
    assert len(cases)==35
    result=check_legacy_record_guard(tmp_path,root)
    assert len(result['test_pixel_stream_records_cases'])==35


@pytest.mark.parametrize('change',['old_count','missing_version13','old_family','duplicate_identity',
                                   'failed_version13','skipped_version13','error_version13'])
def test_legacy_record_gate_refuses_incomplete_migrations(tmp_path,change):
    root,cases=legacy_record_fixture(tmp_path)
    final=next(c for c in cases if c.get('name').endswith('[13]'))
    if change=='old_count':root.remove(cases[-1])
    elif change=='missing_version13':final.set('name','replacement_case')
    elif change=='old_family':
        for case in cases:case.set('name',case.get('name').replace('to_schema14','to_schema13'))
    elif change=='duplicate_identity':cases[-1].set('name',cases[-2].get('name'))
    else:ET.SubElement(final,{'failed_version13':'failure','skipped_version13':'skipped','error_version13':'error'}[change])
    with pytest.raises(AssertionError):check_legacy_record_guard(tmp_path,root)
