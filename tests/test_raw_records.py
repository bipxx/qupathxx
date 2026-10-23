"""Immutable native raw records, project archives and explicit replay proofs."""
import gc
import hashlib
import sqlite3
import subprocess
import sys
import textwrap

import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_tiff_reader import request

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_project,reason='TIFF project support')


def scene(tmp_path):
    source=tmp_path/'samples.tif'
    values=np.arange(16*24,dtype='uint16').reshape(16,24)
    tifffile.imwrite(source,values,metadata=None)
    project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
    obj=doc.add_rectangle(q.Rect(1,2,5,6),'Original')
    capture=q.RawAnalysisRequest(doc,request(1,1),obj,tile_size=16)
    result=q.analyze_raw(capture);record=doc.keep_raw_analysis(capture,result)
    return project,doc,image,obj,record,result,source


def test_save_reopen_requires_explicit_verification_and_preserves_bytes(tmp_path):
    project,doc,image,obj,record,result,_=scene(tmp_path)
    path=tmp_path/'saved.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==13
        original=db.execute('SELECT payload FROM raw_analysis_records').fetchone()[0]
    restored=q.Project.open(str(path));current=restored.document(image)
    assert current.raw_analysis_status(record)=='unverified'
    replay=q.RawAnalysisRequest.replay(current,record);proof=q.analyze_raw(replay)
    revision=current.revision
    assert current.verify_raw_analysis(record,replay,proof)
    assert current.revision==revision and not restored.dirty
    assert current.raw_analysis_status(record)=='current'
    assert proof.input_sha256==result.input_sha256
    restored.save(str(path))
    with sqlite3.connect(path) as db:assert db.execute('SELECT payload FROM raw_analysis_records').fetchone()[0]==original


@pytest.mark.parametrize('mutation',['checksum','truncated','row_id','image_id','semantic'])
def test_archive_corruption_is_rejected(tmp_path,mutation):
    project,_,_,_,_,_,_=scene(tmp_path);path=tmp_path/'corrupt.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        blob=bytearray(db.execute('SELECT payload FROM raw_analysis_records').fetchone()[0])
        if mutation=='row_id':db.execute("UPDATE raw_analysis_records SET id='999999'")
        elif mutation=='image_id':db.execute("UPDATE raw_analysis_records SET image_id='999999'")
        else:
            if mutation=='truncated':blob=blob[:20]
            elif mutation=='checksum':blob[40]^=1
            else:
                # Valid envelope checksum, impossible requested tile size.
                blob[12:20]=(0).to_bytes(8,'little')
                blob[-64:]=hashlib.sha256(blob[:-64]).hexdigest().encode()
            db.execute('UPDATE raw_analysis_records SET payload=?',(blob,))
    with pytest.raises((ValueError,RuntimeError,IndexError)):q.Project.open(str(path))


def test_capture_authority_immutability_and_display_independence(tmp_path):
    project,doc,image,obj,record,result,_=scene(tmp_path)
    capture=q.RawAnalysisRequest.replay(doc,record)
    view=doc.server.tiff_view;view.upper=[100,100,100];doc.set_tiff_view(view)
    assert capture.matches(doc) and doc.raw_analysis_status(record)=='current'
    copy=result.region;copy.channels=2
    assert result.region.channels==1
    for operation in [lambda:setattr(result,'input_sha256','0'*64),lambda:setattr(result.metadata,'width',1),lambda:setattr(doc.raw_analysis_record(record),'id',1)]:
        with pytest.raises(AttributeError):operation()
    foreign=q.RawAnalysisRequest(doc,request(1,1),obj,tile_size=16)
    with pytest.raises(ValueError):doc.keep_raw_analysis(foreign,result)
    held=doc.raw_analysis_record(record)
    assert doc.latest_raw_analysis(obj).id==record and doc.raw_analysis_page(limit=1)[0].id==record
    assert doc.remove_raw_analysis(record) and not doc.remove_raw_analysis(record)
    assert doc.latest_raw_analysis(obj) is None and doc.raw_analysis_record_count==0
    doc.server.close();del project,doc,result;gc.collect()
    assert held.result.metadata.width==24 and held.result.statistics.valid_pixels==30


def test_save_snapshot_survives_removal_from_live_catalog(tmp_path):
    # Run the GIL/lifetime boundary in a child so a regression cannot hang pytest.
    code=textwrap.dedent("""
        import gc,sys
        import numpy as np
        import qupathxx as q
        import tifffile
        from pathlib import Path
        root=Path(sys.argv[1]);source=root/'source.tif'
        tifffile.imwrite(source,np.arange(16*24,dtype='uint16').reshape(16,24),metadata=None)
        project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
        raw=q.RawRegionRequest();raw.region=q.RegionRequest(0,0,0,24,16)
        capture=q.RawAnalysisRequest(doc,raw);result=q.analyze_raw(capture)
        record=doc.keep_raw_analysis(capture,result);del result;gc.collect()
        path=root/'snapshot.qpxproj';job=q.ProjectSaveJob()
        assert job.request(project,str(path));assert doc.remove_raw_analysis(record)
        assert job.wait().state=='saved' and project.dirty
        assert q.Project.open(str(path)).document(image).raw_analysis_record_count==1
        assert doc.raw_analysis_record_count==0
    """)
    done=subprocess.run([sys.executable,'-I','-c',code,str(tmp_path)],capture_output=True,text=True,encoding='utf-8',timeout=30)
    assert done.returncode==0,done.stderr


def test_uint64_one_bit_change_fails_replay_even_when_mean_is_equal(tmp_path):
    source=tmp_path/'wide.tif';values=np.full((4,4),2**63,dtype='uint64')
    tifffile.imwrite(source,values,metadata=None)
    project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
    capture=q.RawAnalysisRequest(doc,request(4,4),tile_size=16);before=q.analyze_raw(capture)
    record=doc.keep_raw_analysis(capture,before);path=tmp_path/'wide.qpxproj';project.save(str(path));doc.server.close()
    values[0,0]+=np.uint64(1);tifffile.imwrite(source,values,metadata=None)
    restored=q.Project.open(str(path));current=restored.document(image)
    replay=q.RawAnalysisRequest.replay(current,record);after=q.analyze_raw(replay)
    assert before.statistics.means==after.statistics.means
    assert before.input_sha256!=after.input_sha256
    revision=current.revision
    assert not current.verify_raw_analysis(record,replay,after)
    assert current.raw_analysis_status(record)=='verification_failed' and current.revision==revision


@pytest.mark.parametrize('direction',['between','out','into'])
def test_parent_reparenting_invalidates_descendant_case_context(tmp_path,direction):
    project,doc,_,_,_,_,_=scene(tmp_path)
    grid=q.TmaGridOptions();grid.rows=1;grid.columns=2;grid.origin=q.Point(8,8)
    grid.column_step=q.Point(2,0);grid.row_step=q.Point(0,2);grid.width=grid.height=12
    cores=doc.create_tma_grid(grid);doc.set_tma_case_ids({cores[0]:'A',cores[1]:'B'})
    parent=doc.add_rectangle(q.Rect(8,7,2,2),'Parent');child=doc.add_rectangle(q.Rect(8.5,7.5,1,1),'Child')
    if direction!='into':doc.reparent_annotation(parent,cores[0])
    doc.reparent_annotation(child,parent)
    capture=q.RawAnalysisRequest(doc,request(1,1),child);result=q.analyze_raw(capture)
    record=doc.keep_raw_analysis(capture,result);before=doc.annotation(child).revision
    destination=cores[1] if direction=='between' else 0 if direction=='out' else cores[0]
    doc.reparent_annotation(parent,destination)
    assert doc.annotation(child).revision==before
    assert not capture.matches(doc) and doc.raw_analysis_status(record)=='context_changed'
    with pytest.raises(ValueError):doc.keep_raw_analysis(capture,result)
    with pytest.raises(ValueError):q.RawAnalysisRequest.replay(doc,record)
    with pytest.raises(ValueError):q.RawTableRequest(doc,[record],True)
    after=q.analyze_raw(q.RawAnalysisRequest(doc,request(1,1),child))
    assert after.input_sha256==result.input_sha256
    assert (after.object_context.tma.case_id if after.object_context.tma else None)==('B' if direction=='between' else None if direction=='out' else 'A')
