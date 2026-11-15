"""Read the actual private workflow call domains, including local adapters.

This intentionally fails closed on a new computed original: add its data-flow
rule rather than silently omitting it from the bidirectional table contract.
"""
import re


def code_mask(source):
    """Keep offsets, hiding comments and quoted contents from call discovery."""
    return re.sub(r'"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'|//[^\n]*|/\*.*?\*/',
                  lambda m: ' ' * len(m[0]), source, flags=re.S)


def calls(source, name):
    masked=code_mask(source)
    qualified=name.replace('::',r'\s*::\s*')
    for match in re.finditer(r'\b'+qualified+r'\s*\(', masked):
        start=match.end();depth=1;args=[];part=start
        for i in range(start,len(source)):
            c=masked[i]
            if c in '({[':depth+=1
            elif c in ')}]':
                depth-=1
                if not depth:
                    args.append(source[part:i].strip());yield args;break
            elif c==',' and depth==1:args.append(source[part:i].strip());part=i+1


def literals(expr):
    return re.findall(r'"((?:[^"\\]|\\.)*)"',expr)


def original_domain(expr):
    """Resolve only literals and conditional branches; reject computed strings."""
    expr=expr.strip()
    if re.fullmatch(r'"(?:[^"\\]|\\.)*"',expr):return literals(expr)
    depth=0;quote=False;escape=False;question=None;nested=0;closing=None
    for i,c in enumerate(expr):
        if quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c=='"':quote=False
        elif c=='"':quote=True
        elif c=='(':depth+=1
        elif c==')':
            depth-=1
            if depth==0:closing=i
        elif depth==0:
            if c=='?':
                if question is None:question=i
                else:nested+=1
            elif c==':' and question is not None:
                if nested:nested-=1
                else:return original_domain(expr[question+1:i])+original_domain(expr[i+1:])
    if expr.startswith('(') and closing==len(expr)-1:return original_domain(expr[1:-1])
    raise AssertionError(('Unresolved original expression',expr))


def inventory(sources, originals_only=False):
    result={}
    def add(original,caption,shortcut=''):
        if originals_only:caption,shortcut=original,''
        if original in result:assert result[original]==(caption,shortcut),(original,result[original],caption,shortcut)
        result[original]=(caption,shortcut)
    for path,source in sources.items():
        if path.endswith('gui_workflow_ui.hpp'):continue
        for kind in ['WorkflowAction','WorkflowIcon']:
            for args in calls(source,kind):
                original=args[0]
                if original in ['label','text']:
                    # These four local button adapters have a closed literal
                    # domain at their call sites, with captions keyed by target.
                    assert any(path.endswith(n) for n in ['gui_object_classifier.hpp','gui_pixel_classifier.hpp','gui_raw_pixel.hpp','gui_tma.hpp']),path
                    captions=dict(re.findall(r'case GuiInputTarget::(\w+):caption="([^"]*)"',source))
                    for button in calls(source,'button'):
                        names=original_domain(button[0])
                        target=button[1].split('::')[-1]
                        for name in names:
                            caption=captions.get(target,name)
                            if target in ['CancelQptma','CancelTmaMetadata']:caption='Cancel'
                            if target=='ApplyTmaGeometry':caption='Apply geometry' if name=='Apply core geometry' else 'Add core'
                            if target=='TogglePixelOverlay':caption=name
                            add(name,caption)
                elif original=='caption':
                    assert path.endswith('gui.cpp'),path
                    expr=re.search(r'const char\* caption=(table_kind==0\?.*?);',source,re.S).group(1)
                    for name in original_domain(expr):
                        shortcut=re.search(r'\(([^)]+)\)',name)
                        add(name,'Export CSV',shortcut.group(1) if shortcut else '')
                elif original=='model.cancel_original' or original.startswith('ids.'):
                    assert path.endswith('gui_result_panel.hpp'),path
                    for owner in sources.values():
                        if original=='model.cancel_original':
                            for name in re.findall(r'\w+\.cancel_original="([^"]+)"',owner):
                                add(name,'Cancel', 'Ctrl+Alt+G' if name=='Cancel cells (Ctrl+Alt+G)' else '')
                        else:
                            index={'ids.keep':0,'ids.view':1,'ids.recheck':2}[original]
                            for init in re.findall(r'const ui::ResultIds \w+\{([^}]+)\}',owner):
                                args=next(calls('ids('+init+')','ids'));name=literals(args[index])[0]
                                add(name,{'ids.keep':'Keep result','ids.view':'View kept results','ids.recheck':'Recheck result'}[original])
                else:
                    names=original_domain(original)
                    # Presentation arguments no longer exist. Only literal
                    # originals (including ternary domains) resolve here;
                    # computed originals require an explicit rule above.
                    for name in names:add(name,name)
    return result
