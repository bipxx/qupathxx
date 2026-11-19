"""Check the native GeoPackage provider with retained GDAL and authored controls."""
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
    for key in ("host", "probe", "manifest", "wkb-manifest", "reader-manifest", "template", "baseline", "output"):
        parser.add_argument("--" + key, type=lambda arg: Path(arg).resolve(), required=True)
    args = parser.parse_args()
    out = args.output
    source_dir = out / "sources"
    source_dir.mkdir(parents=True)
    original = sha(args.template)
    baseline = json.loads(args.baseline.read_bytes())
    assert sha(args.host) == baseline["host_sha"].lower() and original == baseline["template_sha"].lower()
    provider, = json.loads(args.manifest.read_bytes())["plugins"]
    module = args.manifest.with_name(provider["module"]["path"])
    identity = sha(module), sha(args.manifest)
    assert provider["provider_id"] == "org.qupathxx.geopackage" and provider["abi"] == 3
    assert provider["module"]["sha256"] == identity[0]
    fixture = source_dir / "writer.gpkg"
    shutil.copyfile(args.baseline.with_name("study.gpkg"), fixture)
    assert sha(fixture) == baseline["fixture_sha"].lower()
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    prefix = [args.host, "--reader-plugins", args.reader_manifest]
    cmd = [*prefix, "--format-plugins", args.manifest, "annotations-import", provider["provider_id"], args.template, "1"]
    opts = {"layer": "rois", "name_column": "name", "coordinate_space": "image-pixels"}
    checks = {"imports": [], "reopens": [], "refusals": [], "shared_wkb": []}

    def run(argv, code=0):
        p = subprocess.run(argv, cwd=out, env=env, capture_output=True, encoding="utf-8", timeout=30)
        assert p.returncode == code, (argv, p.returncode, p.stdout, p.stderr)
        return p

    def invoke(src, dest, options=opts):
        argv = [*cmd, src, dest]
        return argv if options is None else [*argv, "--plugin-options-json", json.dumps(options)]

    outputs = []
    def accept(label, path, options):
        output = out / (label + ".qpxproj")
        report = json.loads(run(invoke(path, output, options)).stdout)
        assert report["receipt"]["source_sha256"] == sha(path)
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        assert [r["source_id"] for r in report["objects"]] == ["gpkg-fid-1", "gpkg-fid-2", "gpkg-fid-3"]
        outputs.append((label, output))
        checks["imports"].append(report)

    def database(label, change):
        file = source_dir / (label + ".gpkg")
        shutil.copyfile(fixture, file)
        with closing(sqlite3.connect(file)) as db, db:
            change(db)
        return file

    def change_blob(db, fid, change):
        raw, = db.execute("SELECT geom FROM rois WHERE fid=?", (fid,)).fetchone()
        db.execute("UPDATE rois SET geom=? WHERE fid=?", (change(raw), fid))

    accept("writer", fixture, opts)

    def big_renamed(db):
        change_blob(db, 1, lambda raw: b"GP\0\0" + struct.pack(">iBIdd", -1, 0, 1, .5, .75))
        db.execute('ALTER TABLE rois RENAME TO "microscopy ""regions"""')
        for table in ("gpkg_contents", "gpkg_geometry_columns"):
            db.execute(f"UPDATE {table} SET table_name=? WHERE table_name='rois'", ('microscopy "regions"',))

    accept("quoted-big-endian", database("renamed", big_renamed),
           {"layer": 'microscopy "regions"', "coordinate_space": "image-pixels"})

    def refuse(label, path, diagnostic, selected=opts):
        output = out / (label + ".qpxproj")
        before = sha(path)
        report = run(invoke(path, output, selected), 2)
        assert diagnostic in report.stderr and not output.exists() and sha(path) == before, report.stderr
        checks["refusals"].append(label)

    refuse("implicit-selection", fixture, "requires explicit", None)
    refuse("geographic-interpretation", fixture, "must be image-pixels", {**opts, "coordinate_space": "degrees"})
    refuse("missing-layer", fixture, "ordinary main-schema table", {**opts, "layer": "missing"})
    refuse("missing-name", fixture, "column is absent", {**opts, "name_column": "missing"})
    refuse("z-axis", database("z", lambda db: db.execute("UPDATE gpkg_geometry_columns SET z=1")), "without Z/M")
    refuse("epsg-crs", database("epsg", lambda db: db.execute("UPDATE gpkg_geometry_columns SET srs_id=4326")),
           "undefined Cartesian SRS -1")
    refuse("wrong-binary-srs", database("wrong-srs", lambda db: change_blob(
        db, 1, lambda raw: raw[:4]+struct.pack("<i", 4326)+raw[8:])), "feature SRS must be -1")
    refuse("bad-envelope", database("envelope", lambda db: change_blob(
        db, 2, lambda raw: raw[:8]+struct.pack("<d", 0)+raw[16:])), "envelope differs")
    refuse("extended-flag", database("extended", lambda db: change_blob(
        db, 1, lambda raw: raw[:3]+bytes([raw[3]|32])+raw[4:])), "Unsupported extended")
    refuse("trailing-wkb", database("trailing", lambda db: change_blob(
        db, 1, lambda raw: raw+b"\0")), "Trailing GeoPackage WKB")
    refuse("view", database("view-src", lambda db: db.executescript(
        "ALTER TABLE rois RENAME TO saved; CREATE VIEW rois AS SELECT * FROM saved;")), "ordinary main-schema table")
    pending = database("pending-src", lambda _: None)
    sidecar = pending.with_name(pending.name + "-journal")
    sidecar.write_bytes(b"authored guard control")
    refuse("sidecar", pending, "SQLite sidecar present")
    sidecar.unlink()

    point = source_dir / "regression.wkb"
    point.write_bytes(struct.pack("<BIdd", 1, 1, .5, .75))
    wkb_out = out / "wkb-regression.qpxproj"
    run([*prefix, "--format-plugins", args.wkb_manifest, "annotations-import", "org.qupathxx.wkt-wkb",
         args.template, "1", point, wkb_out])
    outputs.append(("wkb", wkb_out))
    source_dir.rename(out / "unavailable")
    for label, output in outputs:
        result = run([args.probe, output, args.reader_manifest, label])
        group = "shared_wkb" if label == "wkb" else "reopens"
        checks[group].append({"stdout": result.stdout.strip(), "project_sha256": sha(output)})
    assert sha(args.template) == original and identity == (sha(module), sha(args.manifest))
    (out / "validation.json").write_text(json.dumps({
        "passed": True, "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
        "module_sha256": identity[0], "manifest_sha256": identity[1], **checks,
    }, indent=2) + "\n", encoding="utf-8")
    print({key: len(items) for key, items in checks.items()})


if __name__ == "__main__":
    main()
