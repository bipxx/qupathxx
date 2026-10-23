"""Snapshot CSV retains original labels and works without a source decoder."""
import csv
import io

import pytest
import qupathxx as q
from test_raw_records import scene

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_project,reason='TIFF project support')


def rows(value):return list(csv.DictReader(io.StringIO(value)))


def test_frozen_csv_and_historical_export_after_source_deletion(tmp_path):
    project,doc,_,obj,record,result,source=scene(tmp_path)
    snapshot=q.RawTableRequest(doc,[record,record],True)
    doc.rename_annotation(obj,'Later, "name"\nnew line')
    assert doc.raw_analysis_status(record)=='object_changed'
    doc.server.close();source.unlink()
    current=q.RawTableRequest(doc)
    frozen=rows(q.raw_analysis_csv(snapshot));historical=rows(q.raw_analysis_csv(current))
    assert len(frozen)==len(historical)==1 and snapshot.records==snapshot.rows==1
    assert frozen[0]['object_name']==historical[0]['object_name']=='Original'
    assert frozen[0]['status_at_capture']=='current' and historical[0]['status_at_capture']=='object_changed'
    assert frozen[0]['input_sha256']==result.input_sha256
    assert float(frozen[0]['mean'])==result.statistics.means[0]
    with pytest.raises(ValueError):q.RawTableRequest(doc,require_current=True)
    destination=tmp_path/'historical.csv';text=q.raw_analysis_csv(current)
    assert q.save_raw_analysis_csv(current,str(destination))==len(text.encode())
    assert destination.read_bytes()==text.encode()


def test_csv_budget_new_only_protection_and_native_job(tmp_path):
    _,doc,_,_,record,_,source=scene(tmp_path);table=q.RawTableRequest(doc)
    output=tmp_path/'records.csv';output.write_bytes(b'keep original')
    with pytest.raises((ValueError,RuntimeError)):q.save_raw_analysis_csv(table,str(output))
    assert output.read_bytes()==b'keep original'
    with pytest.raises((ValueError,RuntimeError)):q.save_raw_analysis_csv(table,str(source),True)
    with pytest.raises((ValueError,RuntimeError)):q.raw_analysis_csv(table,32)
    bounded=tmp_path/'bounded.csv'
    with pytest.raises((ValueError,RuntimeError)):q.save_raw_analysis_csv(table,str(bounded),max_bytes=64)
    assert not bounded.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))
    with pytest.raises((ValueError,RuntimeError)):q.RawTableRequest(doc,snapshot_bytes=1)
    with pytest.raises(IndexError):q.RawTableRequest(doc,[record+1000])
    job=q.RawTableJob();assert job.request(table,str(output),True)
    done=job.wait();assert done.state=='saved' and done.completed_rows==done.total_rows==1
    assert done.bytes==output.stat().st_size and not job.busy


def test_nearest_tma_context_is_immutable_and_rfc_quoted(tmp_path):
    _,doc,_,_,_,_,_=scene(tmp_path)
    grid=q.TmaGridOptions();grid.rows=grid.columns=1;grid.origin=q.Point(12,8);grid.width=grid.height=12
    core=doc.create_tma_grid(grid)[0];case='case,"组织"\n1';doc.set_tma_case_ids({core:case})
    child=doc.add_rectangle(q.Rect(9,6,2,2),'Child');doc.reparent_annotation(child,core)
    from test_tiff_reader import request
    capture=q.RawAnalysisRequest(doc,request(1,1),child);result=q.analyze_raw(capture)
    record=doc.keep_raw_analysis(capture,result);snapshot=q.RawTableRequest(doc,[record],True)
    doc.set_tma_case_ids({core:'Changed'})
    assert not capture.matches(doc) and doc.raw_analysis_status(record)=='context_changed'
    assert rows(q.raw_analysis_csv(snapshot))[0]['tma_case_id']==case
    with pytest.raises(ValueError):q.RawAnalysisRequest.replay(doc,record)
