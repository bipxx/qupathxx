"""Native graph retirement remains separate from Python/GIL-owned graph release."""
import os
import subprocess
import sys

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(
    not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI') != '1',
    reason='requires GUI/project build and QUPATHXX_TEST_GUI=1',
)


def event(frame, kind, target=q.GuiInputTarget.CANVAS):
    result = q.GuiInput()
    result.frame, result.kind, result.target = frame, kind, target
    return result


def test_loaded_native_workspace_retires_on_worker_with_external_document_reference(tmp_path):
    initial = q.Project()
    doc = q.Document(q.Image.demo())
    image = initial.add_document(doc)
    obj = doc.add_rectangle(q.Rect(10, 10, 20, 20), 'Original')
    path = tmp_path/'initial.qpxproj'
    initial.save(str(path))
    current = q.Project.open(str(path))
    old_doc = current.document(image)
    old_doc.rename_annotation(obj, 'Retained external document')
    other = q.Project()
    other.add_document(q.Document(q.Image(32, 32, [40, 80, 160]*32*32)))
    target = tmp_path/'other.qpxproj'
    other.save(str(target))
    config = q.GuiOptions()
    config.project_open_path, config.autosave_interval_seconds = str(target), 0
    config.inputs = [event(3, q.GuiInputKind.SELECT_ALL_KEY), event(4, q.GuiInputKind.OPEN_PROJECT_KEY),
                     event(16, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
                     event(17, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    report = q.show(current, frame_limit=40, options=config)
    assert not report.project_error and report.project_replacements == report.retired_workspaces == 1
    assert report.max_retirement_ms >= 0 and current.identity == other.identity
    assert old_doc.annotation(obj).name == 'Retained external document' and old_doc.can_undo
    assert report.selected_ids == [] and q.Project.open(str(path)).document(image).annotation(obj).name == 'Original'


def test_mixed_native_graph_with_python_owned_inactive_document_uses_owner_thread(tmp_path):
    script = r'''
import gc,sys,weakref
from pathlib import Path
import qupathxx as q
root=Path(sys.argv[1])
p=q.Project(); p.add_document(q.Document(q.Image.demo()))
initial=root/'initial.qpxproj'; p.save(str(initial))
other=q.Project(); other.add_document(q.Document(q.Image.demo()))
target=root/'other.qpxproj'; other.save(str(target))
current=q.Project.open(str(initial))
class Tracked(q.Document):
    pass
doc=Tracked(q.Image.demo()); reference=weakref.ref(doc)
current.add_document(doc,'Python-owned inactive document')
del doc; gc.collect(); assert reference() is not None
options=q.GuiOptions(); options.autosave_interval_seconds=0; options.project_open_path=str(target)
inputs=[]
for frame,kind,where in [(4,q.GuiInputKind.OPEN_PROJECT_KEY,q.GuiInputTarget.CANVAS),
                         (16,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
                         (17,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]:
    item=q.GuiInput(); item.frame,item.kind,item.target=frame,kind,where; inputs.append(item)
options.inputs=inputs
report=q.show(current,frame_limit=40,options=options)
gc.collect()
assert not report.project_error and report.project_replacements==1
assert report.retired_workspaces==0 and reference() is None
assert current.identity==other.identity
'''
    subprocess.run([sys.executable, '-I', '-c', script, str(tmp_path)], check=True, timeout=20)


@pytest.mark.parametrize('reader', ['ppm', 'openslide'])
def test_native_reader_factories_retire_without_mutating_source(tmp_path, reader):
    from pathlib import Path
    if reader == 'openslide':
        if not q.has_openslide or not os.environ.get('QUPATHXX_APERIO_FIXTURE'):
            pytest.skip('requires OpenSlide Aperio fixture')
        source = Path(os.environ['QUPATHXX_APERIO_FIXTURE'])
    else:
        source = tmp_path/'native-source.ppm'
        source.write_bytes(b'P6\n4 4\n255\n'+bytes((40, 80, 160))*16)
    before = source.stat().st_size
    current = q.Project()
    current.add_image(str(source))
    empty = q.Project()
    destination = tmp_path/'empty.qpxproj'
    empty.save(str(destination))
    config = q.GuiOptions()
    config.autosave_interval_seconds, config.project_open_path = 0, str(destination)
    config.inputs = [event(4, q.GuiInputKind.OPEN_PROJECT_KEY),
                     event(16, q.GuiInputKind.LEFT_DOWN, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT),
                     event(17, q.GuiInputKind.LEFT_UP, q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    report = q.show(current, frame_limit=40, options=config)
    assert not report.project_error and report.project_replacements == report.retired_workspaces == 1
    assert current.identity == empty.identity and current.images == [] and report.active_image == 0
    assert source.exists() and source.stat().st_size == before
