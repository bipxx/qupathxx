"""Reader generations and metadata association never invent historical byte proof."""
import gc
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys

import pytest
import qupathxx as q
from test_tissue_levels import pyramid_levels
from test_provenance import legacy_provenance_blob


def ppm(path,value=16):
    path.write_bytes(b'P6\n32 24\n255\n'+bytes([value])*32*24*3)
    return path


def open_ppm(path):
    if not q.has_project:pytest.skip('native project image factory')
    project=q.Project();image=project.add_image(str(path),reader='ppm');return project.document(image).server


def saved(path):
    server=open_ppm(path);doc=q.Document(server);obj=doc.add_rectangle(q.Rect(1,2,20,16))
    request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]));result=q.analyze(request);record=doc.keep_analysis(request,result)
    return server,doc,obj,request,result,record


def test_file_observation_and_fingerprint_share_native_identity_and_stamps(tmp_path):
    path=ppm(tmp_path/'\u96ea.ppm');server=open_ppm(path);source=server.reader_source;version=source.primary
    fingerprint=q.fingerprint_file(str(path));association=q.associate_source_file(source,fingerprint)
    assert source.scope=='single_file' and source.reader=='ppm' and source.locator==version.path==str(path.resolve())
    assert source.generation and version.bytes==len(path.read_bytes()) and version.observed_unix_ms>0
    assert version.identity==fingerprint.identity and version.identity_scheme==fingerprint.identity_scheme
    assert version.stamp_scheme==fingerprint.stamp_scheme and version.modified_token==fingerprint.modified_token and version.change_token==fingerprint.change_token
    assert association.status=='matching_metadata' and association.source.generation==source.generation
    assert association.fingerprint.sha256==hashlib.sha256(path.read_bytes()).hexdigest()
    data=json.loads(q.source_file_association_json(association));assert data['source']==json.loads(q.reader_source_json(source))
    assert data['fingerprint']==json.loads(q.file_fingerprint_json(fingerprint)) and data['status']=='matching_metadata'
    for value,field in [(source,'generation'),(version,'identity'),(association,'status')]:
        with pytest.raises(AttributeError):setattr(value,field,'changed')
    assert q.reader_source_json(server.reader_source)==q.reader_source_json(source)


def test_reopened_and_memory_readers_have_distinct_generations(tmp_path):
    path=ppm(tmp_path/'source.ppm');a=open_ppm(path);b=open_ppm(path)
    assert a.reader_source.generation!=b.reader_source.generation and a.reader_source.primary.identity==b.reader_source.primary.identity
    doc=q.Document(q.Image.demo());source=doc.server.reader_source
    assert source.scope=='memory' and source.primary is None and source.locator==''
    assert q.associate_source_file(source,q.fingerprint_file(str(path))).status=='unobserved'
    job=q.FileFingerprintJob()
    with pytest.raises(ValueError,match='locator'):job.request_source(source)


def test_same_path_replacement_is_a_different_file_and_original_pixels_survive(tmp_path):
    path=ppm(tmp_path/'source.ppm');server,doc,obj,request,result,record=saved(path);source=server.reader_source
    replacement=ppm(tmp_path/'replacement.ppm',128);os.replace(replacement,path)
    association=q.associate_source_file(source,q.fingerprint_file(str(path)))
    assert association.status=='different_file' and source.primary.identity!=association.fingerprint.identity
    assert q.analyze(q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]))).input_sha256==result.input_sha256
    assert result.provenance.reader_source.generation==source.generation and doc.analysis_status(record)=='current'


def test_changed_content_and_stamp_on_the_same_file_is_not_a_match(tmp_path):
    path=ppm(tmp_path/'source.ppm');server=open_ppm(path);source=server.reader_source
    ppm(path,128);stamp=path.stat();os.utime(path,ns=(stamp.st_atime_ns,stamp.st_mtime_ns+2_000_000_000))
    association=q.associate_source_file(source,q.fingerprint_file(str(path)))
    assert association.source.primary.identity==association.fingerprint.identity and association.status=='version_changed'
    assert association.source.primary.modified_token!=association.fingerprint.modified_token


def test_cached_snapshot_capture_and_close_do_not_reopen_a_removed_file(tmp_path):
    path=ppm(tmp_path/'source.ppm');server,doc,obj,request,result,record=saved(path);source=server.reader_source
    text=q.reader_source_json(source);path.unlink()
    captured=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]])).provenance
    assert q.reader_source_json(captured.reader_source)==text
    server.close();del server,doc,request,result;gc.collect();assert q.reader_source_json(source)==text


@pytest.mark.skipif(not q.has_project,reason='native project')
def test_archived_reader_generation_survives_save_recovery_and_new_live_generation(tmp_path):
    path=ppm(tmp_path/'source.ppm');server,doc,obj,request,result,record=saved(path);source=server.reader_source
    project=q.Project();image=project.add_document(doc,source_path=str(path),reader='ppm');primary=tmp_path/'source.qpxproj';project.save(str(primary))
    expected=q.provenance_json(result.provenance);loaded=q.Project.open(str(primary));live=loaded.document(image)
    assert q.provenance_json(live.analysis_record(record).result.provenance)==expected
    assert live.server.reader_source.scope=='unopened' and live.server.reader_source.primary is None
    before=live.server.reader_source;first=q.AnalysisRequest(live,obj,q.StainTransform([[1,0,0],[0,1,0]]));q.analyze(first)
    assert first.provenance.reader_source.scope=='unopened' and before.generation==live.server.reader_source.generation
    assert live.server.reader_source.scope=='single_file' and live.server.reader_source.primary.identity==source.primary.identity
    assert before.generation!=source.generation
    fresh=q.AnalysisRequest(live,obj,q.StainTransform([[1,0,0],[0,1,0]]))
    assert fresh.provenance.reader_source.primary is not None and q.provenance_json(live.analysis_record(record).result.provenance)==expected
    project.rename_image(image,'Later image name')
    recovery=tmp_path/'source.qpxrecover';job=q.ProjectAutosave();assert job.request(project,str(recovery));assert job.wait().state=='ready'
    recovered=q.Project.recover(str(recovery));assert q.provenance_json(recovered.document(image).analysis_record(record).result.provenance)==expected


@pytest.mark.skipif(not q.has_project,reason='native project')
def test_qpv1_legacy_payload_does_not_invent_a_reader_generation(tmp_path):
    doc=q.Document(q.Image(32,24,[16]*32*24*3));obj=doc.add_rectangle(q.Rect(1,2,20,16));request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]));record=doc.keep_analysis(request,q.analyze(request))
    project=q.Project();image=project.add_document(doc);path=tmp_path/'legacy.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as database:
        key,blob=database.execute('SELECT record_id,payload FROM analysis_provenance').fetchone();data=bytearray(blob);assert data[:4]==b'QPV\x02'
        legacy=legacy_provenance_blob(data)
        database.execute('UPDATE analysis_provenance SET payload=? WHERE record_id=?',(legacy,key))
    loaded=q.Project.open(str(path));assert loaded.document(image).analysis_record(record).result.provenance.reader_source is None


@pytest.mark.skipif(not q.has_openslide,reason='native pyramid')
def test_openslide_primary_file_is_observed_without_claiming_complete_dependencies(pyramid_levels):
    server,_=pyramid_levels;source=server.reader_source;assert source.scope=='primary_file' and source.primary is not None
    assert q.associate_source_file(source,q.fingerprint_file(source.locator)).status=='matching_metadata'
    doc=q.Document(server);obj=doc.add_rectangle(q.Rect(0,0,32,32));request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]))
    assert request.provenance.reader_source.generation==source.generation
    server.close();assert request.provenance.reader_source.primary.identity==source.primary.identity


def test_worker_association_output_is_atomic_and_alias_safe(tmp_path):
    path=ppm(tmp_path/'source.ppm');source=open_ppm(path).reader_source;output=tmp_path/'source.json';job=q.FileFingerprintJob()
    assert job.request_source(source,str(output));assert job.wait().state=='succeeded'
    assert output.read_bytes()==q.source_file_association_json(job.association).encode() and job.association.status=='matching_metadata'
    previous=output.read_bytes();options=q.FileFingerprintOptions();options.byte_limit=1
    assert job.request_source(source,str(output),options);assert job.wait().state=='failed' and job.association is None and output.read_bytes()==previous
    with pytest.raises(ValueError):q.save_source_file_association(q.associate_source_file(source,q.fingerprint_file(str(path))),str(path))
    alias=tmp_path/'hardlink.ppm';os.link(path,alias)
    with pytest.raises(ValueError):q.save_source_file_association(q.associate_source_file(source,q.fingerprint_file(str(path))),str(alias))
    assert job.request(str(path));assert job.wait().state=='succeeded' and job.association is None
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


def test_worker_captures_native_values_without_python_source_owners(tmp_path):
    script=r'''
import gc,pathlib,sys
import qupathxx as q
path=pathlib.Path(sys.argv[1])/'child.ppm';path.write_bytes(b'P6\n2 2\n255\n'+bytes(range(12)))
project=q.Project();image=project.add_image(str(path),reader='ppm');server=project.document(image).server;source=server.reader_source;generation=source.generation;job=q.FileFingerprintJob()
assert job.request_source(source,str(path)+'.json');del source,server,project;gc.collect();assert job.wait().state=='succeeded'
association=job.association;del job;gc.collect();assert association.source.generation==generation and association.status=='matching_metadata'
'''
    child=subprocess.run([sys.executable,'-I','-c',script,str(tmp_path)],capture_output=True,text=True,timeout=20)
    assert child.returncode==0,child.stdout+child.stderr


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='desktop replay')
@pytest.mark.parametrize('changed',[False,True])
def test_gui_associates_active_reader_generation_and_exports_json(tmp_path,changed):
    path=ppm(tmp_path/'source.ppm');server,doc,_,_,_,_=saved(path);source=server.reader_source
    if changed:
        ppm(path,128);stamp=path.stat();os.utime(path,ns=(stamp.st_atime_ns,stamp.st_mtime_ns+2_000_000_000))
    output=tmp_path/'association.json';options=q.GuiOptions();options.file_fingerprint_associate_source=True;options.file_fingerprint_output=str(output)
    start=q.GuiInput();start.frame,start.kind=4,q.GuiInputKind.FINGERPRINT_FILE_KEY
    ready=q.GuiInput();ready.frame,ready.kind,ready.wait_for=6,q.GuiInputKind.MOVE,q.GuiInputWait.FILE_FINGERPRINT_READY;options.inputs=[start,ready]
    report=q.show(doc,frame_limit=20,options=options)
    assert report.file_fingerprint_state=='succeeded' and not report.file_fingerprint_error
    association=report.file_fingerprint_association;assert association.source.generation==source.generation
    assert association.status==('version_changed' if changed else 'matching_metadata')
    assert output.read_bytes()==q.source_file_association_json(association).encode()


@pytest.mark.skipif(not q.has_project,reason='native project')
@pytest.mark.parametrize('mode',['locator','generation','scope','size'])
def test_corrupt_reader_observations_are_rejected_even_with_updated_checksum(tmp_path,mode):
    path=ppm(tmp_path/'source.ppm');server,doc,_,_,result,record=saved(path);project=q.Project();project.add_document(doc)
    primary=tmp_path/'corrupt.qpxproj';project.save(str(primary))
    with sqlite3.connect(primary) as database:
        key,blob=database.execute('SELECT record_id,payload FROM analysis_provenance').fetchone();data=bytearray(blob);source=result.provenance.reader_source
        if mode=='locator':
            encoded=source.locator.encode();at=data.find(encoded,data.find(encoded)+len(encoded));assert at>=0;data[at+len(encoded)-1]^=1
        elif mode=='generation':at=data.index(source.generation.encode());data[at]=0
        elif mode=='scope':at=data.index(b'single_file');data[at:at+11]=b'invalid____'
        else:struct.pack_into('<Q',data,len(data)-80,2**64-1)
        data[-64:]=hashlib.sha256(data[:-64]).hexdigest().encode();database.execute('UPDATE analysis_provenance SET payload=? WHERE record_id=?',(data,key))
    before=primary.read_bytes()
    with pytest.raises(ValueError):q.Project.open(str(primary))
    assert primary.read_bytes()==before


def test_different_file_association_cannot_replace_reader_source_or_renamed_alias(tmp_path):
    path=ppm(tmp_path/'reader.ppm');source=open_ppm(path).reader_source
    other=ppm(tmp_path/'other.ppm',128);association=q.associate_source_file(source,q.fingerprint_file(str(other)))
    assert association.status=='different_file'
    original=path.read_bytes()
    with pytest.raises(ValueError):q.save_source_file_association(association,str(path))
    assert path.read_bytes()==original
    renamed=tmp_path/'renamed.ppm';path.rename(renamed)
    with pytest.raises(ValueError):q.save_source_file_association(association,str(renamed))
    assert renamed.read_bytes()==original
