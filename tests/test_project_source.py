"""Saved file facts and reviewed replacement share native project authority."""
from project_schema_contract import CURRENT_SCHEMA_VERSION
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3

import pytest
import qupathxx as q
from test_source_identity import ppm

pytestmark=pytest.mark.skipif(not q.has_project,reason='project storage')


def scene(tmp_path):
    source=ppm(tmp_path/'original.ppm',16);p=q.Project();image=p.add_image(str(source),reader='ppm')
    doc=p.document(image);obj=doc.add_rectangle(q.Rect(1,2,20,16));doc.set_calibration(.25,.5)
    path=tmp_path/'saved.qpxproj';p.save(str(path));return p,image,doc,obj,source,path


def fingerprint(path):
    job=q.FileFingerprintJob();assert job.request(str(path));assert job.wait().state=='succeeded';return job.result


def retain(p,image,path):
    review=p.review_source_verification(image,fingerprint(path));p.apply_source_review(review);return review


def test_initial_verification_is_reviewed_and_reopens_without_source_io(tmp_path):
    p,image,doc,obj,source,path=scene(tmp_path);before=q.reader_source_json(doc.server.reader_source)
    review=p.review_source_verification(image,fingerprint(source))
    assert review.image_id==image and review.path==str(source) and review.comparison=='unverified'
    assert review.coverage=='single_file' and not review.consumed and p.source_verification(image) is None
    assert not p.dirty and q.reader_source_json(doc.server.reader_source)==before
    expected=q.source_file_association_json(review.association);p.apply_source_review(review)
    assert review.consumed and p.dirty and doc.mean_intensity(obj)==16
    assert q.reader_source_json(doc.server.reader_source)==before
    assert p.source_verification(image).fingerprint.sha256==hashlib.sha256(source.read_bytes()).hexdigest()
    p.save(str(path));source.unlink();restored=q.Project.open(str(path))
    assert not restored.images[0].available and restored.document(image).server.reader_source.scope=='unopened'
    assert q.source_file_association_json(restored.source_verification(image))==expected
    with pytest.raises((ValueError,RuntimeError,IndexError)):p.apply_source_review(review)


def test_matching_moved_source_keeps_historical_baseline(tmp_path):
    p,image,doc,obj,source,path=scene(tmp_path);retain(p,image,source)
    baseline=q.source_file_association_json(p.source_verification(image));old=doc.server
    moved=tmp_path/'moved.ppm';shutil.copyfile(source,moved)
    review=p.review_source_verification(image,fingerprint(moved));assert review.comparison=='matching_bytes'
    assert p.images[0].source_path==str(source);p.apply_source_review(review)
    assert p.images[0].source_path==str(moved) and doc.mean_intensity(obj)==16
    assert q.source_file_association_json(p.source_verification(image))==baseline
    p.save(str(path));assert q.source_file_association_json(q.Project.open(str(path)).source_verification(image))==baseline
    assert old.reader_source.generation!=doc.server.reader_source.generation


def test_changed_bytes_require_explicit_acceptance_and_keep_old_analysis(tmp_path):
    p,image,doc,obj,source,path=scene(tmp_path);retain(p,image,source)
    request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]));result=q.analyze(request)
    record=doc.keep_analysis(request,result);historical=q.provenance_json(result.provenance)
    baseline=q.source_file_association_json(p.source_verification(image));p.save(str(path))
    changed=ppm(tmp_path/'changed.ppm',96);review=p.review_source_verification(image,fingerprint(changed))
    assert review.comparison=='changed_bytes' and not p.dirty
    with pytest.raises(ValueError,match='accept_changed'):p.apply_source_review(review)
    with pytest.raises(ValueError,match='reviewed'):p.relink(image,str(changed))
    assert not review.consumed and not p.dirty and doc.mean_intensity(obj)==16
    assert q.source_file_association_json(p.source_verification(image))==baseline
    p.apply_source_review(review,accept_changed=True);assert doc.mean_intensity(obj)==96
    assert (doc.pixel_width_um,doc.pixel_height_um)==(.25,.5)
    assert q.provenance_json(doc.analysis_record(record).result.provenance)==historical
    assert p.source_verification(image).fingerprint.sha256==hashlib.sha256(changed.read_bytes()).hexdigest()
    p.save(str(path));assert q.provenance_json(q.Project.open(str(path)).document(image).analysis_record(record).result.provenance)==historical


@pytest.mark.parametrize('change',['file','baseline','relink','workspace'])
def test_stale_review_or_changed_file_never_mutates_project(tmp_path,change):
    p,image,doc,obj,source,path=scene(tmp_path)
    if change=='baseline':retain(p,image,source)
    review=p.review_source_verification(image,fingerprint(source))
    if change=='file':ppm(source,192)
    elif change=='baseline':p.clear_source_verification(image)
    elif change=='relink':p.relink(image,str(source))
    else:
        other=q.Project();other.add_image(str(source),reader='ppm');p=other
    current=q.reader_source_json(p.document(image).server.reader_source);location=p.images[0].source_path
    with pytest.raises(RuntimeError,match='changed|stale'):p.apply_source_review(review)
    assert not review.consumed and p.images[0].source_path==location
    assert q.reader_source_json(p.document(image).server.reader_source)==current


def test_failed_write_and_async_snapshot_keep_their_own_verification(tmp_path):
    p,image,doc,obj,source,path=scene(tmp_path);retain(p,image,source);p.save(str(path))
    baseline=q.source_file_association_json(p.source_verification(image));generation=p.generation
    with sqlite3.connect(path) as db:db.execute("CREATE TRIGGER fail_verification BEFORE INSERT ON source_verifications BEGIN SELECT RAISE(ABORT,'source verification write failed'); END")
    p.clear_source_verification(image);retain(p,image,source)
    with pytest.raises(RuntimeError,match='write failed'):p.save(str(path))
    assert p.dirty and p.generation==generation
    assert q.source_file_association_json(q.Project.open(str(path)).source_verification(image))==baseline
    with sqlite3.connect(path) as db:db.execute('DROP TRIGGER fail_verification')
    captured=q.source_file_association_json(p.source_verification(image));output=tmp_path/'async.qpxproj'
    job=q.ProjectSaveJob();assert job.request(p,str(output));p.clear_source_verification(image)
    assert job.wait().state=='saved' and p.dirty
    assert q.source_file_association_json(q.Project.open(str(output)).source_verification(image))==captured


@pytest.mark.parametrize('failure',['checksum','truncated','orphan','embedded'])
def test_invalid_verification_rows_are_refused(tmp_path,failure):
    p,image,doc,obj,source,path=scene(tmp_path);retain(p,image,source)
    if failure=='embedded':p.add_document(q.Document(q.Image.demo()),'Embedded')
    p.save(str(path))
    with sqlite3.connect(path) as db:
        if failure=='checksum':
            value=bytearray(db.execute('SELECT payload FROM source_verifications').fetchone()[0]);value[-1]^=1;db.execute('UPDATE source_verifications SET payload=?',(value,))
        elif failure=='truncated':db.execute("UPDATE source_verifications SET payload=x'51535601'")
        elif failure=='orphan':db.execute("UPDATE source_verifications SET image_id='999'")
        else:db.execute("UPDATE source_verifications SET image_id='2'")
    with pytest.raises((ValueError,RuntimeError)):q.Project.open(str(path))


def test_schema14_migration_and_rollback_preserve_old_database(tmp_path):
    p,image,doc,obj,source,path=scene(tmp_path)
    with sqlite3.connect(path) as db:db.execute("DROP TABLE IF EXISTS image_data_properties");db.execute("DROP TABLE IF EXISTS image_analysis_settings");db.execute("DROP TABLE IF EXISTS image_properties");db.execute('DROP TABLE object_planes');db.execute('DROP TABLE object_properties');db.execute('DROP TABLE source_verifications');db.execute('PRAGMA user_version=14');db.execute("CREATE TRIGGER fail_migration BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'source migration failed'); END")
    restored=q.Project.open(str(path));assert restored.source_verification(image) is None
    retain(restored,image,source)
    with pytest.raises(RuntimeError,match='source migration failed'):restored.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('PRAGMA user_version').fetchone()[0]==14
        assert db.execute("SELECT count(*) FROM sqlite_schema WHERE name='source_verifications'").fetchone()[0]==0
        db.execute('DROP TRIGGER fail_migration')
    restored.save(str(path));assert q.Project.open(str(path)).source_verification(image) is not None
    with sqlite3.connect(path) as db:assert db.execute('PRAGMA user_version').fetchone()[0]==CURRENT_SCHEMA_VERSION


def test_reverification_of_unchanged_live_source_keeps_clean_state(tmp_path):
    p,image,doc,obj,source,path=scene(tmp_path);retain(p,image,source);p.save(str(path))
    before=q.reader_source_json(doc.server.reader_source);baseline=q.source_file_association_json(p.source_verification(image))
    review=p.review_source_verification(image,fingerprint(source));assert not review.requires_acceptance
    p.apply_source_review(review)
    assert review.consumed and not p.dirty and q.reader_source_json(doc.server.reader_source)==before
    assert q.source_file_association_json(p.source_verification(image))==baseline


def test_unverified_but_observed_source_change_requires_acceptance(tmp_path):
    p,image,doc,obj,source,path=scene(tmp_path);ppm(source,96)
    review=p.review_source_verification(image,fingerprint(source))
    assert review.comparison=='changed_observed_source' and review.requires_acceptance
    with pytest.raises(ValueError,match='accept_changed'):p.apply_source_review(review)
    assert not p.dirty and doc.mean_intensity(obj)==16 and p.source_verification(image) is None
    p.apply_source_review(review,accept_changed=True);assert doc.mean_intensity(obj)==96


@pytest.mark.skipif(not q.has_tiff,reason='compound TIFF reader')
def test_compound_verification_is_explicitly_primary_only_and_guards_companions(tmp_path):
    from test_binaryonly_ome import fixture
    directory=tmp_path/'compound';manifest=fixture.generate(directory,'xml')
    primary=directory/fixture.planes.FILES[0];p=q.Project();image=p.add_tiff_image(str(primary))
    review=p.review_source_verification(image,fingerprint(primary))
    assert review.coverage=='compound_primary_only' and review.association.status=='matching_primary_metadata'
    p.apply_source_review(review);path=tmp_path/'compound.qpxproj';p.save(str(path))
    historical=q.source_file_association_json(p.source_verification(image))
    assert q.source_file_association_json(q.Project.open(str(path)).source_verification(image))==historical
    pending=p.review_source_verification(image,fingerprint(primary));metadata=directory/manifest['metadata_file']
    with metadata.open('ab') as stream:stream.write(b' ')
    with pytest.raises(RuntimeError,match='changed'):p.apply_source_review(pending)
    assert not pending.consumed and q.source_file_association_json(p.source_verification(image))==historical
    fresh=p.review_source_verification(image,fingerprint(primary))
    assert fresh.comparison=='changed_dependencies' and fresh.requires_acceptance
    with pytest.raises(ValueError,match='accept_changed'):p.apply_source_review(fresh)
    p.apply_source_review(fresh,accept_changed=True)
    assert p.source_verification(image).fingerprint.sha256==pending.association.fingerprint.sha256



@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in source verification desktop')
@pytest.mark.parametrize('changed,accept',[(False,False),(True,False),(True,True)])
def test_desktop_hash_review_apply_and_save(tmp_path,changed,accept):
    from test_gui_replay import event
    p,image,doc,obj,source,path=scene(tmp_path)
    if changed:retain(p,image,source);p.save(str(path))
    candidate=ppm(tmp_path/'candidate.ppm',96) if changed else source
    options=q.GuiOptions();options.file_fingerprint_path=str(candidate);options.settle_tiles=True
    inputs=[event(4,q.GuiInputKind.FINGERPRINT_FILE_KEY),event(6,q.GuiInputKind.REVIEW_SOURCE_VERIFICATION_KEY,wait=q.GuiInputWait.FILE_FINGERPRINT_READY)]
    if accept:
        inputs.extend([event(8,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.ACCEPT_SOURCE_REPLACEMENT,q.GuiInputWait.TARGET_READY),event(9,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.ACCEPT_SOURCE_REPLACEMENT)])
    if not changed or accept:
        inputs.extend([event(12,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.APPLY_SOURCE_VERIFICATION,q.GuiInputWait.TARGET_READY),event(13,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.APPLY_SOURCE_VERIFICATION)])
    if not changed or accept:inputs.append(event(24,q.GuiInputKind.SAVE_KEY))
    options.inputs=inputs
    report=q.show(p,frame_limit=50,capture_path=str(tmp_path/'source-review.ppm'),options=options)
    assert report.replay_complete and report.file_fingerprint_state=='succeeded'
    expected=96 if accept else 16;assert doc.mean_intensity(obj)==expected
    verification=p.source_verification(image)
    assert verification is not None
    expected_path=candidate if accept else source
    assert verification.fingerprint.sha256==hashlib.sha256(expected_path.read_bytes()).hexdigest()
    if not changed or accept:assert not p.dirty
    else:assert p.images[0].source_path==str(source)
    assert q.Project.open(str(path)).source_verification(image).fingerprint.sha256==verification.fingerprint.sha256
