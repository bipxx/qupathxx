"""Raw project batches retain captured axes and resume by original sample bits."""
import csv
import io
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
import tifffile
from test_tiff_project import scene

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_project,reason='TIFF project support')


def transform():return q.StainTransform([[1,0,0],[0,1,0]])


def test_batch_captures_axes_and_context_and_reuses_without_statistics(tmp_path):
    project,image,values=scene(tmp_path);doc=project.document(image)
    obj=doc.add_rectangle(q.Rect(1,2,5,6),'Original')
    view=doc.server.tiff_view;view.z=2;view.t=1;doc.set_tiff_view(view)
    options=q.ProjectBatchOptions();options.kind=q.ProjectBatchKind.RAW;options.roi_tile_size=16
    options.raw.channels=2
    request=q.ProjectBatchRequest(project,transform(),options=options)
    doc.rename_annotation(obj,'After capture');view.z=0;view.t=0;doc.set_tiff_view(view)
    report=q.run_project_batch(request,str(tmp_path/'batch'))
    assert report.state=='succeeded',report.error
    first=report.images[0]
    assert first.raw_tiles==1 and first.raw_hashed_tiles==0 and first.records==1
    assert doc.raw_analysis_record_count==0
    checkpoint=q.Project.open(first.path).document(image)
    retained=checkpoint.raw_analysis_records[0]
    assert retained.result.region.z==2 and retained.result.region.t==1
    assert retained.result.object_context.name=='Original'
    np.testing.assert_allclose(retained.result.statistics.means,values[1,2,:,2:8,1:6].mean(axis=(1,2)))
    table=list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(first.path))))
    assert len(table)==2 and all(r['batch_image_id']==str(image) for r in table)
    assert table[0]['object_name']=='Original'
    again=q.run_project_batch(request,str(tmp_path/'batch'))
    assert again.state=='succeeded' and again.reused==1
    assert again.images[0].raw_tiles==0 and again.images[0].raw_hashed_tiles==1
    assert again.images[0].path==first.path
    stats=json.loads(q.project_batch_image_json(again.images[0]))
    assert stats['raw_tiles']==0 and stats['raw_hashed_tiles']==1


def test_resume_rejects_one_bit_sample_change_and_corrupt_checkpoint(tmp_path):
    source=tmp_path/'wide.tif';values=np.full((4,4),2**63,dtype='uint64')
    tifffile.imwrite(source,values,metadata=None)
    project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
    doc.add_rectangle(q.Rect(0,0,4,4))
    options=q.ProjectBatchOptions();options.kind=q.ProjectBatchKind.RAW
    request=q.ProjectBatchRequest(project,transform(),options=options);directory=str(tmp_path/'batch')
    first=q.run_project_batch(request,directory);assert first.state=='succeeded'
    values[0,0]+=np.uint64(1);doc.server.close();tifffile.imwrite(source,values,metadata=None)
    changed=q.run_project_batch(request,directory)
    assert changed.state=='succeeded' and changed.saved==1 and changed.reused==0
    assert changed.images[0].pixel_sha256!=first.images[0].pixel_sha256
    assert changed.images[0].raw_hashed_tiles==1 and changed.images[0].raw_tiles==1
    import sqlite3
    with sqlite3.connect(changed.images[0].path) as db:db.execute('DELETE FROM raw_analysis_records')
    corrupted=q.run_project_batch(request,directory)
    assert corrupted.state=='failed' and corrupted.reused==0


def test_explicit_axes_override_view_and_mixed_reader_failure_is_per_image(tmp_path):
    project,image,values=scene(tmp_path);doc=project.document(image)
    doc.add_rectangle(q.Rect(0,0,4,4))
    other=q.Document(q.Image(4,4,[0]*4*4*3));other.add_rectangle(q.Rect(0,0,4,4));project.add_document(other,'RGB')
    options=q.ProjectBatchOptions();options.kind=q.ProjectBatchKind.RAW;options.failure=q.ProjectBatchFailure.CONTINUE
    options.raw_follow_display=False;options.raw.z=1;options.raw.t=1
    report=q.run_project_batch(q.ProjectBatchRequest(project,transform(),options=options),str(tmp_path/'mixed'))
    assert report.state=='partial' and report.saved==report.failed==1
    retained=q.Project.open(report.images[0].path).document(image).raw_analysis_records[0]
    assert retained.result.region.z==1 and retained.result.region.t==1
    np.testing.assert_allclose(retained.result.statistics.means,[values[1,1,0,:4,:4].mean()])


def test_native_cli_raw_axes_unicode_checkpoint_and_resume(tmp_path):
    from qupathxx import _native
    cli=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    project,image,values=scene(tmp_path);doc=project.document(image);doc.add_rectangle(q.Rect(0,0,4,4))
    path=tmp_path/'量化.qpxproj';project.save(str(path));directory=tmp_path/'结果'
    command=[str(cli),str(path),str(directory),'--kind','raw','--raw-channel','1','--raw-z','2','--raw-time','1','--tile-size','16']
    done=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=40)
    assert done.returncode==0,done.stderr
    lines=[json.loads(line) for line in done.stdout.splitlines() if line.strip()]
    result=next(line for line in lines if line['schema']=='qpx.batch-image/1')
    assert result['raw_tiles']==1
    record=q.Project.open(result['path']).document(image).raw_analysis_records[0]
    np.testing.assert_allclose(record.result.statistics.means,[values[1,2,1,:4,:4].mean()])
    again=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=40)
    assert again.returncode==0,again.stderr
    repeated=next(json.loads(line) for line in again.stdout.splitlines() if json.loads(line)['schema']=='qpx.batch-image/1')
    assert repeated['state']=='reused' and repeated['raw_tiles']==0 and repeated['raw_hashed_tiles']==1
