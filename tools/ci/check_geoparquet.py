"""Verify GeoParquet image-pixel annotations through the native project route."""
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import shutil
import subprocess

import geopandas as gpd
import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq
from shapely import to_wkb
from shapely.affinity import affine_transform
from shapely.geometry import GeometryCollection, LineString, MultiLineString, MultiPoint, MultiPolygon, Point, Polygon


def sha(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "manifest", "reader-manifest", "template", "baseline", "output"):
        parser.add_argument("--" + key, type=lambda value: Path(value).resolve(), required=True)
    args = parser.parse_args()
    root = args.output
    inputs = root / "inputs"
    inputs.mkdir(parents=True)
    original = inputs / "author.parquet"
    shutil.copyfile(args.baseline.with_name("regions.parquet"), original)
    baseline = json.loads(args.baseline.read_bytes())
    assert sha(original) == baseline["fixture_sha"].lower() and baseline["exit_code"] == 2
    assert sha(args.host) == baseline["host_sha"].lower() and sha(args.template) == baseline["template_sha"].lower()
    table = pq.read_table(original)
    frame = gpd.read_parquet(original)
    assert frame.crs is None and frame["source_id"].tolist() == ["0001", "1", "μ"]
    provider, = json.loads(args.manifest.read_bytes())["plugins"]
    assert provider["provider_id"] == "org.qupathxx.geoparquet" and provider["abi"] == 3
    assert not provider["dependencies"]
    module = args.manifest.with_name(provider["module"]["path"])
    identity = sha(module), sha(args.manifest)
    assert identity[0] == provider["module"]["sha256"]
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    system = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    command = [args.host, "--reader-plugins", args.reader_manifest, "--format-plugins",
               args.manifest, "annotations-import", provider["provider_id"]]
    opts = {"coordinate_space": "image-pixels", "geometry": "geometry", "id": "source_id", "name": "title"}
    imports, reopens, refusals = {}, {}, {}
    native_pairs = {}
    registered_pairs = {}
    integer_cases = {}

    def run(argv, expected=0):
        result = subprocess.run(argv, cwd=root, env=env, encoding="utf-8", capture_output=True, timeout=30)
        assert result.returncode == expected, (argv, result.returncode, result.stdout, result.stderr)
        return result

    def argv(path, target, options, *extra, template=args.template):
        flags = [] if options is None else ["--plugin-options-json", json.dumps(options)]
        return [*command, template, "1", path, target, *flags, *extra]

    def accept(label, path, options=opts, *extra, template=args.template, ids=None):
        target = root / f"{label}.qpxproj"
        report = json.loads(run(argv(path, target, options, *extra, template=template)).stdout)
        if ids is None:
            ids = ["0001/0", "0001/1", "1", "μ"] if label == "collection" else ["0001", "1", "μ"]
        assert [item["source_id"] for item in report["objects"]] == ids
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        assert report["receipt"]["source_sha256"] == sha(path)
        imports[label] = report

    def refuse(label, path, diagnostic, options=opts):
        target = root / f"{label}.qpxproj"
        before = sha(path)
        result = run(argv(path, target, options), 2)
        error = json.loads(result.stderr)["error"]
        assert diagnostic.lower() in error.lower() and not target.exists() and sha(path) == before, error
        refusals[label] = error

    def author(label, dataframe, version="1.1.0", compression="snappy"):
        path = inputs / f"{label}.parquet"
        dataframe.to_parquet(path, schema_version=version, row_group_size=2, compression=compression)
        return path

    def write(label, contents):
        path = inputs / f"{label}.parquet"
        pq.write_table(contents, path, row_group_size=2)
        return path

    def change_geo(label, edit, contents=table):
        geo = json.loads(contents.schema.metadata[b"geo"])
        edit(geo, geo["columns"]["geometry"])
        metadata = {**contents.schema.metadata, b"geo": json.dumps(geo).encode()}
        return write(label, contents.replace_schema_metadata(metadata))

    accept("writer", original)
    alternate = frame.copy()
    alternate["alternate"] = frame.geometry.copy()
    alternate.geometry = [Point(0, 0)] * len(alternate)
    accept("alternate", author("two-geometries", alternate, compression=None), {**opts, "geometry": "alternate"})
    classified = frame.copy()
    classified["diagnosis"] = ["tumor", None, None]
    version10 = author("geoparquet10", classified, "1.0.0", "zstd")
    class_opts = {**opts, "class": "diagnosis"}
    refuse("unmapped-class", version10, "no explicit native class mapping", class_opts)
    class_template = root / "classified-template.qpxproj"
    run([args.probe, args.template, args.reader_manifest, "seed-classes", class_template])
    accept("version10", version10, class_opts, "--class-map-json", '{"tumor":"Tumor"}', template=class_template)

    grouped = frame.copy()
    grouped.geometry = [GeometryCollection([frame.geometry.iloc[0], Point(.25, .5)]), *frame.geometry.iloc[1:]]
    encoded = pq.read_table(author("groups-native", grouped))
    col = encoded.column_names.index("geometry")
    big_endian = pa.array(to_wkb(grouped.geometry.to_numpy(), byte_order=0), type=pa.binary())
    accept("collection", write("groups-big-endian", encoded.set_column(col, "geometry", big_endian)))

    refuse("missing-options", original, "requires explicit", None)
    refuse("wrong-space", original, "requires explicit image-pixels", {**opts, "coordinate_space": "world"})
    refuse("missing-column", original, "missing required json member", {**opts, "geometry": "absent"})
    refuse("missing-geo", write("no-geo", table.replace_schema_metadata({})), "geo metadata")
    refuse("unknown-version", change_geo("new-version", lambda meta, col: meta.update(version="2.0.0")),
           "GeoParquet version")
    refuse("implicit-crs84", change_geo("default-crs", lambda meta, col: col.pop("crs")), "explicit null CRS")
    refuse("wgs84", author("wgs84-src", frame.set_crs(4326)), "explicit null CRS")
    refuse("z-declaration", change_geo("3d-types", lambda meta, col: col.update(geometry_types=["Point Z"])),
           "basic XY types")
    refuse("type-mismatch", change_geo("wrong-types", lambda meta, col: col.update(geometry_types=["Point"])),
           "absent from GeoParquet geometry_types")
    refuse("unsupported-encoding", change_geo("different-encoding", lambda meta, col: col.update(encoding="geoarrow.point")),
           "requires WKB")
    refuse("spherical", change_geo("spherical-edges", lambda meta, col: col.update(edges="spherical")), "planar edges")
    refuse("bbox-mismatch", change_geo("wrong-bounds", lambda meta, col: col.update(bbox=[0., 0., 3., 3.])),
           "bbox differs")
    col = table.column_names.index("geometry")
    blobs = table["geometry"].to_pylist()
    refuse("null-wkb", write("null-shape", table.set_column(col, "geometry", pa.array([None, *blobs[1:]]))),
           "missing or null")
    refuse("trailing-wkb", write("trailing-shape", table.set_column(col, "geometry", pa.array([blobs[0] + b"\0", *blobs[1:]]))),
           "trailing GeoParquet WKB bytes")
    col = table.column_names.index("source_id")
    refuse("duplicate-id", write("duplicate-row-id", table.set_column(col, "source_id", pa.array(["0001", "0001", "μ"]))),
           "source IDs are not unique")

    integer_points = [Point(.5, .75), Point(2.5, 1.75), Point(1.25, .5)]
    for encoding in ("WKB", "geoarrow"):
        for dtype in ("int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64"):
            info = np.iinfo(dtype)
            middle = 2**53+1 if info.bits == 64 else 1
            values = np.array([info.min, middle, info.max], dtype=dtype)
            data = gpd.GeoDataFrame({"source_id": values, "title": ["First", "Middle", "Last"]},
                                    geometry=integer_points)
            label = f"{encoding.lower()}-{dtype}-ids"
            path = inputs / (label + ".parquet")
            data.to_parquet(path, schema_version="1.1.0", geometry_encoding=encoding, row_group_size=1)
            contents = pq.read_table(path)
            assert contents["source_id"].type == pa.from_numpy_dtype(values.dtype)
            assert contents["source_id"].to_pylist() == values.tolist()
            ids = [str(value) for value in values]
            accept(label, path, ids=ids)
            integer_cases[label] = (dtype, ids)
        col = contents.column_names.index("source_id")
        for issue, values, error in (
            ("duplicate", [1, 1, 2], "source IDs are not unique"),
            ("null", [None, 1, 2], "missing or null"),
            ("real", [0.5, 1.5, 2.5], "ID column must be UTF-8 or an integer"),
        ):
            label = f"{encoding.lower()}-{issue}-integer-id"
            invalid = contents.set_column(col, "source_id", pa.array(values))
            refuse(label, write(label, invalid), error)
    existing = root / "writer.qpxproj"
    before = sha(existing)
    run(argv(original, existing, opts), 2)
    assert sha(existing) == before
    refusals["overwrite"] = "existing project preserved"

    native_baseline = args.baseline.parent / "geoarrow"
    polygon_src = inputs / "native-polygons.parquet"
    shutil.copyfile(native_baseline / "regions-native.parquet", polygon_src)
    assert sha(polygon_src) == json.loads((native_baseline / "baseline.json").read_bytes())["fixture_sha"].lower()
    polygon = gpd.read_parquet(polygon_src)
    a, b = polygon.geometry
    lines = [LineString([(.5, .75), (1.5, 1.75)]), LineString([(2, 2), (3, 1), (3.5, .5)])]
    native = {
        "point": [Point(.5, .75), Point(2.5, 1.75)],
        "linestring": lines,
        "polygon": [a, b],
        "multipoint": [MultiPoint([(.5, .75), (1.5, 1.75)]), MultiPoint([(2.5, 1.75)])],
        "multilinestring": [MultiLineString(lines), MultiLineString([lines[1]])],
        "multipolygon": [MultiPolygon([a]), MultiPolygon([b, Polygon([(2, 2), (3, 2), (3, 3), (2, 2)])])],
    }
    for kind, shapes in native.items():
        data = polygon.copy()
        data.geometry = shapes
        encoded = inputs / (kind + "-native.parquet")
        if kind == "polygon":
            encoded = polygon_src
        else:
            data.to_parquet(encoded, schema_version="1.1.0", geometry_encoding="geoarrow",
                            row_group_size=1 if kind == "multipolygon" else 2,
                            compression="zstd" if kind == "multilinestring" else "snappy")
        assert gpd.read_parquet(encoded).geometry.equals(data.geometry)
        metadata = pq.read_metadata(encoded)
        assert json.loads(metadata.metadata[b"geo"])["columns"]["geometry"]["encoding"] == kind
        primary = "native-" + kind
        control = "wkb-" + kind
        ids = ["0001/0", "0001/1", "μ/0"] if kind == "multilinestring" else ["0001", "μ"]
        accept(primary, encoded, ids=ids)
        accept(control, author(control, data), ids=ids)
        native_pairs[primary] = control

    nested = pq.read_table(polygon_src)
    col = nested.column_names.index("geometry")
    tag = nested.schema.field(col)
    rows = nested["geometry"].to_pylist()
    broken = [part.copy() for part in rows[0]]
    broken[0] = broken[0][:-1]
    refuse("native-open-ring", write("open-ring", nested.set_column(col, tag, pa.array([broken, rows[1]], type=tag.type))),
           "explicitly closed")
    refuse("native-null", write("null-native", nested.set_column(col, tag, pa.array([None, rows[1]], type=tag.type))),
           "null list")
    refuse("native-shape-mismatch",
           change_geo("native-kind-changed", lambda meta, field: field.update(encoding="linestring", geometry_types=["LineString"]), nested),
           "separated XY struct")
    refuse("native-unknown-crs", change_geo("native-no-crs", lambda meta, field: field.pop("crs"), nested),
           "explicit null CRS")

    registration = args.baseline.parent / "registration"
    projected = inputs / "projected.parquet"
    shutil.copyfile(registration / projected.name, projected)
    assert sha(projected) == json.loads((registration / "baseline.json").read_bytes())["fixture_sha"].lower()
    profile = {**opts, "coordinate_space": "projected-xy", "source_crs": "EPSG:3857",
               "source_to_pixels": [0.5, 0, 500, 0, -0.25, 500]}
    accept("registered", projected, profile)
    assert json.loads(imports["registered"]["receipt"]["options"]) == profile
    transformed = polygon.copy()
    transformed.geometry = transformed.geometry.map(lambda shape: affine_transform(shape, [2, 2, 0, -4, -1000, 2000]))
    transformed = transformed.set_crs(3857)
    native_projected = inputs / "projected-native.parquet"
    transformed.to_parquet(native_projected, schema_version="1.1.0", geometry_encoding="geoarrow", row_group_size=1)
    shear = {**profile, "source_to_pixels": [0.5, 0.25, 0, 0, -0.25, 500]}
    accept("registered-native", native_projected, shear, ids=["0001", "μ"])
    assert json.loads(imports["registered-native"]["receipt"]["options"]) == shear
    registered_pairs["registered-native"] = "wkb-polygon"
    refuse("projected-without-registration", projected, "explicit null CRS")
    refuse("projected-without-affine", projected, "requires source_crs and source_to_pixels",
           {key: value for key, value in profile.items() if key != "source_to_pixels"})
    refuse("projected-wrong-epsg", projected, "differs from the declared EPSG", {**profile, "source_crs": "EPSG:3395"})
    refuse("projected-no-crs", original, "requires an explicit projected CRS", profile)
    refuse("geographic-affine", inputs / "wgs84-src.parquet", "requires an explicit projected CRS",
           {**profile, "source_crs": "EPSG:4326"})
    refuse("singular-registration", projected, "must be invertible", {**profile, "source_to_pixels": [1, 0, 0, 1, 0, 0]})
    refuse("ill-conditioned-registration", projected, "well-conditioned", {**profile, "source_to_pixels": [1, 0, 0, 0, 1e-13, 0]})
    refuse("off-image-registration", projected, "ROI must lie inside the image",
           {**profile, "source_to_pixels": [0.5, 0, 600, 0, -0.25, 500]})
    refuse("incorrect-projected-bbox",
           change_geo("target-bbox-in-metadata", lambda meta, field: field.update(bbox=[0, 0, 4, 3]), pq.read_table(projected)),
           "bbox differs", profile)
    refuse("pixel-crs-override", original, "cannot specify a source CRS", {**opts, "source_crs": "EPSG:3857"})

    inputs.rename(root / "inputs-unavailable")
    for label in imports:
        if label in native_pairs.values():
            continue
        saved = root / f"{label}.qpxproj"
        if label in integer_cases:
            kind, ids = integer_cases[label]
            exchange = root / (label + ".geojson")
            result = run([args.host, "geojson-export", saved, "1", exchange])
            features = json.loads(exchange.read_bytes())["features"]
            assert len(features) == len(ids)
            for feature, point, identifier, name in zip(features, integer_points, ids,
                                                        ("First", "Middle", "Last"), strict=True):
                assert feature["geometry"]["type"] == "Point"
                assert feature["geometry"]["coordinates"] == [point.x, point.y]
                assert feature["properties"]["name"] == name
                facts = feature["properties"]["metadata"]
                assert facts["qpx.annotation-plugin.source_id"] == identifier
                assert facts["qpx.annotation-plugin.source.geoparquet.source_id"] == identifier
                assert facts["qpx.annotation-plugin.source.geoparquet.source_id_type"] == kind
        elif label in native_pairs:
            reference = root / f"{native_pairs[label]}.qpxproj"
            result = run([args.probe, saved, args.reader_manifest, "compare-native", reference])
        elif label in registered_pairs:
            reference = root / f"{registered_pairs[label]}.qpxproj"
            result = run([args.probe, saved, args.reader_manifest, "compare-registered", reference])
        else:
            result = run([args.probe, saved, args.reader_manifest, label])
        reopens[label] = {"sha256": sha(saved), "stdout": result.stdout.strip()}
    assert sha(args.template) == baseline["template_sha"].lower()
    assert identity == (sha(module), sha(args.manifest))
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "producer": gpd.__version__, "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
        "module_sha256": identity[0], "manifest_sha256": identity[1],
        "imports": imports, "reopens": reopens, "refusals": refusals,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{len(imports)} imports, {len(reopens)} exact reopens, {len(refusals)} refusals")


if __name__ == "__main__":
    main()
