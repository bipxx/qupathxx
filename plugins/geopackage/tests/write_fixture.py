"""Author and verify image-pixel or projected fixtures with GDAL."""
import argparse
from pathlib import Path

import numpy as np
from pyogrio import raw
from shapely.affinity import translate
from shapely import from_wkb, to_wkb
from shapely.geometry import LineString, Point, Polygon


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--projected", action="store_true")
    args = parser.parse_args()
    geometries = [
        Point(.5, .75),
        LineString([(1, .5), (2, 1.5), (3, 1)]),
        Polygon([(0, 0), (4, 0), (4, 3), (0, 3)], holes=[[(1, 1), (3, 1), (3, 2), (1, 2)]]),
    ]
    names = ["Spot", "Trace", "Region"]
    if args.projected:
        geometries = [translate(item, xoff=-1000, yoff=2000) for item in geometries]
        crs, layer_options = "EPSG:3857", {"SPATIAL_INDEX": "NO"}
    else:
        crs, layer_options = None, {"SRID": "-1", "SPATIAL_INDEX": "NO"}
    raw.write(args.output, to_wkb(geometries), [np.array(names, dtype=object)], ["name"],
              layer="rois", driver="GPKG", geometry_type="Unknown", crs=crs, layer_options=layer_options)
    _, ids, payload, fields = raw.read(args.output, layer="rois", return_fids=True)
    assert ids.tolist() == [1, 2, 3] and fields[0].tolist() == names
    assert all(expected.equals_exact(actual, 0) for expected, actual in zip(geometries, from_wkb(payload)))


if __name__ == "__main__":
    main()
