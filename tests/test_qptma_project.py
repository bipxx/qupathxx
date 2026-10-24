"""Project-enabled QPTMA persistence and standalone commands preserve their inputs."""
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import pytest
import qupathxx as q

pytestmark=pytest.mark.skipif(not q.has_project,reason='requires native project storage')
FIXTURE=Path(__file__).resolve().parent/'fixtures/qupath-tma-0.7.0/fractional-import.qptma'


def scene(tmp_path, linked=False):
    first=q.Document(q.Image(8,8,[42]*8*8*3));first.add_rectangle(q.Rect(1,1,3,3),'Retained annotation');first.set_calibration(.5,.25)
    second=q.Document(q.Image(256,192,[255]*256*192*3))
    project=q.Project();a=project.add_document(first,'untouched');b=project.add_document(second,'TMA target');project.set_active_image(a)
    view=q.ProjectView();view.center=q.Point(3,4);view.zoom=.75;project.set_view(a,view)
    image=None
    if linked:
        image=tmp_path/'images'/'linked.ppm';image.parent.mkdir();image.write_bytes(b'P6\n1 1\n255\n'+bytes([12,34,56]));project.add_image(str(image),reader='ppm',name='linked')
    path=tmp_path/'source.qpxproj';project.save(str(path))
    return project,a,b,path,image


def cli(*args):
    executable=Path(q.__file__).parent/('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    assert executable.is_file()
    return subprocess.run([str(executable),*map(str,args)],capture_output=True,text=True,encoding='utf-8',timeout=30)


def no_temporaries(path):
    assert not any(p.name.startswith('.qpx-save-new-') or '.qpx-tmp-' in p.name for p in path.iterdir())


def values(data):
    return [(c.name,c.case_id,c.missing,(c.bounds.x,c.bounds.y,c.bounds.width,c.bounds.height)) for c in data.cores]


def test_new_project_preserves_other_images_views_links_and_imported_values(tmp_path):
    project,a,b,source,linked=scene(tmp_path,linked=True);original=source.read_bytes();wire=q.load_qptma(str(FIXTURE))
    q.import_qptma(project.document(b),wire)
    destination=tmp_path/'moved'/'new project \u03b1.qpxproj';destination.parent.mkdir()
    project.save_new(str(destination))
    assert Path(project.path)==destination and not project.dirty and source.read_bytes()==original
    restored=q.Project.open(str(destination))
    assert len(restored.images)==3 and restored.active_image==a and restored.view(a).zoom==.75
    untouched=restored.document(a)
    assert untouched.annotations[0].name=='Retained annotation' and untouched.pixel_width_um==.5 and untouched.pixel_height_um==.25
    assert untouched.server.read_region(q.RegionRequest(0,0,0,1,1)).data==bytes([42,42,42,255])
    assert restored.document(3).server.read_region(q.RegionRequest(0,0,0,1,1)).data==bytes([12,34,56,255])
    assert values(q.qptma_from_grid(restored.document(b).tma_grid()).data)==values(wire)
    no_temporaries(destination.parent)


@pytest.mark.parametrize('budgets',[{'snapshot_bytes':1},{'file_bytes':4096}])
def test_new_project_quota_failure_preserves_live_save_baseline(tmp_path,budgets):
    project,a,b,source,_=scene(tmp_path);generation=project.generation;original=source.read_bytes();output=tmp_path/'failed.qpxproj'
    with pytest.raises((ValueError,RuntimeError)):project.save_new(str(output),**budgets)
    assert Path(project.path)==source and project.generation==generation and not project.dirty
    assert not output.exists() and source.read_bytes()==original;no_temporaries(tmp_path)


def test_new_project_refuses_existing_output_and_missing_link_destination(tmp_path):
    project,a,b,source,linked=scene(tmp_path,linked=True);original=source.read_bytes();existing=tmp_path/'existing.qpxproj';existing.write_bytes(b'existing file')
    with pytest.raises(ValueError):project.save_new(str(existing))
    assert existing.read_bytes()==b'existing file'
    linked.unlink()
    with pytest.raises(ValueError):project.save_new(str(linked))
    assert not linked.exists() and source.read_bytes()==original and Path(project.path)==source and not project.dirty
    no_temporaries(tmp_path)


def test_project_cli_import_export_retains_sources_and_selects_inactive_image(tmp_path):
    project,a,b,source,_=scene(tmp_path);original=source.read_bytes();wire_before=FIXTURE.read_bytes();output=tmp_path/'imported.qpxproj'
    done=cli('--import-qptma',source,FIXTURE,output,'--image',b)
    assert done.returncode==0,done.stderr
    result=json.loads(done.stdout);assert result['image_id']==b and result['cores']==6 and result['state']=='saved'
    assert source.read_bytes()==original and FIXTURE.read_bytes()==wire_before
    reopened=q.Project.open(str(output));assert reopened.active_image==a and len(reopened.images)==2
    assert reopened.document(a).annotations[0].name=='Retained annotation'
    exported=tmp_path/'grid.qptma';done=cli('--export-qptma',output,exported,'--image',b)
    assert done.returncode==0,done.stderr
    report=json.loads(done.stdout);assert report['omitted_metadata']==['native_object_ids','native_grid_name','axis_labels']
    assert values(q.load_qptma(str(exported)))==values(q.load_qptma(str(FIXTURE)))
    old_export=exported.read_bytes();assert cli('--export-qptma',output,exported,'--image',b).returncode==1 and exported.read_bytes()==old_export
    no_temporaries(tmp_path)


@pytest.mark.parametrize('extra',[[],['--image','0'],['--image','2','--image','2'],['--image','2','--unknown','1']])
def test_project_cli_requires_unambiguous_image_and_options(tmp_path,extra):
    project,a,b,source,_=scene(tmp_path);output=tmp_path/'invalid.qptma';before=source.read_bytes()
    done=cli('--export-qptma',source,output,*extra)
    assert done.returncode==2 and not output.exists() and source.read_bytes()==before


@pytest.mark.parametrize('option,value',[('--qptma-bytes','1'),('--snapshot-bytes','1'),('--project-bytes','4096')])
def test_project_cli_import_quota_failure_leaves_no_partial_destination(tmp_path,option,value):
    project,a,b,source,_=scene(tmp_path);output=tmp_path/'failed.qpxproj';before=source.read_bytes()
    done=cli('--import-qptma',source,FIXTURE,output,'--image',b,option,value)
    assert done.returncode==1 and not output.exists() and source.read_bytes()==before
    no_temporaries(tmp_path)


def test_project_cli_invalid_geometry_existing_grid_and_input_aliases_are_atomic(tmp_path):
    project,a,b,source,_=scene(tmp_path);before=source.read_bytes();data=q.load_qptma(str(FIXTURE));cores=data.cores;cores[-1].bounds=q.Rect(10000,0,10,10);data.cores=cores
    wire=tmp_path/'outside.qptma';q.save_qptma(data,str(wire));output=tmp_path/'failed.qpxproj'
    assert cli('--import-qptma',source,wire,output,'--image',b).returncode==1 and not output.exists()
    assert cli('--import-qptma',source,FIXTURE,source,'--image',b).returncode==1 and source.read_bytes()==before
    imported=tmp_path/'imported.qpxproj';assert cli('--import-qptma',source,FIXTURE,imported,'--image',b).returncode==0
    assert cli('--import-qptma',imported,FIXTURE,output,'--image',b).returncode==1 and not output.exists()
    no_temporaries(tmp_path)


def test_nonhot_journal_does_not_block_read_only_project_open(tmp_path):
    project,a,b,source,_=scene(tmp_path);journal=Path(str(source)+'-journal');journal.write_bytes(b'not a hot SQLite journal');before=source.read_bytes()
    opened=q.Project.open_without_journal_recovery(str(source))
    assert len(opened.images)==2 and source.read_bytes()==before and journal.read_bytes()==b'not a hot SQLite journal'


def test_hot_journal_is_not_repaired_by_read_only_open_or_qptma_cli(tmp_path):
    project,a,b,source,_=scene(tmp_path)
    script="""import os,sqlite3,sys
connection=sqlite3.connect(sys.argv[1]);connection.execute('PRAGMA journal_mode=DELETE');connection.execute('PRAGMA synchronous=FULL')
connection.execute('PRAGMA cache_size=1');connection.execute('PRAGMA cache_spill=ON');connection.execute('BEGIN IMMEDIATE')
connection.execute('UPDATE images SET rgb=zeroblob(length(rgb)) WHERE embedded=1');os._exit(0)
"""
    subprocess.run([sys.executable,'-I','-c',script,str(source)],check=True,timeout=20)
    journal=Path(str(source)+'-journal');assert journal.read_bytes()[:8]==bytes.fromhex('d9d505f920a163d7')
    before=source.read_bytes();journal_before=journal.read_bytes()
    with pytest.raises(RuntimeError):q.Project.open_without_journal_recovery(str(source))
    output=tmp_path/'hot-output.qpxproj';done=cli('--import-qptma',source,FIXTURE,output,'--image',b)
    assert done.returncode==1 and not output.exists() and source.read_bytes()==before and journal.read_bytes()==journal_before
    recovered=q.Project.open(str(source));assert recovered.document(b).server.read_region(q.RegionRequest(0,0,0,1,1)).data==bytes([255,255,255,255])
