"""Actual stale-bytecode and child-launch witnesses for the existing CI owners."""
import ast
import importlib.util
import os
from pathlib import Path
import py_compile
import subprocess
import sys

import pytest

ROOT=Path(__file__).resolve().parents[1]
CI=ROOT/'tools/ci'


def source_executors():
    selected=[]
    for path in sorted(CI.glob('*.py')):
        tree=ast.parse(path.read_text(encoding='utf-8'))
        for node in ast.walk(tree):
            if (isinstance(node,ast.Call) and isinstance(node.func,ast.Name)
                    and node.func.id=='exec' and node.args and isinstance(node.args[0],ast.Call)
                    and isinstance(node.args[0].func,ast.Name) and node.args[0].func.id=='compile'
                    and '.origin' in ast.unparse(node)):
                selected.append((path,node))
    return selected


EXECUTORS=source_executors()


@pytest.mark.parametrize('path,node',EXECUTORS,ids=[p.name+':'+str(n.lineno) for p,n in EXECUTORS])
def test_actual_executor_uses_changed_source_despite_valid_timestamp_cache(tmp_path,path,node):
    target=tmp_path/'subject.py'
    target.write_bytes(b'VALUE = "old"\n')
    py_compile.compile(str(target),doraise=True)
    old=target.stat()
    target.write_bytes(b'VALUE = "new"\n')
    os.utime(target,ns=(old.st_atime_ns,old.st_mtime_ns))
    spec=importlib.util.spec_from_file_location('source_boundary_subject',target)
    cached=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cached)
    assert cached.VALUE=='old'  # The stale-cache condition really distinguishes.
    module=importlib.util.module_from_spec(spec)
    text=ast.unparse(node)
    origin=next(n for n in ast.walk(node) if isinstance(n,ast.Attribute) and n.attr=='origin')
    destination=node.args[1]
    assert isinstance(origin.value,ast.Name) and isinstance(destination,ast.Attribute)
    scope={'Path':Path,origin.value.id:spec,destination.value.id:module}
    exec(compile(ast.Expression(node),str(path),'eval'),scope)
    assert module.VALUE=='new'
    assert module.__file__==str(target)


def test_no_evidence_owner_reintroduces_timestamp_loader():
    assert EXECUTORS
    for path in CI.glob('*.py'):
        tree=ast.parse(path.read_text(encoding='utf-8'))
        assert not any(isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute)
                       and n.func.attr=='exec_module' for n in ast.walk(tree)),path


def test_nested_axis_helper_compiles_its_real_common_source():
    path=CI/'tma_axis_evidence.py'
    spec=importlib.util.spec_from_file_location('review_nested_axis',path)
    module=importlib.util.module_from_spec(spec)
    exec(compile(path.read_bytes(),str(path),'exec',dont_inherit=True),module.__dict__)
    assert Path(module.common.__file__)==CI/'qptma_evidence.py'
    assert len(module.CONTRACT['desktop']['test_gui_tma'])==24
    with pytest.raises(ValueError,match='boundary witness'):
        module.common.require(False,'boundary witness')


@pytest.mark.parametrize('name,value',[
    ('PYTEST_ADDOPTS','--collect-only'),('PYTEST_ADDOPTS','-k nonexistent'),
    ('PYTEST_PLUGINS','not_a_real_plugin'),
])
def test_actual_producer_rejects_conflicting_environment_before_work_creation(tmp_path,name,value):
    env=os.environ.copy()
    for key in ('PYTEST_ADDOPTS','PYTEST_PLUGINS'):env.pop(key,None)
    env[name]=value
    work=tmp_path/'must-not-exist'
    done=subprocess.run([sys.executable,'-I',str(CI/'run.py'),'--mode','headless','--work',str(work)],
                        env=env,capture_output=True,timeout=20)
    assert done.returncode!=0
    assert ('qualification rejects inherited '+name).encode() in done.stderr
    assert not work.exists()


@pytest.mark.parametrize('environment',[{}, {'PYTEST_ADDOPTS':'','PYTEST_PLUGINS':'','PYTEST_DISABLE_PLUGIN_AUTOLOAD':'1'}])
def test_normal_environment_admission_retains_intended_profile(environment):
    tree=ast.parse((CI/'run.py').read_text(encoding='utf-8'))
    function=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='qualification_environment')
    scope={}
    exec(compile(ast.Module(body=[function],type_ignores=[]),str(CI/'run.py'),'exec'),scope)
    actual=scope['qualification_environment'](environment)
    assert actual==environment and actual is not environment
