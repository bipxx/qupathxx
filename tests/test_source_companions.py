"""Installed descriptor boundary; compound reader opening remains a separate gate."""
import gc
import json

import pytest
import qupathxx as q
from test_source_identity import open_ppm, ppm


def test_legacy_single_file_json_and_primary_association_stay_version_one(tmp_path):
    server=open_ppm(ppm(tmp_path/'primary.ppm'))
    source=server.reader_source
    assert source.companions==[]
    wire=json.loads(q.reader_source_json(source))
    assert wire['schema']=='qpx.reader-source/1' and 'companions' not in wire
    association=q.associate_source_file(source,q.fingerprint_file(source.locator))
    assert association.status=='matching_metadata'
    assert json.loads(q.source_file_association_json(association))['schema']=='qpx.source-file-association/1'
    server.close()


def test_companion_list_is_a_copy_and_descriptor_is_read_only(tmp_path):
    server=open_ppm(ppm(tmp_path/'source.ppm'))
    source=server.reader_source
    copy=source.companions
    copy.append(source.primary)
    assert len(copy)==1 and source.companions==[]
    with pytest.raises(AttributeError):
        source.companions=[]
    with pytest.raises(AttributeError):
        source.primary.path='other.ppm'
    expected=source.primary.path
    server.close();del server;gc.collect()
    assert source.primary.path==expected and source.companions==[]


def test_memory_descriptor_has_no_primary_or_companion_files():
    document=q.Document(q.Image.demo())
    source=document.server.reader_source
    assert source.scope=='memory' and source.primary is None and source.companions==[]
    assert json.loads(q.reader_source_json(source))['schema']=='qpx.reader-source/1'
