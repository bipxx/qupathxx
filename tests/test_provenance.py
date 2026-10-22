import hashlib
import json
from pathlib import Path
import sqlite3
import struct
import time

import pytest
import qupathxx as q
from test_cells import phantom
from test_tissue import document
from test_tissue_levels import pyramid_levels


def legacy_provenance_blob(blob):
    """Retain the original QPV1 fields, without synthesizing reader identity."""
    data=bytearray(blob)
    if data[:4]==b"QPV\x01":return data
    assert data[:4]==b"QPV\x02"
    at=60
    for _ in range(3):size=struct.unpack_from("<Q",data,at)[0];at+=8+size
    for _ in range(4):present=struct.unpack_from("<Q",data,at)[0];at+=8+8*present
    levels=struct.unpack_from("<Q",data,at)[0];at+=8+24*levels
    for _ in range(11):size=struct.unpack_from("<Q",data,at)[0];at+=8+size
    at+=24;legacy=data[:at];legacy[3]=1
    return legacy+hashlib.sha256(legacy).hexdigest().encode()


def results():
    doc=document(phantom(64));obj=doc.add_rectangle(q.Rect(0,0,64,64));doc.set_calibration(.5,.25)
    transform=q.StainTransform([[1,0,0],[0,1,0]])
    roi_request=q.AnalysisRequest(doc,obj,transform);roi=q.analyze(roi_request);roi_id=doc.keep_analysis(roi_request,roi)
    tissue_request=q.TissueRequest(doc,obj,transform);tissue=q.detect_tissue(tissue_request);tissue_id=doc.keep_tissue(tissue_request,tissue)
    cell_request=q.CellRequest(doc,obj,transform);cells=q.detect_cells(cell_request);cell_id=doc.keep_cells(cell_request,cells)
    return doc,obj,(roi_id,tissue_id,cell_id),(roi,tissue,cells)


def test_capture_precedes_later_calibration_and_results_retain_immutable_native_provenance():
    doc=document(phantom(64));obj=doc.add_rectangle(q.Rect(0,0,64,64));doc.set_calibration(.5,.25)
    before=int(time.time()*1000);request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]));p=request.provenance
    doc.set_calibration(3,4);result=q.analyze(request)
    assert result.provenance is p and p.effective_pixel_width_um==.5 and p.effective_pixel_height_um==.25
    assert p.source.pixel_width_um is None and p.source.pixel_height_um is None and p.reader=="qpx.dense-rgb/1" and p.locator==""
    assert p.object_id==obj and p.object_revision==doc.annotation(obj).revision and p.document_revision<doc.revision
    # MSVC system_clock and CPython's precise Windows clock can differ by a tick.
    assert before-20<=p.captured_unix_ms<=int(time.time()*1000)+20
    with pytest.raises(AttributeError):p.reader="changed"
    with pytest.raises(AttributeError):p.environment.compiler="changed"
    p.source.levels.clear();assert len(p.source.levels)==1
    captured=q.provenance_json(p);del doc,request,result;assert q.provenance_json(p)==captured


def test_environment_describes_build_and_actual_enabled_native_dependencies():
    e=q.build_environment();assert e.app_version=="0.1.0" and e.compiler and e.configuration and e.platform and e.cmake_version
    assert len(e.source_tree_sha256)==64 and set(e.source_tree_sha256)<=set("0123456789abcdef")
    assert bool(e.geos_version)==q.has_geos and bool(e.openslide_version)==q.has_openslide and bool(e.sqlite_version)==q.has_project
    assert e.gui==q.has_gui and e.python and e.interchange==q.has_interchange
    assert q.build_environment() is e


def test_source_fingerprint_matches_configured_source_byte_manifest():
    root=Path(__file__).resolve().parents[1];paths=[]
    for directory,extensions in [("cpp",(".cpp",".hpp")),("cmake",(".cmake",".in")),("src",(".py",))]:
        paths.extend(p for p in (root/directory).rglob("*") if p.is_file() and p.suffix in extensions)
    paths.extend([root/"CMakeLists.txt",root/"pyproject.toml"])
    manifest="".join(f"{p.relative_to(root).as_posix()}={hashlib.sha256(p.read_bytes()).hexdigest()}\n" for p in sorted(paths,key=lambda p:p.relative_to(root).as_posix()))
    assert q.build_environment().source_tree_sha256==hashlib.sha256(manifest.encode()).hexdigest()


def test_json_is_deterministic_preserves_units_and_native_atomic_file_output(tmp_path):
    doc,_,_,values=results();p=values[0].provenance;text=q.provenance_json(p);data=json.loads(text)
    assert data["schema"]=="qpx.analysis-provenance/1" and data["source"]["pixel_contract"]=="qpx.device-rgba8/1"
    assert data["source"]["effective_pixel_width_um"]==.5 and data["source"]["effective_pixel_height_um"]==.25
    assert data["source"]["pixel_width_um"] is None and data["source"]["levels"]==[{"width":64,"height":64,"downsample":1}]
    assert data["environment"]["source_tree_sha256"]==q.build_environment().source_tree_sha256 and q.provenance_json(p)==text
    path=tmp_path/"\u6765\u6e90.json";q.save_provenance(p,str(path));assert path.read_bytes()==text.encode() and not list(tmp_path.glob("*.qpx-tmp-*"))
    occupied=tmp_path/"directory";occupied.mkdir()
    with pytest.raises(RuntimeError):q.save_provenance(p,str(occupied))
    assert occupied.is_dir() and not list(tmp_path.glob("*.qpx-tmp-*"))


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_all_three_archives_preserve_capture_facts_after_save_reopen_and_recheck(tmp_path):
    doc,obj,ids,values=results();expected=[q.provenance_json(v.provenance if i<2 else v.nuclei.provenance) for i,v in enumerate(values)]
    assert values[2].nuclei.provenance is values[2].cells.provenance
    doc.set_calibration(7,8);project=q.Project();image=project.add_document(doc);path=tmp_path/"history.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0]==7 and db.execute("SELECT count(*) FROM analysis_provenance").fetchone()[0]==3
    restored=q.Project.open(str(path)).document(image)
    observed=[restored.analysis_record(ids[0]).result.provenance,restored.tissue_record(ids[1]).result.provenance,restored.cell_record(ids[2]).result.nuclei.provenance]
    assert [q.provenance_json(p) for p in observed]==expected and restored.pixel_width_um==7
    request=q.CellRequest.replay(restored,ids[2]);fresh=q.detect_cells(request)
    assert fresh.nuclei.provenance.effective_pixel_width_um==7 and restored.verify_cells(ids[2],request,fresh)
    assert q.provenance_json(restored.cell_record(ids[2]).result.nuclei.provenance)==expected[2]
    restored.rename_annotation(obj,"checkpoint change");auto=q.ProjectAutosave();checkpoint=tmp_path/"history.qpxrecover"
    recovered_project=q.Project();recovered_image=recovered_project.add_document(restored)
    assert auto.request(recovered_project,str(checkpoint)) and auto.wait().completed==1
    recovered=q.Project.recover(str(checkpoint)).document(recovered_image)
    assert q.provenance_json(recovered.cell_record(ids[2]).result.nuclei.provenance)==expected[2]


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_schema6_legacy_archives_remain_explicitly_unknown_and_migrate_transactionally(tmp_path):
    doc,_,ids,_=results();project=q.Project();image=project.add_document(doc);path=tmp_path/"legacy.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        db.execute("DROP TABLE analysis_provenance");db.execute("PRAGMA user_version=6")
        db.execute("CREATE TRIGGER fail_provenance BEFORE INSERT ON objects BEGIN SELECT RAISE(ABORT,'injected provenance migration'); END")
    loaded=q.Project.open(str(path));legacy=loaded.document(image)
    assert legacy.analysis_record(ids[0]).result.provenance is None and legacy.tissue_record(ids[1]).result.provenance is None and legacy.cell_record(ids[2]).result.nuclei.provenance is None
    before=path.read_bytes()
    with pytest.raises(RuntimeError,match="injected provenance migration"):loaded.save(str(path))
    assert path.read_bytes()==before
    with sqlite3.connect(path) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0]==6
        assert not db.execute("SELECT count(*) FROM sqlite_schema WHERE name='analysis_provenance'").fetchone()[0]
        db.execute("DROP TRIGGER fail_provenance")
    loaded.save(str(path))
    with sqlite3.connect(path) as db:assert db.execute("PRAGMA user_version").fetchone()[0]==7 and db.execute("SELECT count(*) FROM analysis_provenance").fetchone()[0]==0


@pytest.mark.skipif(not q.has_project,reason="project storage")
@pytest.mark.parametrize("mode",["checksum","truncated","trailing","orphan_record","orphan_image","identity","width","level","calibration","source_hash","utf8"])
def test_malformed_provenance_is_rejected_without_rewriting_project(tmp_path,mode):
    doc,_,_,_=results();project=q.Project();project.add_document(doc);path=tmp_path/"bad.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:
        record,blob=db.execute("SELECT record_id,payload FROM analysis_provenance ORDER BY record_id LIMIT 1").fetchone();data=bytearray(blob)
        if mode=="orphan_record":db.execute("UPDATE analysis_provenance SET record_id='999999' WHERE record_id=?",(record,))
        elif mode=="orphan_image":db.execute("UPDATE analysis_provenance SET image_id='999999' WHERE record_id=?",(record,))
        else:
            if mode=="checksum":data[12]^=1
            elif mode=="truncated":data=data[:70]
            elif mode=="trailing":data=data[:-64]+b"extra"+data[-64:]
            elif mode=="identity":struct.pack_into("<Q",data,4,99999)
            elif mode=="width":struct.pack_into("<Q",data,44,65)
            elif mode=="level":
                # Locate the first level after bounded text and calibration fields.
                at=60
                for _ in range(3):n=struct.unpack_from("<Q",data,at)[0];at+=8+n
                for _ in range(4):present=struct.unpack_from("<Q",data,at)[0];at+=8+(8 if present else 0)
                struct.pack_into("<Q",data,at,65537)
            elif mode=="calibration":
                at=60
                for _ in range(3):n=struct.unpack_from("<Q",data,at)[0];at+=8+n
                at+=16  # Original source has both calibrations absent.
                struct.pack_into("<d",data,at+8,float("nan"))
            elif mode=="utf8":data[data.index(b"qpx.dense-rgb/1")]=255
            elif mode=="source_hash":
                source_hash=q.build_environment().source_tree_sha256.encode();at=data.index(source_hash);data[at]=ord("z")
            if mode not in {"checksum","truncated"}:data[-64:]=hashlib.sha256(data[:-64]).hexdigest().encode()
            db.execute("UPDATE analysis_provenance SET payload=? WHERE record_id=?",(data,record))
    before=path.read_bytes()
    with pytest.raises((ValueError,RuntimeError,IndexError)):q.Project.open(str(path))
    assert path.read_bytes()==before



def test_openslide_capture_keeps_native_locator_and_all_pyramid_levels(pyramid_levels,tmp_path):
    server,_=pyramid_levels;doc=q.Document(server);obj=doc.add_rectangle(q.Rect(0,0,32,32))
    options=q.TissueParameters();options.level=2
    request=q.TissueRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]),options);result=q.detect_tissue(request);p=result.provenance
    assert p.reader=="openslide" and p.locator==str((tmp_path/"tissue-levels.tif").resolve())
    assert [(l.width,l.height,l.downsample) for l in p.source.levels]==[(768,576,1),(512,384,1.5),(192,144,4)]
    assert result.level==2 and result.downsample==4 and p.source.width==768
    server.close();assert json.loads(q.provenance_json(p))["source"]["locator"]==p.locator


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_lazy_linked_reader_reports_descriptor_without_changing_historical_provenance(tmp_path):
    rgb=phantom(64);source=tmp_path/"source.ppm";source.write_bytes(b"P6\n64 64\n255\n"+rgb.tobytes())
    project=q.Project();image=project.add_image(str(source),reader="ppm");doc=project.document(image);obj=doc.add_rectangle(q.Rect(0,0,64,64))
    request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]));result=q.analyze(request);record=doc.keep_analysis(request,result)
    assert result.provenance.reader=="ppm" and result.provenance.locator==str(source.resolve())
    path=tmp_path/"linked.qpxproj";project.save(str(path));loaded=q.Project.open(str(path)).document(image)
    assert loaded.analysis_record(record).result.provenance.reader=="ppm"
    fresh=q.AnalysisRequest(loaded,obj,q.StainTransform([[1,0,0],[0,1,0]]))
    assert fresh.provenance.reader=="ppm" and fresh.provenance.locator==str(source.resolve())



@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_provenance_write_failure_rolls_back_file_and_permits_retry(tmp_path):
    doc,obj,ids,_=results();project=q.Project();image=project.add_document(doc);path=tmp_path/"rollback.qpxproj";project.save(str(path))
    with sqlite3.connect(path) as db:db.execute("CREATE TRIGGER fail_metadata BEFORE INSERT ON analysis_provenance BEGIN SELECT RAISE(ABORT,'injected provenance write'); END")
    before=path.read_bytes();doc.set_calibration(9,10)
    request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]));doc.keep_analysis(request,q.analyze(request))
    with pytest.raises(RuntimeError,match="injected provenance write"):project.save(str(path))
    assert path.read_bytes()==before and project.dirty and project.generation==1
    with sqlite3.connect(path) as db:db.execute("DROP TRIGGER fail_metadata")
    project.save(str(path));assert project.generation==2
    loaded=q.Project.open(str(path)).document(image)
    assert loaded.analysis_record(ids[0]).result.provenance.effective_pixel_width_um==.5
    assert loaded.latest_analysis(obj).result.provenance.effective_pixel_width_um==9



@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_json_preserves_quoted_unicode_descriptor_text(tmp_path):
    doc,_,ids,_=results();project=q.Project();image=project.add_document(doc);path=tmp_path/"unicode.qpxproj";project.save(str(path))
    locator='quoted" slash\\ line\n\u96ea'
    with sqlite3.connect(path) as db:
        record,data=db.execute("SELECT record_id,payload FROM analysis_provenance ORDER BY record_id LIMIT 1").fetchone();data=legacy_provenance_blob(data)
        at=60;n=struct.unpack_from("<Q",data,at)[0];at+=8+n
        old=struct.unpack_from("<Q",data,at)[0];encoded=locator.encode();data[at:at+8+old]=struct.pack("<Q",len(encoded))+encoded
        data[-64:]=hashlib.sha256(data[:-64]).hexdigest().encode();db.execute("UPDATE analysis_provenance SET payload=? WHERE record_id=?",(data,record))
    p=q.Project.open(str(path)).document(image).analysis_record(ids[0]).result.provenance
    assert p.locator==locator and json.loads(q.provenance_json(p))["source"]["locator"]==locator
