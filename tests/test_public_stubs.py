"""Packaged PEP 561 artifacts match the runtime public class/member surface."""
import ast
from pathlib import Path

import qupathxx as q
from qupathxx import _native


def test_packaged_marker_and_public_exports_match_runtime():
    package=Path(_native.__file__).parent
    assert (package/'py.typed').is_file()
    tree=ast.parse((package/'__init__.pyi').read_text(encoding='utf8'))
    names={a.asname for n in tree.body if isinstance(n,ast.ImportFrom) for a in n.names}
    assert names==set(q.__all__)


def test_native_stub_covers_public_classes_members_functions_and_constants():
    tree=ast.parse(Path(_native.__file__).with_name('_native.pyi').read_text(encoding='utf8'))
    top={n.name:n for n in tree.body if isinstance(n,(ast.ClassDef,ast.FunctionDef))}
    top.update({n.target.id:n for n in tree.body if isinstance(n,ast.AnnAssign) and isinstance(n.target,ast.Name)})
    for name,value in vars(_native).items():
        if name.startswith('_'):continue
        assert name in top,name
        if isinstance(value,type) and value.__module__=='qupathxx._native':
            node=top[name];assert isinstance(node,ast.ClassDef)
            members={n.name for n in node.body if isinstance(n,ast.FunctionDef)}
            members.update(n.target.id for n in node.body if isinstance(n,ast.AnnAssign) and isinstance(n.target,ast.Name))
            members.update(t.id for n in node.body if isinstance(n,ast.Assign) for t in n.targets if isinstance(t,ast.Name))
            for member in vars(value):
                if not member.startswith('_'):assert member in members,(name,member)
