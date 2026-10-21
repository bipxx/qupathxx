"""Independent CSV parsing, native snapshot isolation and atomic export checks."""
import csv
import gc
import io
from pathlib import Path
import sqlite3
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q

from test_cells import phantom, transform
from test_cell_records import capture, saved
from test_tissue import document
from test_tissue_levels import pyramid_levels


def setup(parameters=None, rgb=None):
    rgb = phantom() if rgb is None else rgb
    doc = document(rgb)
    obj = doc.add_rectangle(q.Rect(0,0,rgb.shape[1],rgb.shape[0]))
    _, result, record = capture(doc,obj,parameters)
    return doc,obj,result,record


def rows(text):
    reader = csv.DictReader(io.StringIO(text,newline=""))
    values = list(reader)
    assert len(reader.fieldnames) == len(set(reader.fieldnames)) and all(None not in row for row in values)
    return values


@pytest.mark.parametrize("feature", [q.TissueFeature.OPTICAL_DENSITY_SUM,q.TissueFeature.STAIN_1,q.TissueFeature.STAIN_2,q.TissueFeature.STAIN_3])
@pytest.mark.parametrize("expansion", [0,3.5])
def test_every_compartment_value_roundtrips_and_channel_space_is_explicit(feature, expansion):
    p = q.CellParameters()
    p.feature,p.threshold,p.sigma,p.cell_expansion = feature,.05,.7,expansion
    doc,obj,result,record = setup(p)
    revision = doc.revision
    request = q.CellTableRequest(doc)
    text = q.cell_measurements_csv(request)
    assert request.record_count == 1 and request.row_count == result.cell_count and request.document_revision == revision
    assert text == q.cell_measurements_csv(doc) == q.cell_measurements_csv(request)
    assert "\r\n" in text and doc.revision == revision
    parsed = rows(text)
    assert len(parsed) == result.cell_count
    for row,f,n,b in zip(parsed,result.features,result.nuclei.components,result.cells.components):
        assert row["table_schema"] == "qpx.cell-table/1" and row["record_id"] == str(record) and row["source_object_id"] == str(obj)
        assert row["status_at_capture"] == "current" and row["algorithm"] == result.algorithm and row["input_sha256"] == result.nuclei.input_sha256
        assert row["channel_space"] == ("optical_density_rgb" if feature == q.TissueFeature.OPTICAL_DENSITY_SUM else "stain_concentrations")
        assert int(row["cell_label"]) == f.label and int(row["nucleus_samples"]) == f.nucleus_pixels and int(row["cell_samples"]) == f.cell_pixels
        assert int(row["cytoplasm_samples"]) == f.cell_pixels-f.nucleus_pixels
        assert float(row["nucleus_centroid_x_px"]) == n.centroid.x and float(row["cell_centroid_y_px"]) == b.centroid.y
        assert float(row["threshold"]) == p.threshold and float(row["expansion_sample_px"]) == expansion
        for compartment in ["nucleus","cell","cytoplasm"]:
            rgb = getattr(f,compartment+"_rgb")
            channels = getattr(f,compartment+"_channels")
            for index,component in enumerate("rgb"):
                assert row[f"{compartment}_mean_{component}"] == "" if rgb is None else float(row[f"{compartment}_mean_{component}"]) == rgb[index]
                assert row[f"{compartment}_mean_channel_{index+1}"] == "" if channels is None else float(row[f"{compartment}_mean_channel_{index+1}"]) == channels[index]
        for index,component in enumerate("rgb"):
            assert float(row["white_"+component]) == result.nuclei.white[index]
            for stain in range(3):
                assert float(row[f"stain_{stain+1}_{component}"]) == result.nuclei.stains[stain][index]


def test_selection_is_sorted_unique_and_snapshots_survive_removal_source_edits_and_gc():
    doc,obj,result,first = setup()
    request = q.CellRequest(doc,obj,transform())
    second = doc.keep_cells(request,q.detect_cells(request))
    expected = q.cell_measurements_csv(doc,[first,second])
    captured = q.CellTableRequest(doc,[second,first,second])
    assert captured.record_count == 2 and q.cell_measurements_csv(captured) == expected
    assert doc.remove_cells(first) and doc.remove_cells(second)
    doc.rename_annotation(obj,"Edited")
    del doc,result,request
    gc.collect()
    assert q.cell_measurements_csv(captured) == expected
    assert {row["status_at_capture"] for row in rows(expected)} == {"current"}


@pytest.mark.parametrize("field,value", [("byte_limit",0),("byte_limit",256*1024*1024+1),("snapshot_budget",0),("snapshot_budget",512*1024*1024+1)])
def test_invalid_resource_options(field,value):
    options = q.CellTableOptions()
    setattr(options,field,value)
    with pytest.raises(ValueError): options.validate()


def test_exact_byte_and_snapshot_limits_preserve_old_files(tmp_path):
    doc,_,_,record = setup()
    captured = q.CellTableRequest(doc)
    expected = q.cell_measurements_csv(captured)
    options = q.CellTableOptions()
    options.byte_limit = len(expected.encode())
    assert q.cell_measurements_csv(doc,options=options) == expected
    path = tmp_path/"\u6d4b\u91cf.csv"
    q.save_cell_measurements_csv(doc,str(path),options=options)
    assert path.read_bytes() == expected.encode()
    options.byte_limit -= 1
    with pytest.raises((ValueError,RuntimeError,IndexError),match="byte limit"):
        q.save_cell_measurements_csv(doc,str(path),options=options)
    assert path.read_bytes() == expected.encode() and list(tmp_path.glob("*.qpx-tmp-*")) == []
    options = q.CellTableOptions()
    options.snapshot_budget = captured.snapshot_bytes
    assert q.CellTableRequest(doc,options=options).snapshot_bytes == captured.snapshot_bytes
    options.snapshot_budget -= 1
    with pytest.raises((ValueError,RuntimeError,IndexError),match="snapshot"):
        q.CellTableRequest(doc,options=options)
    with pytest.raises(IndexError): q.CellTableRequest(doc,[record,999999])


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_unverified_archives_are_exportable_with_status_and_current_requirement_is_strict(tmp_path):
    project,doc,image,obj,_,result,record,path = saved(tmp_path)
    restored = q.Project.open(str(path)).document(image)
    assert {r["status_at_capture"] for r in rows(q.cell_measurements_csv(restored))} == {"unverified"}
    options = q.CellTableOptions()
    options.require_current = True
    with pytest.raises(ValueError,match="current verified"):
        q.CellTableRequest(restored,options=options)
    replay = q.CellRequest.replay(restored,record)
    assert restored.verify_cells(record,replay,q.detect_cells(replay))
    assert q.CellTableRequest(restored,options=options).row_count == result.cell_count
    restored.server.close()
    assert {r["status_at_capture"] for r in rows(q.cell_measurements_csv(restored))} == {"source_closed"}


def test_empty_runs_and_empty_documents_produce_only_the_schema_header():
    doc,obj,result,record = setup(rgb=np.full((16,16,3),255,dtype=np.uint8))
    assert result.cell_count == 0
    captured = q.CellTableRequest(doc)
    assert captured.record_count == 1 and captured.row_count == 0 and rows(q.cell_measurements_csv(captured)) == []
    doc.remove_cells(record)
    assert q.CellTableRequest(doc).record_count == 0 and q.cell_measurements_csv(doc) == q.cell_measurements_csv(captured)


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_csv_quoting_preserves_producer_commas_quotes_and_newlines(tmp_path):
    _,_,image,_,_,_,_,path = saved(tmp_path)
    producer = 'Producer,"quoted"\r\nsecond'
    with sqlite3.connect(path) as db:
        db.execute("UPDATE cell_records SET producer=?",(producer,))
    restored = q.Project.open(str(path)).document(image)
    assert all(r["producer"] == producer for r in rows(q.cell_measurements_csv(restored)))


@pytest.mark.skipif(not q.has_project,reason="project storage")
def test_unsigned_record_and_revision_ids_keep_all_decimal_digits(tmp_path):
    _,_,image,_,_,_,_,path = saved(tmp_path)
    maximum = str(2**64-1)
    with sqlite3.connect(path) as db:
        db.execute("UPDATE images SET revision=?",(maximum,))
        db.execute("UPDATE cell_records SET id=?",(maximum,))
        db.execute("UPDATE analysis_provenance SET record_id=?",(maximum,))
    restored=q.Project.open(str(path)).document(image)
    parsed=rows(q.cell_measurements_csv(restored))
    assert parsed and all(r["record_id"]==r["export_document_revision"]==maximum for r in parsed)


def test_native_job_progress_atomic_failure_and_reuse(tmp_path):
    doc,_,result,_ = setup()
    captured = q.CellTableRequest(doc)
    path = tmp_path/"table.csv"
    job = q.CellTableExportJob()
    assert job.request(captured,str(path))
    status = job.wait()
    assert status.state == "succeeded" and status.completed == 1 and status.rows == status.total_rows == result.cell_count
    assert status.bytes == path.stat().st_size and path.read_bytes() == q.cell_measurements_csv(captured).encode()
    expected = path.read_bytes()
    options = q.CellTableOptions()
    options.byte_limit = 1
    assert job.request(q.CellTableRequest(doc,options=options),str(path))
    failed = job.wait()
    assert failed.state == "failed" and failed.completed == 1 and "byte limit" in failed.error
    assert path.read_bytes() == expected and not list(tmp_path.glob("*.qpx-tmp-*"))
    assert job.request(captured,str(path)) and job.wait().completed == 2


@pytest.mark.skipif(not q.has_openslide,reason="native pyramids")
def test_coarse_grid_centroids_and_sample_units_are_not_rescaled_counts(pyramid_levels):
    server,_ = pyramid_levels
    doc = q.Document(server)
    obj = doc.add_rectangle(q.Rect(12,16,80,80))
    p = q.CellParameters()
    p.level,p.min_nucleus_pixels = 2,1
    _,result,record = capture(doc,obj,p)
    for row,f,n in zip(rows(q.cell_measurements_csv(doc)),result.features,result.nuclei.components):
        assert row["level"] == "2" and float(row["downsample"]) == 4
        assert int(row["nucleus_samples"]) == f.nucleus_pixels and float(row["nucleus_centroid_x_px"]) == n.centroid.x


def test_cancellation_never_exposes_a_partial_table(tmp_path):
    yy,xx = np.indices((256,256))
    mask = (xx % 3 == 1) & (yy % 3 == 1)
    rgb = np.where(mask[...,None],0,255).repeat(3,axis=2).astype(np.uint8)
    p=q.CellParameters()
    p.sigma,p.cell_expansion,p.min_seed_distance,p.min_nucleus_pixels,p.connectivity = 0,0,0,1,4
    doc,_,_,_=setup(p,rgb)
    path=tmp_path/"cancel.csv"
    path.write_bytes(b"sentinel")
    request=q.CellTableRequest(doc)
    job=q.CellTableExportJob()
    assert job.request(request,str(path))
    job.cancel()
    status=job.wait()
    # Cancellation may arrive after the atomic rename, which is a completed save.
    assert status.state in {"cancelled","succeeded"}
    if status.state == "cancelled": assert path.read_bytes() == b"sentinel"
    else: assert len(rows(path.read_text())) == request.row_count
    assert not list(tmp_path.glob("*.qpx-tmp-*"))


def test_worker_finishes_after_document_results_and_request_wrappers_are_released(tmp_path):
    script=r'''
import gc,pathlib,sys
import qupathxx as q
for i in range(8):
    doc=q.Document(q.Image(32,32,[0]*32*32*3))
    obj=doc.add_rectangle(q.Rect(0,0,32,32))
    source=q.CellRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]))
    result=q.detect_cells(source)
    doc.keep_cells(source,result)
    request=q.CellTableRequest(doc)
    job=q.CellTableExportJob()
    path=pathlib.Path(sys.argv[1])/f"worker-{i}.csv"
    assert job.request(request,str(path))
    del doc,source,result,request
    gc.collect()
    assert job.wait().state=='succeeded' and path.exists()
    del job
    gc.collect()
'''
    child=subprocess.run([sys.executable,"-I","-c",script,str(tmp_path)],capture_output=True,text=True,timeout=20)
    assert child.returncode == 0,child.stdout+child.stderr
