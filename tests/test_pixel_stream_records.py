"""Schema10 stores historical descriptors; project loading never opens QPS files."""
import hashlib
import os
from pathlib import Path
import shutil
import sqlite3
import struct
import pytest
import numpy as np
import qupathxx as q
from test_pixel_stream_read_job import FIXTURE

pytestmark=pytest.mark.skipif(not q.has_project,reason='project backend')


def scene(tmp_path):
    path=tmp_path/'原始.qps';shutil.copyfile(FIXTURE,path);link=q.prepare_pixel_stream_link(str(path))
    doc=q.Document(q.Image.from_numpy(np.full((link.source_height,link.source_width,3),80,np.uint8)))
    project=q.Project();image=project.add_document(doc,'Linked image');return project,image,doc,path,link


def test_only_complete_verification_prepares_an_immutable_link(tmp_path):
    _,_,_,path,prepared=scene(tmp_path);file=q.PixelStreamFile(str(path))
    with pytest.raises(RuntimeError):file.link()
    file.verify();link=file.link();assert link.same_content(prepared) and link.path==str(path.resolve())
    assert link.summary.sha256==file.declared_summary.sha256 and link.model_sha256==file.read_block(0).model.sha256
    assert link.source_width==64 and link.source_height==48 and link.summary.blocks==12
    assert link.payload_bytes<4096 and link.object_id>0
    with pytest.raises(AttributeError):link.path='forged.qps'
    counts=link.summary.class_counts;counts[0]=0;assert link.summary.class_counts!=counts
    file.cancel()
    with pytest.raises(RuntimeError):file.link()


def test_metadata_record_is_historical_bounded_and_not_an_edit_command(tmp_path):
    project,image,doc,path,link=scene(tmp_path);revision=doc.revision;doc.clear_history()
    record_id=doc.keep_pixel_stream(link,'Whole slide output');record=doc.pixel_stream_record(record_id)
    assert doc.revision==revision+1 and doc.pixel_stream_record_count==1 and record.id==record_id
    assert record.name=='Whole slide output' and record.link is link and record.payload_bytes<8192
    assert doc.pixel_stream_records==[record] and not doc.can_undo and project.dirty
    with pytest.raises(AttributeError):record.name='forged'
    assert doc.remove_pixel_stream_record(record_id) and not doc.remove_pixel_stream_record(record_id)
    assert doc.revision==revision+2 and path.exists() and record.link.summary.blocks==12
    with pytest.raises(IndexError):doc.pixel_stream_record(record_id)


@pytest.mark.parametrize('name',['','x'*257,'bad\x00name'])
def test_rejected_record_names_do_not_change_revision(tmp_path,name):
    _,_,doc,_,link=scene(tmp_path);revision=doc.revision
    with pytest.raises(ValueError):doc.keep_pixel_stream(link,name)
    assert doc.revision==revision and doc.pixel_stream_record_count==0


def test_dimension_mismatch_is_rejected_without_claiming_source_identity(tmp_path):
    _,_,_,_,link=scene(tmp_path);wrong=q.Document(q.Image(1,1,[0,0,0]));revision=wrong.revision
    with pytest.raises(ValueError):wrong.keep_pixel_stream(link)
    assert wrong.revision==revision and wrong.pixel_stream_record_count==0
    # Equal dimensions permit an explicitly historical attachment, not a live request.
    same=q.Document(q.Image.from_numpy(np.zeros((48,64,3),np.uint8)));record=same.keep_pixel_stream(link)
    assert same.pixel_stream_record(record).link is link and same.object_count==0


def test_schema11_roundtrip_and_missing_file_do_not_access_external_values(tmp_path):
    project,image,doc,path,link=scene(tmp_path);record=doc.keep_pixel_stream(link,'Historical output')
    source=tmp_path/'project.qpxproj';project.save(str(source));path.unlink()
    loaded=q.Project.open(str(source));restored=loaded.document(image);saved=restored.pixel_stream_record(record)
    assert saved.link.same_content(link) and saved.link.path==str(path.resolve()) and restored.pixel_stream_record_count==1
    assert not loaded.dirty and restored.object_count==0 and saved.link.summary.blocks==12
    with sqlite3.connect(source) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==12
        blob=db.execute('SELECT payload FROM pixel_stream_records').fetchone()[0]
        assert blob[:4]==b'QSL\1' and len(blob)<4096 and path.name.encode('utf-8') in blob
        assert str(tmp_path).encode('utf-8') not in blob
    job=q.PixelStreamReadJob();assert job.open_link(saved.link);failed=job.wait()
    assert failed.state=='failed' and failed.error and failed.block is None and failed.link is None


def test_project_and_stream_directory_can_move_together(tmp_path):
    original=tmp_path/'original';original.mkdir();project,image,doc,path,link=scene(original);record=doc.keep_pixel_stream(link)
    source=original/'project.qpxproj';project.save(str(source));moved=tmp_path/'moved';original.rename(moved)
    loaded=q.Project.open(str(moved/source.name));descriptor=loaded.document(image).pixel_stream_record(record).link
    assert descriptor.path==str((moved/path.name).resolve()) and descriptor.same_content(link)
    job=q.PixelStreamReadJob();assert job.open_link(descriptor,11);ready=job.wait()
    assert ready.state=='ready' and ready.verified and ready.link.same_content(link) and ready.block_index==11


def test_save_as_rebases_stream_path_without_modifying_descriptor(tmp_path):
    project,image,doc,path,link=scene(tmp_path);record=doc.keep_pixel_stream(link);source=tmp_path/'first.qpxproj';project.save(str(source))
    folder=tmp_path/'nested';folder.mkdir();destination=folder/'other.qpxproj';project.save(str(destination))
    loaded=q.Project.open(str(destination));assert loaded.document(image).pixel_stream_record(record).link.path==link.path
    assert doc.pixel_stream_record(record).link is link
    with sqlite3.connect(destination) as db:assert b'../' in db.execute('SELECT payload FROM pixel_stream_records').fetchone()[0]


def test_relink_requires_exact_verified_content_and_never_deletes_files(tmp_path):
    _,_,doc,path,link=scene(tmp_path);record=doc.keep_pixel_stream(link);copied=tmp_path/'moved.qps';shutil.copyfile(path,copied)
    prepared=q.prepare_pixel_stream_link(str(copied));revision=doc.revision;old=doc.pixel_stream_record(record)
    doc.relink_pixel_stream_record(record,prepared);current=doc.pixel_stream_record(record)
    assert current.link.path==str(copied.resolve()) and current.id==old.id and current.name==old.name and doc.revision==revision+1
    assert old.link.path==str(path.resolve()) and path.exists() and copied.exists()
    other=Path(__file__).parent/'data/pixel-streams/polygon-window-v1-windows.qps'
    if q.has_geos:
        wrong=q.prepare_pixel_stream_link(str(other));revision=doc.revision
        with pytest.raises(ValueError):doc.relink_pixel_stream_record(record,wrong)
        assert doc.revision==revision and doc.pixel_stream_record(record) is current
    doc.remove_pixel_stream_record(record);assert copied.exists() and path.exists()


def test_opening_link_verifies_complete_file_before_exposing_values(tmp_path):
    _,_,_,path,link=scene(tmp_path);data=bytearray(path.read_bytes());at=132+struct.unpack_from('<Q',data,60)[0]+136;data[at]^=1;path.write_bytes(data)
    # The corrupt record is not block zero, so an ordinary indexed open succeeds.
    job=q.PixelStreamReadJob();assert job.open(str(path));assert job.wait().state=='ready'
    assert job.open_link(link);failed=job.wait();assert failed.state=='failed' and failed.block is None and failed.link is None and not failed.verified


def test_complete_different_valid_file_is_rejected_for_saved_link(tmp_path):
    if not q.has_geos:pytest.skip('polygon fixture requires GEOS')
    _,_,_,path,link=scene(tmp_path);other=Path(__file__).parent/'data/pixel-streams/polygon-window-v1-windows.qps';shutil.copyfile(other,path)
    job=q.PixelStreamReadJob();assert job.open_link(link);failed=job.wait()
    assert failed.state=='failed' and 'does not match' in failed.error and failed.block is None


@pytest.mark.parametrize('version',range(1,12))
def test_legacy_schemas_load_and_transactionally_upgrade_to_schema12(tmp_path,version):
    project,image,doc,_,_=scene(tmp_path);source=tmp_path/'legacy.qpxproj';project.save(str(source))
    tables=[(12,'tiff_views'),(11,'tma_cores'),(11,'tma_grids'),(10,'pixel_stream_records'),(9,'pixel_classifier_records'),(8,'classifier_records'),(7,'analysis_provenance'),(6,'cell_feature_links'),(5,'cell_records'),(4,'tissue_records'),(3,'analysis_records'),(2,'views')]
    with sqlite3.connect(source) as db:
        for added,name in tables:
            if added>version:db.execute('DROP TABLE '+name)
        db.execute('PRAGMA user_version='+str(version))
    loaded=q.Project.open(str(source));assert loaded.document(image).pixel_stream_record_count==0
    loaded.save(str(source))
    with sqlite3.connect(source) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==12 and db.execute('SELECT count(*) FROM pixel_stream_records').fetchone()[0]==0


def test_schema9_migration_rolls_back_with_the_original_bytes_intact(tmp_path):
    project,_,_,_,_=scene(tmp_path);source=tmp_path/'legacy.qpxproj';project.save(str(source))
    with sqlite3.connect(source) as db:
        db.execute("DROP TABLE tiff_views");db.execute("DROP TABLE tma_cores");db.execute("DROP TABLE tma_grids")
        db.execute('DROP TABLE pixel_stream_records');db.execute('PRAGMA user_version=9')
        db.execute("CREATE TRIGGER fail_stream_migration BEFORE INSERT ON images BEGIN SELECT RAISE(ABORT,'injected stream migration'); END")
    loaded=q.Project.open(str(source));before=source.read_bytes()
    with pytest.raises(RuntimeError,match='injected stream migration'):loaded.save(str(source))
    assert source.read_bytes()==before
    with sqlite3.connect(source) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==9 and db.execute("SELECT count(*) FROM sqlite_schema WHERE name='pixel_stream_records'").fetchone()[0]==0
        db.execute('DROP TRIGGER fail_stream_migration')
    loaded.save(str(source))
    with sqlite3.connect(source) as db:assert db.execute('PRAGMA user_version').fetchone()[0]==12


@pytest.mark.parametrize('mutation',['checksum','id','orphan','source','bounds','class-counts','trailing'])
def test_corrupt_or_forged_stream_record_rows_are_rejected(tmp_path,mutation):
    project,_,doc,_,link=scene(tmp_path);record=doc.keep_pixel_stream(link);source=tmp_path/'corrupt.qpxproj';project.save(str(source))
    with sqlite3.connect(source) as db:
        blob=bytearray(db.execute('SELECT payload FROM pixel_stream_records').fetchone()[0]);at=12
        for _ in range(4):n=struct.unpack_from('<Q',blob,at)[0];at+=8+n
        # Four variable texts follow record ID: name/path/model/content; then15 integers.
        if mutation=='checksum':blob[-1]^=1
        elif mutation=='id':db.execute('UPDATE pixel_stream_records SET id=?',(str(record+1),))
        elif mutation=='orphan':db.execute("UPDATE pixel_stream_records SET image_id='999'")
        else:
            if mutation=='source':struct.pack_into('<Q',blob,at,65)
            elif mutation=='bounds':struct.pack_into('<Q',blob,at+4*8,999)
            elif mutation=='class-counts':struct.pack_into('<Q',blob,at+14*8,1025)
            elif mutation=='trailing':blob[-64:-64]=b'junk'
            blob[-64:]=hashlib.sha256(blob[:-64]).hexdigest().encode()
        if mutation not in ['id','orphan']:db.execute('UPDATE pixel_stream_records SET payload=?',(bytes(blob),))
    with pytest.raises((ValueError,RuntimeError,IndexError)):q.Project.open(str(source))


def test_autosave_recovery_and_background_save_capture_links_without_reading_file(tmp_path):
    project,image,doc,path,link=scene(tmp_path);record=doc.keep_pixel_stream(link);source=tmp_path/'project.qpxproj';project.save(str(source));doc.keep_pixel_stream(link,'After primary save')
    recovery=tmp_path/'snapshots/recovery.qpxrecover';recovery.parent.mkdir();autosave=q.ProjectAutosave();assert autosave.request(project,str(recovery));assert autosave.wait().completed==1
    path.unlink();restored=q.Project.recover(str(recovery));assert restored.document(image).pixel_stream_record(record).link.same_content(link)
    destination=tmp_path/'background.qpxproj';save=q.ProjectSaveJob();assert save.request(project,str(destination));assert save.wait().state=='saved'
    assert q.Project.open(str(destination)).document(image).pixel_stream_record(record).link.same_content(link)


def test_batch_checkpoints_preserve_links_and_digest_the_descriptor_rows(tmp_path):
    project,image,doc,path,link=scene(tmp_path);doc.add_rectangle(q.Rect(4,4,16,16),'Batch area');record=doc.keep_pixel_stream(link)
    request=q.ProjectBatchRequest(project,q.StainTransform([[1,0,0],[0,1,0]]));path.unlink()
    output=tmp_path/'batch';first=q.run_project_batch(request,str(output));assert first.state=='succeeded' and first.saved==1
    checkpoint=Path(first.images[0].path);restored=q.Project.open(str(checkpoint));assert restored.document(image).pixel_stream_record(record).link.same_content(link)
    assert restored.document(image).pixel_stream_record(record).link.path.removeprefix('\\\\?\\')==str(path.resolve())
    second=q.run_project_batch(request,str(output));assert second.reused==1 and second.state=='succeeded'
    with sqlite3.connect(checkpoint) as db:
        data=bytearray(db.execute('SELECT payload FROM pixel_stream_records').fetchone()[0]);length=struct.unpack_from('<Q',data,12)[0];assert length>0
        data[20]^=1;data[-64:]=hashlib.sha256(data[:-64]).hexdigest().encode();db.execute('UPDATE pixel_stream_records SET payload=?',(bytes(data),))
    rejected=q.run_project_batch(request,str(output));assert rejected.failed==1 and rejected.reused==0 and rejected.state=='failed'
