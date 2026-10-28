"""Run an explicitly staged native spatial CLI, using an installed fixture reader."""
import csv
import io
import json
import math
import os
from pathlib import Path
import subprocess

import pytest
import qupathxx as q


@pytest.fixture(scope='session')
def cli():
    value=os.environ.get('QUPATHXX_SPATIAL_CLI')
    if not value:pytest.skip('requires an explicitly staged spatial CLI')
    path=Path(value).resolve();assert path.is_file()
    return path


def run(cli,args,expected=0):
    env=os.environ.copy()
    if os.name=='nt':env['PATH']=os.pathsep.join([str(cli.parent),str(Path(env['SystemRoot'])/'System32'),env['SystemRoot']])
    child=subprocess.run([str(cli),*map(str,args)],cwd=cli.parent,env=env,capture_output=True,text=True,encoding='utf-8',timeout=30)
    assert child.returncode==expected,child.stdout+child.stderr
    if expected:
        assert not child.stdout
        messages=[json.loads(line) for line in child.stderr.splitlines()];assert len(messages)==1 and messages[0]['schema']=='qpx.batch-error/1'
        return messages[0]
    assert not child.stderr
    messages=[json.loads(line) for line in child.stdout.splitlines()];assert len(messages)==1 and messages[0]['schema']=='qpx.spatial-cli/1'
    return messages[0]


def scene(tmp_path,calibrated=True):
    doc=q.Document(q.Image.demo());a=doc.add_detection(q.Roi.points([q.Point(32,32)]))
    b=doc.add_cell(q.Roi.rectangle(q.Rect(34,34,8,8)),q.Roi.rectangle(q.Rect(34,35,2,2)))
    c=doc.add_detection(q.Roi.points([q.Point(32,32)]))
    if calibrated:doc.set_calibration(.5,.25)
    project=q.Project();image=project.add_document(doc,'Missing source',str(tmp_path/'missing.ppm'),'ppm')
    untouched=project.add_document(q.Document(q.Image.demo()),'Embedded reference');project.set_active_image(untouched)
    source=tmp_path/'source α.qpxproj';project.save(str(source))
    return project,doc,image,(a,b,c),source


@pytest.mark.parametrize('units',['px','um'])
@pytest.mark.parametrize('body',[False,True])
@pytest.mark.parametrize('empty',[False,True])
def test_native_measure_export_verify_and_reopen(cli,tmp_path,units,body,empty):
    project,doc,image,(a,b,c),source=scene(tmp_path);original=source.read_bytes();output=tmp_path/'measured α.qpxproj'
    args=['--measure-spatial',source,output,'--image',image,'--source',b,'--source',a,'--units',units,'--name','Nearest α']
    if not empty:args+=['--target',c]
    if body:args+=['--body']
    report=run(cli,args);assert report['command']=='measure' and report['state']=='saved' and report['rows']==report['sources']==2
    assert report['targets']==(0 if empty else 1) and report['units']==units and report['historical'] is False
    assert source.read_bytes()==original and not (tmp_path/'missing.ppm').exists()
    restored=q.Project.open(str(output));other=restored.document(image);record=other.spatial_record(report['record_id'])
    assert restored.identity==project.identity and restored.active_image==project.active_image and len(restored.images)==2
    assert record.name=='Nearest α' and other.spatial_status(record.id)=='unverified'
    assert record.result.sha256==report['result_sha256'] and record.result.input_sha256==report['input_sha256']
    assert record.result.provenance.environment.git_revision==os.environ['QUPATHXX_SPATIAL_CLI_SOURCE']
    row=record.result.row(1)
    if empty:assert row.distance is None
    else:
        dx,dy=(6,6) if body else (3,4);sx,sy=(1,1) if units=='px' else (.5,.25)
        assert row.target_id==c and row.distance==pytest.approx(math.hypot(dx*sx,dy*sy),abs=1e-9,rel=1e-12)
    csv_path=tmp_path/'table α.csv';export=run(cli,['--export-spatial',output,csv_path,'--image',image,'--record',record.id])
    assert export['historical'] is True and export['result_sha256']==report['result_sha256']
    assert csv_path.read_bytes()==q.spatial_distance_csv(record.result).encode()
    rows=list(csv.DictReader(io.StringIO(csv_path.read_text())));assert rows[0]['status']==('no_target' if empty else 'matched')
    saved=output.read_bytes();verified=run(cli,['--verify-spatial',output,'--image',image,'--record',record.id])
    assert verified['state']=='verified' and verified['historical'] is False and output.read_bytes()==saved


@pytest.mark.parametrize('extra',[
    ['--image','1'],['--source','0'],['--source','-1'],['--source','18446744073709551616'],
    ['--units','nm'],['--body','--body'],['--record','1'],['--work-bytes','1'],['--name',''],['--unknown','1']])
def test_parser_refuses_invalid_options_before_opening_any_project(cli,tmp_path,extra):
    output=tmp_path/'new.qpxproj';run(cli,['--measure-spatial',tmp_path/'does-not-exist.qpxproj',output,'--image','1','--source','1',*extra],2)
    assert not output.exists()


def test_source_aliases_limits_and_stale_history_are_non_destructive(cli,tmp_path):
    _,doc,image,(a,b,c),source=scene(tmp_path);original=source.read_bytes();output=tmp_path/'new.qpxproj'
    base=['--measure-spatial',source,output,'--image',image,'--source',a,'--source',b,'--target',c]
    for path in [source,Path(str(source)+'-journal'),Path(str(source)+'-wal'),Path(str(source)+'-shm'),tmp_path/'missing.ppm']:
        bad=base.copy();bad[2]=path;run(cli,bad,1);assert source.read_bytes()==original
    assert not (tmp_path/'missing.ppm').exists()
    run(cli,[*base,'--save-snapshot-bytes','1'],1);assert not output.exists()
    run(cli,[*base,'--project-bytes','1'],1);assert not output.exists()
    run(cli,[*base,'--max-distance-tests','1'],1);assert not output.exists()
    report=run(cli,base);saved=output.read_bytes();run(cli,base,1);assert output.read_bytes()==saved
    csv_path=tmp_path/'limited.csv';run(cli,['--export-spatial',output,csv_path,'--image',image,'--record',report['record_id'],'--csv-bytes','1'],1);assert not csv_path.exists()
    restored=q.Project.open(str(output));other=restored.document(image);other.remove_annotation(c);changed=tmp_path/'changed.qpxproj';restored.save(str(changed))
    run(cli,['--verify-spatial',changed,'--image',image,'--record',report['record_id']],1)
    run(cli,['--export-spatial',changed,csv_path,'--image',image,'--record',report['record_id']])
    assert source.read_bytes()==original
    assert not list(tmp_path.glob('*.qpx-tmp-*')) and not list(tmp_path.glob('.qpx-save-new-*'))


def test_unknown_calibration_and_image_record_selection(cli,tmp_path):
    _,_,image,(a,b,c),source=scene(tmp_path,False);output=tmp_path/'unknown.qpxproj'
    run(cli,['--measure-spatial',source,output,'--image',image,'--source',a,'--target',b,'--units','um'],1)
    assert not output.exists()
    run(cli,['--measure-spatial',source,output,'--image','99999','--source',a],1)
    run(cli,['--export-spatial',source,tmp_path/'none.csv','--image',image,'--record','99999'],1)
    assert not (tmp_path/'none.csv').exists()


def test_id_files_bypass_command_line_limits_and_preserve_cohort_identity(cli,tmp_path):
    doc=q.Document(q.Image.demo());point=q.Roi.points([q.Point(32,32)])
    doc.execute([q.ObjectEdit.create_geometry(point) for _ in range(5000)])
    ids=[a.id for a in doc.annotations];project=q.Project();image=project.add_document(doc,'Large explicit cohort',str(tmp_path/'missing.ppm'),'ppm')
    source=tmp_path/'large.qpxproj';project.save(str(source));before=source.read_bytes()
    sources=tmp_path/'sources.txt';sources.write_text(str(ids[-1])+'\r\n');targets=tmp_path/'targets.txt';targets.write_text('\n'.join(map(str,reversed(ids))))
    original_sources=sources.read_bytes();original_targets=targets.read_bytes();output=tmp_path/'large-result.qpxproj'
    report=run(cli,['--measure-spatial',source,output,'--image',image,'--source-ids',sources,'--target-ids',targets])
    assert report['sources']==1 and report['targets']==5000
    result=q.Project.open(str(output)).document(image).spatial_record(report['record_id']).result
    assert result.row(0).target_id==ids[0] and result.row(0).distance==0 and result.target_object(4999).id==ids[-1]
    assert source.read_bytes()==before and sources.read_bytes()==original_sources and targets.read_bytes()==original_targets
    too_small=tmp_path/'too-small.qpxproj';run(cli,['--measure-spatial',source,too_small,'--image',image,'--source-ids',sources,'--target-ids',targets,'--ids-bytes','1'],1)
    assert not too_small.exists()
    run(cli,['--measure-spatial',source,sources,'--image',image,'--source-ids',sources,'--target-ids',targets],1)
    assert sources.read_bytes()==original_sources
    run(cli,['--measure-spatial',source,tmp_path/'mixed.qpxproj','--image',image,'--source',ids[0],'--source-ids',sources],2)


@pytest.mark.parametrize('text',['','0','-1','+1','1,2','1\x00','18446744073709551616','1\n1'])
def test_id_file_refusals_are_bounded_and_non_destructive(cli,tmp_path,text):
    _,_,image,_,source=scene(tmp_path);before=source.read_bytes();ids=tmp_path/'ids.txt';ids.write_bytes(text.encode());output=tmp_path/'refused.qpxproj'
    run(cli,['--measure-spatial',source,output,'--image',image,'--source-ids',ids],1)
    assert not output.exists() and source.read_bytes()==before and ids.read_bytes()==text.encode()


def test_id_file_count_limit_and_empty_target_file(cli,tmp_path):
    _,_,image,(a,_,_),source=scene(tmp_path);ids=tmp_path/'many.txt';ids.write_text('1\n'*100001);output=tmp_path/'refused.qpxproj'
    error=run(cli,['--measure-spatial',source,output,'--image',image,'--source-ids',ids],1)
    assert '100000' in error['error'] and not output.exists()
    ids.write_text('');report=run(cli,['--measure-spatial',source,output,'--image',image,'--source',a,'--target-ids',ids])
    assert report['targets']==0 and q.Project.open(str(output)).document(image).spatial_record(report['record_id']).result.row(0).distance is None
