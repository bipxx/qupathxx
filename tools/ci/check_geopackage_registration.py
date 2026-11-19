"""Exercise registered GeoPackage geometry using the native import and reopen routes."""

import argparse
from contextlib import closing
import json
import os
from pathlib import Path
import shutil
import sqlite3
import struct
import subprocess

from check_project_matrix_index import sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "manifest", "reader-manifest", "template", "fixture", "output"):
        parser.add_argument("--" + key, type=lambda arg: Path(arg).resolve(), required=True)
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True)
    originals = {path: sha(path) for path in (args.template, args.fixture)}
    provider, = json.loads(args.manifest.read_bytes())["plugins"]
    module_sha = sha(args.manifest.with_name(provider["module"]["path"]))
    assert provider["module"]["sha256"] == module_sha and provider["version"] == "1.1.0"
    env = os.environ.copy()
    system = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    prefix = [args.host, "--reader-plugins", args.reader_manifest, "--format-plugins", args.manifest,
              "annotations-import", provider["provider_id"], args.template, "1"]
    options = {"layer": "rois", "name_column": "name", "coordinate_space": "projected-xy",
               "source_crs": "EPSG:3857", "source_to_pixels": [1, 0, 1000, 0, 1, -2000]}
    imports, reopens, refused = {}, {}, {}

    def run(argv, code=0):
        result = subprocess.run(argv, cwd=root, env=env, capture_output=True, encoding="utf-8", timeout=30)
        assert result.returncode == code, (argv, result.returncode, result.stdout, result.stderr)
        return result

    def variant(name, change=None):
        path = root / (name + ".gpkg")
        shutil.copyfile(args.fixture, path)
        if change:
            with closing(sqlite3.connect(path)) as db, db:
                change(db)
        return path

    def alter_feature(db, fid, edit):
        raw, = db.execute("SELECT geom FROM rois WHERE fid=?", (fid,)).fetchone()
        db.execute("UPDATE rois SET geom=? WHERE fid=?", (edit(raw), fid))

    def swap_binary_id(blob, new):
        order = "<" if blob[3] & 1 else ">"
        return blob[:4] + struct.pack(order + "i", new) + blob[8:]

    def local_srs(db):
        for table in ("gpkg_spatial_ref_sys", "gpkg_contents", "gpkg_geometry_columns"):
            db.execute(f"UPDATE {table} SET srs_id=10001 WHERE srs_id=3857")
        for (fid,) in db.execute("SELECT fid FROM rois"):
            alter_feature(db, fid, lambda geom: swap_binary_id(geom, 10001))

    def accept(name, source, selected):
        output = root / (name + ".qpx")
        report = json.loads(run([*prefix, source, output, "--plugin-options-json", json.dumps(selected)]).stdout)
        assert report["state"] == "saved"
        assert report["receipt"]["provider"]["module_sha256"] == module_sha
        assert report["receipt"]["source_sha256"] == sha(source)
        assert json.loads(report["receipt"]["options"]) == selected
        assert [item["source_id"] for item in report["objects"]] == [f"gpkg-fid-{i}" for i in (1, 2, 3)]
        imports[name] = report
        offline = source.with_suffix(".offline")
        source.rename(offline)
        try:
            reopened = run([args.probe, output, args.reader_manifest, name])
            reopens[name] = reopened.stdout.strip()
        finally:
            offline.rename(source)

    fixture = variant("copy-of-producer")
    accept("projected", fixture, options)
    accept("projected-reflected", fixture, {**options, "source_to_pixels": [-1, 0, -996, 0, 1, -2000]})
    accept("projected-local-srs", variant("local-srs", local_srs), options)

    def refuse(name, diagnostic, selected=options, source=fixture):
        output = root / (name + ".qpx")
        before = sha(source)
        failed = run([*prefix, source, output, "--plugin-options-json", json.dumps(selected)], 2)
        assert diagnostic in failed.stderr and not output.exists() and sha(source) == before, failed.stderr
        refused[name] = json.loads(failed.stderr)["error"]

    for key in ("source_crs", "source_to_pixels"):
        refuse("missing-" + key, "requires source_crs and source_to_pixels",
               {field: value for field, value in options.items() if field != key})
    refuse("unrequested-transform", "cannot specify a source CRS",
           {**options, "coordinate_space": "image-pixels"})
    refuse("different-epsg", "differs from the declared EPSG", {**options, "source_crs": "EPSG:32617"})
    refuse("noncanonical-epsg", "canonical EPSG", {**options, "source_crs": "EPSG:03857"})
    refuse("wrong-coefficients", "requires six row-major", {**options, "source_to_pixels": [1, 0]})
    refuse("singular-affine", "invertible and well-conditioned",
           {**options, "source_to_pixels": [1, 0, 1000, 0, 0, -2000]})
    refuse("outside-image", "ROI must lie inside the image",
           {**options, "source_to_pixels": [1, 0, 1001, 0, 1, -2000]})
    refuse("inconsistent-layer", "matching contents and geometry-column SRS IDs", source=variant(
        "inconsistent-layer-src", lambda db: db.execute("UPDATE gpkg_contents SET srs_id=4326")))
    refuse("wrong-organization", "differs from the declared EPSG", source=variant(
        "wrong-organization-src", lambda db: db.execute(
            "UPDATE gpkg_spatial_ref_sys SET organization='OTHER' WHERE srs_id=3857")))
    refuse("geographic-wkt", "requires a projected CRS declaration", source=variant(
        "geographic-wkt-src", lambda db: db.execute(
            "UPDATE gpkg_spatial_ref_sys SET definition=(SELECT definition FROM gpkg_spatial_ref_sys "
            "WHERE srs_id=4326) WHERE srs_id=3857")))
    refuse("binary-srs-mismatch", "feature SRS differs", source=variant(
        "binary-srs-mismatch-src", lambda db: alter_feature(db, 3, lambda blob: swap_binary_id(blob, 4326))))
    refuse("source-bounds-mismatch", "XY envelope differs", source=variant(
        "source-bounds-mismatch-src", lambda db: alter_feature(db, 3,
            lambda blob: blob[:8] + struct.pack("<d", 1.0) + blob[16:])))
    assert originals == {path: sha(path) for path in originals}
    report = {"passed": True, "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
              "module_sha256": module_sha, "manifest_sha256": sha(args.manifest),
              "fixture_sha256": sha(args.fixture), "imports": imports, "reopens": reopens, "refusals": refused}
    (root / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(imports)} imports, {len(reopens)} offline reopens and {len(refused)} refusals")


if __name__ == "__main__":
    main()
