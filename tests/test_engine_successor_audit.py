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
