import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import pytest

CI = Path(__file__).parents[1]/'tools/ci'
sys.path.insert(0, str(CI))
spec = importlib.util.spec_from_file_location('candidate_sources', CI/'collect_candidate_sources.py')
collector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)


def setup(tmp_path):
    cache = tmp_path/'cache'
    cache.mkdir()
    data = b'authored source container stand-in; no external data'
    (cache/'source.tar.gz').write_bytes(data)
    record = {'component':'authored','version':'1','filename':'source.tar.gz','bytes':len(data),
              'sha256':hashlib.sha256(data).hexdigest(),'source':'independent authored test declaration',
              'review':'authored fixture with no third-party material','material':{
                  'id':'source','category':'admitted_open','kind':'source_archive','license_id':'AGPL-3.0-only',
                  'license_evidence':'LICENSE','attribution':'qupathxx contributors','source_ids':[]}}
    root = copy.deepcopy(record)
    root['material'].update(id='root', kind='notice')
    value = {'records':[record,root], 'release_profiles':{'test':{'sources':['source'],'firstparty_license':'root'}}}
    inventory = tmp_path/'inventory.json'
    inventory.write_text(json.dumps(value),encoding='utf8')
    return cache,inventory,value


def test_source_collection_is_pinned_offline_and_member_checked(tmp_path):
    cache,inventory,_ = setup(tmp_path)
    result = collector.collect(tmp_path/'output',[cache],[inventory],'test','a'*64)
    assert result['state']=='passed' and result['archive_members']==3 and result['sources'][0]['version']=='1'
    entries = collector.admission.inventory([inventory,tmp_path/'output/generated-materials.json'])
    assert collector.admission.check_archive(tmp_path/'output'/result['archive'],entries)==3
    (cache/'source.tar.gz').write_bytes(b'changed')
    with pytest.raises(ValueError,match='identity differs'):
        collector.collect(tmp_path/'changed',[cache],[inventory],'test','a'*64)


@pytest.mark.parametrize('category',['proprietary','noncommercial','unreviewed'])
def test_source_collection_refuses_restricted_before_download(tmp_path,category):
    cache,inventory,value = setup(tmp_path)
    value['records'][0]['material']['category']=category
    inventory.write_text(json.dumps(value),encoding='utf8')
    with pytest.raises(ValueError,match='Restricted/unreviewed'):
        collector.collect(tmp_path/'blocked',[cache],[inventory],'test','a'*64,allow_download=True)
    assert not (tmp_path/'blocked').exists()


def test_missing_review_or_cache_cannot_become_automatic_admission(tmp_path):
    cache,inventory,value = setup(tmp_path)
    value['records'][0].pop('review')
    inventory.write_text(json.dumps(value),encoding='utf8')
    with pytest.raises(ValueError,match='explicit contents/license review'):
        collector.collect(tmp_path/'unreviewed',[cache],[inventory],'test','a'*64)
    value['records'][0]['review']='authored explicit review'
    inventory.write_text(json.dumps(value),encoding='utf8')
    with pytest.raises(ValueError,match='download disabled'):
        collector.collect(tmp_path/'missing',[tmp_path/'empty'],[inventory],'test','a'*64)


def test_code_only_source_never_falls_back_to_upstream_carrier(tmp_path, monkeypatch):
    cache,inventory,value = setup(tmp_path)
    value['records'][0]['member_selection']='reviewed-selection.json'
    inventory.write_text(json.dumps(value),encoding='utf8')
    def forbidden(*args,**kwargs):
        pytest.fail('Curated source must not request the upstream carrier')
    monkeypatch.setattr(collector.urllib.request,'urlopen',forbidden)
    with pytest.raises(ValueError,match='carrier fallback forbidden'):
        collector.collect(tmp_path/'blocked',[tmp_path/'absent'],[inventory],'test','a'*64,allow_download=True)


def test_source_companion_must_match_reviewed_native_source_id(tmp_path):
    cache,inventory,value=setup(tmp_path)
    value['release_profiles']['test']['compiled_source_sha256']='b'*64
    inventory.write_text(json.dumps(value),encoding='utf8')
    with pytest.raises(ValueError,match='source identity differs'):
        collector.collect(tmp_path/'blocked',[cache],[inventory],'test','a'*64)
    assert not (tmp_path/'blocked').exists()
