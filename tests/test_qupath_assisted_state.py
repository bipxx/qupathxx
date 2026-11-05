"""Versioned assisted routes reuse pinned native state without duplicate captures."""
import hashlib
import json
import os
import shutil
from pathlib import Path
from urllib.parse import unquote, urlparse

import pytest
import qupathxx as q

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-assisted-state-0.7.0'
MANIFEST=json.loads((FIXTURE/'manifest.json').read_bytes())
pytestmark=pytest.mark.skipif(not q.has_project or not q.has_interchange or not q.has_tiff,reason='project/interchange/TIFF')


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path,value):path.write_text(json.dumps(value,ensure_ascii=False),encoding='utf-8')


def foreign(tmp_path,label):
    root=tmp_path/label;shutil.copytree(FIXTURE/label,root)
    parent=ROOT/'tests/fixtures'/MANIFEST['parents'][label]['fixture']
    captured=next(c for c in MANIFEST['captures'] if c['label']==label)
    for relative,sha in captured['input_files'].items():
        target=root/relative
        if relative!='project/project.qpproj':
            target.parent.mkdir(parents=True,exist_ok=True)
            data=(parent/relative).read_bytes()
            if label=='mixed' and relative=='project/data/2/data.qpdata':
                assert data.count(b'0.7.0')==1
                data=data.replace(b'0.7.0',b'0.6.0',1)
            target.write_bytes(data)
        assert digest(target)==sha
    path=root/'project/project.qpproj';project=json.loads(path.read_bytes())
    for image in project['images']:
        uri=unquote(urlparse(image['serverBuilder']['uri']).path)
        source=root/('sources/'+Path(uri).name if label in ('mixed','geojson') else 'source.ome.tif')
        assert source.is_file();image['serverBuilder']['uri']=source.as_uri()
    write(path,project)
    index=path.parent/'qupathxx-exchange/index.json';value=json.loads(index.read_bytes())
    value['project_sha256']=digest(path);write(index,value)
    return path,index


def test_retained_capture_producer_parents_and_outputs():
    for name,sha in MANIFEST['files'].items():assert digest(FIXTURE/name)==sha
    assert digest(ROOT/MANIFEST['producer']['path'])==MANIFEST['producer']['sha256']
    for parent in MANIFEST['parents'].values():
        assert digest(ROOT/'tests/fixtures'/parent['fixture']/'manifest.json')==parent['manifest_sha256']
    assert len(MANIFEST['captures'])==6 and all(c['returncode']==0 for c in MANIFEST['captures'])


@pytest.mark.parametrize('label',['facts','settings','properties','tma','geojson','mixed'])
def test_version2_import_and_reopen(tmp_path,label):
    path,index=foreign(tmp_path,label);bundle=json.loads(index.read_bytes())
    preview=q.preview_qupath_project(str(path));target=tmp_path/'import.qpxproj'
    project=preview.import_new(str(target),allow_losses=True);restored=q.Project.open(str(target))
    assert len(project.images)==len(bundle['images'])
    for info,entry in zip(preview.images,bundle['images'],strict=True):
        doc=restored.document(info.native_id)
        assert not doc.can_undo and doc.object_count==info.objects
        assert len(set(info.source_object_ids))==len(info.imported_object_ids)==doc.object_count
        if entry['representation']=='native-saved-data':
            assert set(entry)=={'entry_id','representation','qpdata_sha256'}
            assert not (index.parent/(entry['entry_id']+'.geojson')).exists()
        if label=='tma':
            from test_qupath_saved_tma import assert_document
            assert_document(doc,dict(zip(info.source_object_ids,info.imported_object_ids,strict=True)),info.name)
        elif label=='settings':
            from test_image_analysis_settings import REFERENCE
            row=next(r for r in REFERENCE if r['name']==info.name);value=doc.image_analysis_settings
            if row['image_type']=='UNSET':assert value is None
            else:
                assert value.image_type==row['image_type']
                if row['stains']:
                    import numpy as np
                    assert value.profile_name==row['stains']['name'] and value.stain_names==row['stains']['names']
                    assert list(value.white)==row['stains']['white']
                    np.testing.assert_allclose(value.basis,row['stains']['basis'],rtol=0,atol=1e-14)
        elif label=='properties' and info.name=='portable-values':
            from test_image_data_properties import REFERENCE,expected,signature
            assert signature(doc.image_data_properties.values)==signature({k:expected(v) for k,v in REFERENCE[info.name].items()})
        elif label=='facts':
            ref=json.loads((ROOT/'tests/fixtures/qupath-root-facts-0.7.0/reference.json').read_bytes())
            assert doc.image_properties.metadata==ref['metadata'] and doc.image_properties.measurements['Root area']==12.5


@pytest.mark.parametrize('damage',['mode','missing-mode','alternate-file','alternate-calibration','alternate-loss','hash-type','hash-shape','stale','unknown-field','missing-data','truncated-rehashed','producer-rehashed'])
def test_native_entry_refuses_ambiguous_stale_or_bad_stream_without_fallback(tmp_path,damage):
    path,index=foreign(tmp_path,'facts');bundle=json.loads(index.read_bytes());entry=bundle['images'][0]
    data=path.parent/'data'/entry['entry_id']/'data.qpdata'
    if damage=='mode':entry['representation']='other'
    elif damage=='missing-mode':del entry['representation']
    elif damage=='alternate-file':entry.update(file='rogue.geojson',sha256='0'*64)
    elif damage=='alternate-calibration':entry['pixel_width_um']=1
    elif damage=='alternate-loss':entry['issues']=[]
    elif damage=='hash-type':entry['qpdata_sha256']=1
    elif damage=='hash-shape':entry['qpdata_sha256']='x'*64
    elif damage=='stale':data.write_bytes(data.read_bytes()+b'!')
    elif damage=='unknown-field':bundle['other']=True
    elif damage=='missing-data':data.unlink()
    else:
        raw=data.read_bytes()
        if damage=='truncated-rehashed':raw=raw[:-1]
        else:
            assert raw.count(b'0.7.0')==1;raw=raw.replace(b'0.7.0',b'0.6.0',1)
        data.write_bytes(raw);entry['qpdata_sha256']=digest(data)
    write(index,bundle)
    # An unrelated alternate geometry cannot rescue an explicitly native record.
    write(index.parent/'rogue.geojson',{'type':'FeatureCollection','features':[]})
    with pytest.raises(ValueError):q.preview_qupath_project(str(path))
    assert not list(tmp_path.glob('*.qpxproj'))


@pytest.mark.parametrize('member',['project.qpproj','qupathxx-exchange/index.json','data/1/data.qpdata','classifiers/classes.json','../source.ome.tif'])
def test_reviewed_native_bundle_changes_preserve_destination_and_preview(tmp_path,member):
    path,index=foreign(tmp_path,'facts');preview=q.preview_qupath_project(str(path))
    changed=path.parent/member;existed=changed.exists();changed.parent.mkdir(parents=True,exist_ok=True)
    changed.write_bytes(changed.read_bytes()+b' ' if existed else b'{}')
    target=tmp_path/'changed.qpxproj'
    with pytest.raises(RuntimeError,match='changed' if existed else 'appeared since review'):preview.import_new(str(target))
    assert not preview.consumed and not target.exists()


def test_native_unreferenced_geometry_is_not_an_import_authority(tmp_path):
    path,index=foreign(tmp_path,'facts')
    write(index.parent/'1.geojson',{'type':'FeatureCollection','features':[]})
    preview=q.preview_qupath_project(str(path));assert preview.images[0].objects==1
    # An unused alternate geometry is outside the reviewed dependency set.
    (index.parent/'1.geojson').write_bytes(b'invalid alternate data')
    project=preview.import_new(str(tmp_path/'facts.qpxproj'))
    assert project.document(preview.images[0].native_id).image_properties.measurements['Root area']==12.5


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='opt-in assisted-state desktop')
def test_desktop_opens_native_bundle_and_saves_retained_grid(tmp_path):
    from test_gui_replay import event
    path,index=foreign(tmp_path,'tma')
    project=q.Project();project.add_document(q.Document(q.Image.demo()),'Original')
    options=q.GuiOptions();options.project_open_path=str(path);options.settle_tiles=True
    options.autosave_interval_seconds=0
    options.inputs=[event(5,q.GuiInputKind.OPEN_PROJECT_KEY),event(60,q.GuiInputKind.OPEN_PROJECT_KEY),
                    event(80,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT,q.GuiInputWait.TARGET_READY),
                    event(81,q.GuiInputKind.LEFT_UP,q.GuiInputTarget.DISCARD_RECOVERY_REPLACEMENT)]
    report=q.show(project,frame_limit=120,capture_path=str(tmp_path/'assisted-tma.ppm'),options=options)
    assert report.replay_complete and report.project_replacements==1
    reopened=q.Project.open(str(path.with_suffix('.qpxproj')))
    assert len(reopened.images)==2
    for image in reopened.images:
        doc=reopened.document(image.id)
        # Source UUIDs remain in imported attributes; the headless test verifies
        # the full independent UUID mapping. Desktop verifies the retained grid.
        assert doc.tma_core_count==6 and doc.object_count==11
        assert sum(doc.annotation(i).tma_core.missing for i in doc.tma_core_ids(True))==2
