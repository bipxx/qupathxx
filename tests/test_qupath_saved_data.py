"""Direct native saved-data decoding against actual pinned QuPath output."""
import hashlib
import json
import os
from pathlib import Path
import shutil

import pytest
import qupathxx as q

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-saved-data-0.7.0'
pytestmark=pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='project/interchange/TIFF')


def corpus(tmp_path,label):
    directory=tmp_path/'foreign';shutil.copytree(FIXTURE,directory)
    path=directory/'project/project.qpproj';data=json.loads(path.read_bytes())
    data['images']=[image for image in data['images'] if image['imageName']==label]
    assert len(data['images'])==1
    data['images'][0]['serverBuilder']['uri']=(directory/'source.ome.tif').as_uri()
    path.write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8');return path


def test_actual_saved_data_fixture_identity():
    manifest=json.loads((FIXTURE/'manifest.json').read_bytes())
    for name,digest in manifest['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    for name,digest in manifest['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest
    assert manifest['upstream']['revision']=='04ccfa4fb7d43e9b566393e08e83690b72248d44'


def test_direct_geometry_hierarchy_classes_ids_and_calibration_reopen(tmp_path):
    path=corpus(tmp_path,'geometry');preview=q.preview_qupath_project(str(path))
    assert preview.issues==[] and preview.images[0].objects==8
    reference=next(row for row in json.loads((FIXTURE/'reference.json').read_bytes()) if row['name']=='geometry')['objects']
    destination=tmp_path/'native.qpxproj';preview.import_new(str(destination))
    doc=q.Project.open(str(destination)).document(preview.images[0].native_id)
    assert (doc.pixel_width_um,doc.pixel_height_um)==(.25,.5)
    ids=dict(zip(preview.images[0].source_object_ids,preview.images[0].imported_object_ids,strict=True))
    assert set(ids)=={row['id'] for row in reference}
    kinds={'annotation':q.ObjectKind.ANNOTATION,'detection':q.ObjectKind.DETECTION,'cell':q.ObjectKind.CELL}
    for row in reference:
        obj=doc.annotation(ids[row['id']]);assert obj.name==row['name']
        assert obj.parent_id==(ids[row['parent']] if row['parent'] else 0)
        assert obj.kind==kinds[row['kind']]
        bounds=obj.geometry.bounds
        assert [bounds.x,bounds.y,bounds.width,bounds.height]==pytest.approx(row['bounds'])
        assert obj.geometry.area==pytest.approx(row['area'],rel=1e-12,abs=1e-10)
        assert obj.class_name==(row['classification'] or '')
        if row['nucleus_area'] is not None:assert obj.nucleus.area==pytest.approx(row['nucleus_area'],rel=1e-12)
    assert doc.object_class('Tumor').color==0xff2030


def test_saved_settings_losses_require_review_and_explicit_admission(tmp_path):
    path=corpus(tmp_path,'losses');preview=q.preview_qupath_project(str(path))
    codes={issue.code for issue in preview.issues}
    assert not {'object_metadata_dropped','object_measurements_dropped'} & codes
    assert {'object_color_dropped','object_lock_dropped','classification_hierarchy_flattened','workflow_dropped','image_settings_dropped'}<=codes
    output=tmp_path/'loss.qpxproj'
    with pytest.raises(ValueError,match='loss'):preview.import_new(str(output))
    assert not output.exists() and not preview.consumed
    preview.import_new(str(output),allow_losses=True)
    obj=q.Project.open(str(output)).document(preview.images[0].native_id).annotations[0]
    assert obj.properties.metadata=={'note':'saved note'} and obj.properties.measurements=={'Synthetic':3.5}
    assert obj.name=='Lossy' and obj.class_name=='Tumor: Positive'


def test_nondefault_saved_plane_is_explicitly_refused(tmp_path):
    path=corpus(tmp_path,'plane')
    with pytest.raises(ValueError,match='non-default Z/T/channel'):q.preview_qupath_project(str(path))
    assert not list(tmp_path.glob('*.qpxproj'))


def test_direct_preview_rechecks_saved_data_and_absent_exchange(tmp_path):
    path=corpus(tmp_path,'geometry');preview=q.preview_qupath_project(str(path))
    (path.parent/'qupathxx-exchange').mkdir();(path.parent/'qupathxx-exchange/index.json').write_text('{}')
    with pytest.raises(RuntimeError,match='appeared'):preview.import_new(str(tmp_path/'changed.qpxproj'))
    assert not preview.consumed and not (tmp_path/'changed.qpxproj').exists()


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in saved-data desktop')
def test_direct_saved_data_desktop_import(tmp_path):
    from test_gui_replay import event
    path=corpus(tmp_path,'geometry');project=q.Project();project.add_document(q.Document(q.Image.demo()),'Original')
    options=q.GuiOptions();options.project_open_path=str(path);options.settle_tiles=True
    options.inputs=[event(5,q.GuiInputKind.OPEN_PROJECT_KEY),event(60,q.GuiInputKind.OPEN_PROJECT_KEY),event(80,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT,q.GuiInputWait.TARGET_READY),event(81,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    report=q.show(project,frame_limit=120,capture_path=str(tmp_path/'direct-import.ppm'),options=options)
    assert report.replay_complete and report.project_replacements==1 and len(project.images)==1
    assert project.document(project.images[0].id).object_count==8
    restored=q.Project.open(str(path.with_suffix('.qpxproj')))
    assert restored.document(restored.images[0].id).object_count==8
