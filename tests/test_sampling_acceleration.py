"""Exact reference membership and concurrent first-use cache initialization."""
from concurrent.futures import ThreadPoolExecutor
from fractions import Fraction
import threading

import pytest
import qupathxx as q

pytestmark = pytest.mark.skipif(not q.has_geos, reason='polygon topology')


def parity(ring, point):
    px, py = map(Fraction, point)
    inside = False
    for a, b in zip(ring, ring[1:]+ring[:1]):
        ax, ay = map(Fraction, a); bx, by = map(Fraction, b)
        if (ay > py) == (by > py):
            continue
        crossing = ax+(bx-ax)*(py-ay)/(by-ay)
        if px < crossing:
            inside = not inside
    return inside


@pytest.mark.parametrize('origin', [0., 1048576.5, -1048576.5])
@pytest.mark.parametrize('reverse', [False, True])
def test_sparse_boundary_cells_match_exact_fraction_reference(origin, reverse):
    shell = [(0,0),(64,0),(64,64),(48,64),(48,24),(16,24),(16,64),(0,64)]
    hole = [(24,4),(40,4),(40,16),(24,16)]
    rings = [[(x+origin,y+origin) for x,y in ring] for ring in [shell,hole]]
    if reverse:
        rings = [list(reversed(ring)) for ring in rings]
    roi = q.Roi.polygon([q.Point(*p) for p in rings[0]], [[q.Point(*p) for p in rings[1]]])
    # Include exact grid lines, shell/hole boundaries and quarter-pixel points.
    points = [(origin+x/4,origin+y/4) for y in range(-1,258,7) for x in range(-1,258,7)]
    for ring in rings:
        for a,b in zip(ring,ring[1:]+ring[:1]):
            points += [a, ((a[0]+b[0])/2,(a[1]+b[1])/2)]
    for point in points:
        expected = parity(rings[0],point) and not parity(rings[1],point)
        assert roi.contains_sample(q.Point(*point),q.RoiSamplingPolicy.POLYGON_WINDING) == expected, point


def test_concurrent_first_winding_queries_share_one_native_geometry():
    roi = q.Roi.polygon([q.Point(0,0),q.Point(64,0),q.Point(64,64),q.Point(0,64)],
                        [[q.Point(16,16),q.Point(48,16),q.Point(48,48),q.Point(16,48)]])
    barrier = threading.Barrier(4)
    def queries(offset):
        barrier.wait(timeout=10)
        for index in range(256):
            x,y = (index+offset)%65,(index*7+offset)%65
            expected = x<64 and y<64 and not (16<=x<48 and 16<=y<48)
            assert roi.contains_sample(q.Point(x,y),q.RoiSamplingPolicy.POLYGON_WINDING) == expected
    with ThreadPoolExecutor(max_workers=4) as executor:
        list(executor.map(queries,range(4)))


def test_extreme_aspect_ratio_retains_exact_boundary_rules():
    width,height = 1e-308,1e150
    roi = q.Roi.polygon([q.Point(0,0),q.Point(width,0),q.Point(width,height),q.Point(0,height)])
    assert roi.contains_sample(q.Point(0,height/2),q.RoiSamplingPolicy.POLYGON_WINDING)
    assert not roi.contains_sample(q.Point(width,height/2),q.RoiSamplingPolicy.POLYGON_WINDING)
