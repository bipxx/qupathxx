"""Pinned class facts; native hierarchy integration is the remaining CLASS1 task."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-class-hierarchy-0.7.0'
MANIFEST=json.loads((FIXTURE/'manifest.json').read_bytes())
REFERENCE={row['name']:row for row in json.loads((FIXTURE/'reference.json').read_bytes())}
PRODUCER={row['name']:row for row in json.loads((FIXTURE/'producer-reference.json').read_bytes())['cases']}


def test_class_reference_probes_parent_and_retained_geometry():
    for name,digest in MANIFEST['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==digest
    for name,digest in MANIFEST['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest
    parent=ROOT/'tests/fixtures'/MANIFEST['parent']['fixture'];assert hashlib.sha256((parent/'manifest.json').read_bytes()).hexdigest()==MANIFEST['parent']['manifest_sha256']
    for name in ['rgb','rgba','null-color','color-conflict']:
        before={row['id']:row for row in PRODUCER[name]['objects']};after={row['id']:row for row in REFERENCE[name]['native']}
        assert before.keys()==after.keys() and all(row['area']==before[key]['area']==120 for key,row in after.items())


def test_palette_order_duplicates_null_sentinel_and_zero_images():
    expected=[['Tumor α','Positive','High'],['Tumor α'],['Tumor α','Negative'],['Tumor α','Positive'],['Cell µ 🚀'],['Marker / label #1'],['Tumor α','Positive','High']]
    assert [row['path'] for row in REFERENCE['rgb']['palette']]==expected
    assert REFERENCE['empty']['palette']==REFERENCE['rgb']['palette'] and REFERENCE['empty']['native']==[]
    assert len(PRODUCER['rgb']['palette_input'])==8 and PRODUCER['rgb']['palette_input'][-1]['name'] is None
    assert json.loads((FIXTURE/'empty/project.qpproj').read_bytes())['images']==[]
    rejected=json.loads((FIXTURE/'producer-reference.json').read_bytes())['rejected_names'];assert len(rejected)==2 and all(row['rejected'] for row in rejected)


def test_explicit_path_and_argb_survive_native_and_geojson():
    for name in ['rgb','rgba']:
        expected={row['id']:row['classification'] for row in PRODUCER[name]['objects']}
        for route in ['native','geojson']:assert {row['id']:row['classification'] for row in REFERENCE[name][route]}==expected
    rgba=REFERENCE['rgba']['native'][0]['classification'];assert rgba['path']==['Tumor α','Alpha'] and rgba['argb']&0xffffffff==0x80123456
    geo=json.loads((FIXTURE/'rgb/objects.geojson').read_bytes())
    assert geo['features'][1]['properties']['classification']['names']==['Tumor α','Positive']


def test_fresh_process_color_priority_and_upstream_colorless_geojson_failure():
    conflict=REFERENCE['color-conflict'];assert PRODUCER['color-conflict']['objects'][0]['classification']['argb']==0x101112
    assert conflict['palette'][0]['argb']==0xa0b0c0
    for route in ['native','geojson']:assert conflict[route][0]['classification']['argb']==0xa0b0c0
    empty_color=REFERENCE['null-color'];assert PRODUCER['null-color']['objects'][0]['classification']['argb'] is None
    assert empty_color['native'][0]['classification']['argb']==empty_color['palette'][0]['argb'] and empty_color['palette'][0]['argb'] is not None
    assert 'color' not in json.loads((FIXTURE/'null-color/classifiers/classes.json').read_bytes())['pathClasses'][0]
    assert empty_color['geojson']==[] and 'Unable to parse PathClass' in empty_color['geojson_error']['message']


def test_default_color_oracle_covers_independent_contexts_and_rejection_input():
    reference=json.loads((FIXTURE/'default-colors.json').read_bytes());rows=reference['cases'];assert len(rows)==38
    roots={row['path'][0]:row['argb']&0xffffffff for row in rows if len(row['path'])==1}
    assert roots['Positive']==0xfffa3e3e and roots['Negative']==0xff7070e1 and roots['1+']==0xffffff00
    assert roots['é']!=roots['e\u0301'] and 'Class 🚀' in roots and reference['dark_input']=='Dark candidate 141'
    assert roots[reference['dark_input']]!=0xff0d2615
    assert any(len(row['path'])==3 for row in rows) and any(row['parent_argb'] is not None and row['parent_argb']<0 for row in rows)
