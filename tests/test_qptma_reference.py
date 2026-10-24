"""Pinned, actually executed QuPath TMA public-API reference contract."""
import csv
import hashlib
import json
import math
from pathlib import Path
import pytest

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-tma-0.7.0'
REFERENCE=json.loads((FIXTURE/'reference.json').read_text(encoding='utf-8'))


def wire(name):
    lines=(FIXTURE/name).read_text(encoding='utf-8').splitlines()
    assert lines[2]=='' and lines[3:5]==['TMA grid width: 3','TMA grid height: 2']
    assert lines[5].split('\t')==['Core name','X','Y','Width','Height','Present','Case ID']
    rows=list(csv.reader(lines[6:],delimiter='\t',quoting=csv.QUOTE_NONE))
    assert len(rows)==6 and all(len(row)==7 for row in rows)
    return [dict(name=row[0],bounds=[float(v) for v in row[1:5]],missing=row[5]=='false',case_id=row[6]) for row in rows]


def test_reference_files_and_generator_are_byte_bound():
    manifest=json.loads((FIXTURE/'manifest.json').read_text(encoding='utf-8'))
    assert manifest['schema']=='qpx.qupath-tma-fixtures/1'
    assert manifest['upstream_revision']==REFERENCE['upstream_revision']=='04ccfa4fb7d43e9b566393e08e83690b72248d44'
    assert manifest['qupath_version']==REFERENCE['qupath_version']=='0.7.0'
    for name,item in manifest['files'].items():
        data=(FIXTURE/name).read_bytes()
        assert len(data)==item['bytes'] and hashlib.sha256(data).hexdigest()==item['sha256']
    assert hashlib.sha256((ROOT/manifest['generator']).read_bytes()).hexdigest()==manifest['generator_sha256']
    assert b'QPX_TMA_REFERENCE_OK 6' in (FIXTURE/'capture.log').read_bytes()


@pytest.mark.parametrize('index',range(6))
def test_qupath_export_round_trip_integer_geometry_and_metadata(index):
    created=REFERENCE['created']['cores'][index]
    result=REFERENCE['exported_then_imported']['cores'][index]
    assert result==wire('qupath-export.qptma')[index]
    assert result['bounds']==[math.trunc(v) if i<2 else math.ceil(v) for i,v in enumerate(created['bounds'])]
    assert all(result[key]==created[key] for key in ['name','missing','case_id'])


@pytest.mark.parametrize('index',range(6))
def test_fractional_wire_import_supported_by_pinned_qupath(index):
    created=REFERENCE['created']['cores'][index]
    result=REFERENCE['fractional_imported']['cores'][index]
    assert result==created==wire('fractional-import.qptma')[index]


def test_fixture_contains_required_tma_semantics_and_precision_edges():
    cores=REFERENCE['created']['cores']
    assert REFERENCE['image_size']==[256,192] and REFERENCE['synthetic'] is True
    assert all(REFERENCE[key]['rows']==2 and REFERENCE[key]['columns']==3 for key in ['created','exported_then_imported','fractional_imported'])
    assert sum(c['missing'] for c in cores)==2
    assert cores[0]['case_id']==cores[1]['case_id']=='case-one'
    assert cores[2]['case_id']=='' and cores[3]['case_id']=='patient \u03b1'
    assert cores[4]['case_id']=='comma, value' and cores[5]['case_id']=='quoted "case"'
    assert any(c['bounds']!=r['bounds'] for c,r in zip(cores,REFERENCE['exported_then_imported']['cores']))
