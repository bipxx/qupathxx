"""Quantitative TIFF views persist independently of display layout preferences."""
import sqlite3
import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_tiff_reader import request

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_project,reason='TIFF project support')


def test_opened_tiff_document_defaults_to_native_link_and_current_view(tmp_path):
    original, image, values = scene(tmp_path)
    doc = original.document(image)
    view = doc.server.tiff_view
    view.z, view.t, view.channels = 2, 1, [1, 0, -1]
    doc.set_tiff_view(view)
    project = q.Project()
    attached = project.add_document(doc)
    assert not project.images[0].embedded and project.images[0].reader == 'tiff'
    path = tmp_path/'attached.qpxproj'
    project.save(str(path))
    restored = q.Project.open(str(path)).document(attached)
    assert restored.server.tiff_view.z == 2 and restored.server.tiff_view.t == 1
    assert restored.server.tiff_view.channels == [1, 0, -1]
    np.testing.assert_array_equal(restored.server.read_raw(request(24,16,c=2,z=2,t=1)).numpy(), values[1,2].transpose(1,2,0))


def scene(tmp_path):
    path=tmp_path/'source.ome.tif'
    values=np.arange(2*3*2*16*24,dtype='uint16').reshape(2,3,2,16,24)
    tifffile.imwrite(path,values,ome=True,photometric='minisblack',metadata={'axes':'TZCYX'})
    project=q.Project();image=project.add_tiff_image(str(path),name='Quantitative')
    return project,image,values


def test_view_change_preserves_objects_history_and_exact_raw(tmp_path):
    p,image,values=scene(tmp_path);doc=p.document(image)
    obj=doc.add_rectangle(q.Rect(1,2,5,6));doc.set_calibration(.25,.5)
    original=doc.server
    r=request(24,16,c=2,z=2,t=1);original.read_raw(r)
    decoded=original.tiff_read_stats.decoded_blocks
    before=doc.revision;v=original.tiff_view;v.z=2;v.t=1;v.channels=[1,0,-1];v.lower=[0,0,0];v.upper=[5000,5000,5000]
    p.set_tiff_view(image,v)
    assert doc.revision==before+1 and doc.annotation(obj).geometry.bounds.area==30
    np.testing.assert_array_equal(doc.server.read_raw(r).numpy(),values[1,2].transpose(1,2,0))
    assert doc.server.tiff_read_stats.decoded_blocks==decoded
    assert original.tiff_view.z==0 and doc.server.tiff_view.z==2
    before=doc.revision;doc.set_tiff_view(doc.server.tiff_view);assert doc.revision==before
    assert doc.undo() and doc.object_count==1
    assert doc.undo() and doc.object_count==0
    assert doc.redo() and doc.object_count==1
    assert doc.redo()
    path=tmp_path/'project.qpxproj';p.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==14
        assert db.execute('SELECT count(*) FROM tiff_views').fetchone()[0]==1
    restored=q.Project.open(str(path)).document(image)
    assert restored.server.tiff_view.channels==[1,0,-1] and restored.server.tiff_view.z==2
    np.testing.assert_array_equal(restored.server.read_raw(r).numpy(),values[1,2].transpose(1,2,0))
    assert restored.annotation(obj).geometry.bounds.area==30


@pytest.mark.parametrize('mutation',['checksum','missing','orphan'])
def test_corrupt_view_rows_rejected(tmp_path,mutation):
    p,image,_=scene(tmp_path);path=tmp_path/'bad.qpxproj';p.save(str(path))
    with sqlite3.connect(path) as db:
        if mutation=='checksum':db.execute("UPDATE tiff_views SET payload=x'3031'")
        elif mutation=='missing':db.execute('DELETE FROM tiff_views')
        else:db.execute('INSERT INTO tiff_views SELECT 999,payload FROM tiff_views LIMIT 1')
    with pytest.raises((RuntimeError,ValueError)):q.Project.open(str(path))


def test_relink_retains_axes_and_display_selection(tmp_path):
    p,image,values=scene(tmp_path);v=p.document(image).server.tiff_view;v.z=1;v.t=1
    p.set_tiff_view(image,v)
    path=tmp_path/'saved.qpxproj';p.save(str(path))
    p.document(image).server.close()
    moved=tmp_path/'moved.ome.tif';(tmp_path/'source.ome.tif').rename(moved)
    restored=q.Project.open(str(path));restored.relink(image,str(moved))
    assert restored.document(image).server.tiff_view.z==1
    np.testing.assert_array_equal(restored.document(image).server.read_raw(request(24,16,c=2,z=1,t=1)).numpy(),values[1,1].transpose(1,2,0))


def test_dense_document_cannot_claim_quantitative_tiff_link(tmp_path):
    source=tmp_path/'source.tif';tifffile.imwrite(source,np.ones((8,8),dtype='uint16'),metadata=None)
    project=q.Project();doc=q.Document(q.Image(8,8,[0]*8*8*3))
    with pytest.raises(ValueError,match='native TIFF'):
        project.add_document(doc,source_path=str(source),reader='tiff')
    assert not project.images
