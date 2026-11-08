"""Relink publishes its verified reader, retaining ownership and source identity."""
import shutil

import numpy as np
import pytest
import qupathxx as q

from test_source_identity import ppm
from test_tiff_project import scene
from test_tiff_reader import request

pytestmark=pytest.mark.skipif(not q.has_project,reason='project storage')


@pytest.mark.parametrize('mutation',['remove','replace'])
def test_relink_keeps_verified_pixels_source_and_old_owner(tmp_path,mutation):
    original=ppm(tmp_path/'original.ppm',16);candidate=ppm(tmp_path/'candidate.ppm',96)
    p=q.Project();image=p.add_image(str(original),reader='ppm');doc=p.document(image)
    obj=doc.add_rectangle(q.Rect(1,2,20,16));doc.set_calibration(.25,.5)
    old=doc.server;old_source=q.reader_source_json(old.reader_source);before=doc.revision
    path=tmp_path/'saved.qpxproj';p.save(str(path))
    p.relink(image,str(candidate));linked=doc.server;source=linked.reader_source
    assert source.scope=='single_file' and source.primary.path==str(candidate)
    assert source.generation!=old.reader_source.generation
    assert doc.revision==before and p.dirty
    assert doc.server.metadata.width==32 and doc.server.metadata.height==24
    assert (doc.pixel_width_um,doc.pixel_height_um)==(.25,.5)
    captured=q.reader_source_json(source)
    if mutation=='remove':candidate.unlink()
    else:ppm(candidate,192)
    # Dense PPM verification already owns its pixels; a second open would fail
    # after unlink or silently read192 after replacement.
    assert doc.mean_intensity(obj)==96
    assert q.reader_source_json(linked.reader_source)==captured
    old_doc=q.Document(old);old_obj=old_doc.add_rectangle(q.Rect(1,2,20,16))
    assert old_doc.mean_intensity(old_obj)==16
    assert q.reader_source_json(old.reader_source)==old_source
    ppm(candidate,96);p.save(str(path));restored=q.Project.open(str(path))
    restored_doc=restored.document(image)
    assert restored_doc.server.reader_source.scope=='unopened'
    assert restored_doc.mean_intensity(obj)==96
    assert (restored_doc.pixel_width_um,restored_doc.pixel_height_um)==(.25,.5)
    assert restored_doc.annotation(obj).name==doc.annotation(obj).name
    linked.close();assert old_doc.mean_intensity(old_obj)==16


def test_failed_relink_leaves_source_generation_path_and_clean_state(tmp_path):
    original=ppm(tmp_path/'original.ppm',16);p=q.Project();image=p.add_image(str(original),reader='ppm')
    doc=p.document(image);obj=doc.add_rectangle(q.Rect(1,2,20,16));p.save(str(tmp_path/'saved.qpxproj'))
    before=q.reader_source_json(doc.server.reader_source);path=p.images[0].source_path
    bad=tmp_path/'bad.ppm';bad.write_bytes(b'P6\n1 1\n255\n'+bytes([99])*3)
    with pytest.raises(ValueError,match='match'):p.relink(image,str(bad))
    assert p.images[0].source_path==path and not p.dirty
    assert q.reader_source_json(doc.server.reader_source)==before and doc.mean_intensity(obj)==16


@pytest.mark.skipif(not q.has_tiff,reason='TIFF reader')
def test_relink_retains_verified_tiff_axes_options_and_source(tmp_path):
    p,image,values=scene(tmp_path);doc=p.document(image);doc.set_calibration(.25,.5)
    view=doc.server.tiff_view;view.z=1;view.t=1;view.channels=[1,0,-1]
    view.lower=[0,0,0];view.upper=[5000,5000,5000];p.set_tiff_view(image,view)
    old=doc.server;old_generation=old.reader_source.generation
    candidate=tmp_path/'candidate.ome.tif';shutil.copyfile(tmp_path/'source.ome.tif',candidate)
    p.relink(image,str(candidate));linked=doc.server;source=linked.reader_source
    assert source.primary is not None and source.primary.path==str(candidate)
    assert source.generation!=old_generation and source.scope!='unopened'
    assert linked.tiff_options.cache_bytes==old.tiff_options.cache_bytes
    assert linked.tiff_options.decode_bytes==old.tiff_options.decode_bytes
    assert linked.tiff_view.channels==[1,0,-1] and linked.tiff_view.upper==[5000,5000,5000]
    assert linked.tiff_view.z==1 and linked.tiff_view.t==1
    assert linked.tiff_read_stats.decoded_blocks==0
    raw=request(24,16,c=2,z=1,t=1)
    np.testing.assert_array_equal(linked.read_raw(raw).numpy(),values[1,1].transpose(1,2,0))
    assert linked.reader_source.generation==source.generation
    path=tmp_path/'saved.qpxproj';p.save(str(path));restored=q.Project.open(str(path)).document(image)
    np.testing.assert_array_equal(restored.server.read_raw(raw).numpy(),values[1,1].transpose(1,2,0))
    assert (restored.pixel_width_um,restored.pixel_height_um)==(.25,.5)
    linked.close();np.testing.assert_array_equal(old.read_raw(raw).numpy(),values[1,1].transpose(1,2,0))
