"""Actual tool entry without an image/ROI; native prerequisites stay authoritative."""
import os
import json

import pytest
import qupathxx as q
from test_gui_tiles import replay

pytestmark=pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in organized desktop')


def test_raw_file_browser_opens_from_tools_in_an_empty_project(tmp_path):
    project=q.Project()
    options=q.GuiOptions()
    options.width,options.height=1100,800
    options.docked_workspace=True
    options.autosave_interval_seconds=0
    options.layout_path=str(tmp_path/'tools.ini')
    (tmp_path/'tools.ini').write_text('; qupathxx workspace layout v1\n; qupathxx language=en\n; qupathxx sidebar=workflow\n; qupathxx workflow=tools\n',encoding='utf8')
    # Open the grouped chooser, then Raw pixel classifier in its Classifiers
    # group. Observed 1x English popup centers: launcher y=112, item y=240.
    options.inputs=[replay(4,q.GuiInputKind.LEFT_DOWN,100/1100,112/800,target=q.GuiInputTarget.WINDOW),
                    replay(5,q.GuiInputKind.LEFT_UP,100/1100,112/800,target=q.GuiInputTarget.WINDOW),
                    replay(8,q.GuiInputKind.LEFT_DOWN,100/1100,240/800,target=q.GuiInputTarget.WINDOW),
                    replay(9,q.GuiInputKind.LEFT_UP,100/1100,240/800,target=q.GuiInputTarget.WINDOW)]
    files=replay(12,q.GuiInputKind.LEFT_DOWN,target=q.GuiInputTarget.RAW_PIXEL_FILES_TAB)
    files.wait_for=q.GuiInputWait.TARGET_READY
    options.inputs += [files,replay(13,q.GuiInputKind.LEFT_UP,target=q.GuiInputTarget.RAW_PIXEL_FILES_TAB)]
    before=project.generation
    report=q.show(project,frame_limit=22,options=options)
    assert report.replay_complete and project.generation==before and not project.images
    assert '[Window][Raw pixel classifier]' in (tmp_path/'tools.ini').read_text(encoding='utf8')


@pytest.mark.skipif(not q.has_interchange,reason='native object exchange unavailable')
def test_export_shortcut_reveals_annotation_exchange_from_another_task(tmp_path):
    project=q.Project()
    doc=q.Document(q.Image(128,128,[30,120,210]*128*128))
    doc.add_rectangle(q.Rect(10,20,30,40),'Exported ROI')
    project.add_document(doc)
    options=q.GuiOptions();options.docked_workspace=True;options.autosave_interval_seconds=0
    options.layout_path=str(tmp_path/'exchange.ini')
    (tmp_path/'exchange.ini').write_text('; qupathxx workspace layout v1\n; qupathxx sidebar=workflow\n; qupathxx workflow=tools\n',encoding='utf8')
    options.interchange_path=str(tmp_path/'objects.geojson')
    select=q.GuiInput();select.frame=3;select.kind=q.GuiInputKind.SELECT_ALL_KEY
    export=q.GuiInput();export.frame=5;export.kind=q.GuiInputKind.EXPORT_KEY
    options.inputs=[select,export]
    before=project.generation
    report=q.show(project,frame_limit=18,options=options)
    assert report.replay_complete and project.generation==before and doc.object_count==1
    features=json.loads((tmp_path/'objects.geojson').read_text(encoding='utf8'))['features']
    assert len(features)==1 and features[0]['geometry']['type']=='Polygon'
    assert '; qupathxx sidebar=annotations\n' in (tmp_path/'exchange.ini').read_text(encoding='utf8')


def test_clean_project_does_not_expose_an_enabled_save_command(tmp_path):
    from test_gui_docking import workspace, config
    project,_=workspace(tmp_path)
    before=(tmp_path/'source.qpxproj').read_bytes()
    generation=project.generation
    options=config(tmp_path/'clean.ini')
    options.replay_timeout_seconds=.15
    down=replay(3,q.GuiInputKind.LEFT_DOWN,target=q.GuiInputTarget.SAVE_PROJECT)
    down.wait_for=q.GuiInputWait.TARGET_READY
    options.inputs=[down]
    with pytest.raises(RuntimeError,match='timed out'):
        q.show(project,frame_limit=15,options=options)
    assert not project.dirty and project.generation==generation
    assert (tmp_path/'source.qpxproj').read_bytes()==before


def test_first_save_requests_a_destination_without_starting_a_failed_save(tmp_path):
    project=q.Project()
    doc=q.Document(q.Image(128,128,[30,120,210]*128*128))
    image=project.add_document(doc)
    view=q.ProjectView();view.center=q.Point(64,64);view.zoom=1
    project.set_view(image,view)
    generation=project.generation
    options=q.GuiOptions();options.docked_workspace=True;options.autosave_interval_seconds=0
    options.layout_path=str(tmp_path/'first-save.ini')
    save=q.GuiInput();save.frame=3;save.kind=q.GuiInputKind.SAVE_KEY
    options.inputs=[save]
    report=q.show(project,frame_limit=18,capture_path=str(tmp_path/'destination.ppm'),options=options)
    assert report.replay_complete and report.saves_completed==0 and not report.project_error
    assert project.path=='' and project.dirty and project.generation==generation
    assert len(project.images)==1 and not list(tmp_path.glob('*.qpxproj'))
