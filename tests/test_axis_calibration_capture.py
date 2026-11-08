"""Pinned saved physical axes, independently checked before product implementation."""
import hashlib
import json
import math
from pathlib import Path

import numpy as np
import pytest
import tifffile

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-axis-calibration-0.7.0'
ROWS=json.loads((FIXTURE/'saved-reference.json').read_bytes())
EXPECTED={
    'nanoseconds':(4.5,[0,.25,.5,.75]),
    'microseconds':(4.5,[0,.25,.5,.75]),
    'milliseconds-offset':(4.5,[1.25,1.5,1.75,2]),
    'seconds':(4.5,[2,3,4,5]),
    'minutes-irregular':(4.5,[-30,0,45,120]),
    'hours':(4.5,[0,1800,3600,7200]),
    'days':(4.5,[0,86400,172800,259200]),
    'partial':(4.5,[0,3.25]),
    'nonmonotonic':(4.5,[5,3,3,-1]),
    'unknown':(None,[]),
}


def test_executed_probes_binary_source_and_reference_identity():
    manifest=json.loads((FIXTURE/'manifest.json').read_bytes())
    for name,sha in manifest['files'].items():assert hashlib.sha256((FIXTURE/name).read_bytes()).hexdigest()==sha
    for name,sha in manifest['probes'].items():assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==sha
    for entry,sha in manifest['reused_binary_hashes'].items():
        assert hashlib.sha256((FIXTURE/f'project/data/{entry}/data.qpdata').read_bytes()).hexdigest()==sha
    assert [c['returncode'] for c in manifest['capture_commands']]==[1,1,0,0]
    assert len(ROWS)==10 and set(r['name'] for r in ROWS)==set(EXPECTED)


@pytest.mark.parametrize('row',ROWS,ids=[r['name'] for r in ROWS])
def test_direct_saved_reference_matches_independent_physical_values(row):
    z,times=EXPECTED[row['name']];actual=float.fromhex(row['z_spacing_um'])
    assert math.isnan(actual) if z is None else actual==z
    assert [float.fromhex(t) for t in row['seconds']]==times
    assert row['z_planes']==3 and row['timepoints']==4
    factor={'NANOSECONDS':1e-9,'MICROSECONDS':1e-6,'MILLISECONDS':1e-3,'SECONDS':1,'MINUTES':60,'HOURS':3600,'DAYS':86400}[row['time_unit']]
    assert [float.fromhex(t)*factor for t in row['times']]==times


def test_source_pixels_and_acquisition_axes_are_independent():
    import qupathxx as q
    if not q.has_tiff:pytest.skip('TIFF')
    expected=np.arange(4*3*16*20,dtype=np.uint16).reshape(4,3,16,20)
    np.testing.assert_array_equal(tifffile.imread(FIXTURE/'source.ome.tif'),expected)
    source=q.open_tiff(str(FIXTURE/'source.ome.tif'));m=source.raw_metadata
    assert (m.z_planes,m.timepoints)==(3,4)
    assert (m.z_spacing_um,m.time_interval_seconds)==(1.75,.25)


def test_project_builder_reference_cannot_substitute_for_saved_calibration():
    entry_rows=json.loads((FIXTURE/'reference.json').read_bytes())
    for actual,saved in zip(entry_rows[:7],ROWS[:7],strict=True):
        assert actual['reused'] and float.fromhex(actual['z_spacing_um'])==1.75
        assert float.fromhex(saved['z_spacing_um'])==4.5
    assert {r['time_unit'] for r in ROWS[:7]}=={'NANOSECONDS','MICROSECONDS','MILLISECONDS','SECONDS','MINUTES','HOURS','DAYS'}
