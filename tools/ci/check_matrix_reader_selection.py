"""Check direct matrix imports and saved projects with explicitly selected reader views."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("host", "batch", "previous", "prior-reader-frame-host", "project-probe", "output"):
        parser.add_argument("--" + name, type=lambda value: Path(value).resolve(), required=True)
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True)
    fits_root = BUILD / "fits-reader/hdu-candidate"
    fits = fits_root / "science-bundle.fits"
    fits_manifest = fits_root / "package-1.1.0/reader-plugins.json"
    apng_root = BUILD / "apng-intake"
    apng = apng_root / "apng.png"
    apng_manifest = apng_root / "package-1.0.0-final/reader-plugins.json"
    counts_manifest = BUILD / "format-family-plugins/package-final/format-plugins.json"
    ann_root = BUILD / "gui-aggregation/catalogue-closure/anndata-zarr"
    ann_manifest = ann_root / "package-sparse-final/bin/anndata-zarr-format-plugins.json"
    store = ann_root / "producer-blosc-01/default-blosc.zarr"
    checks = {}

    def run(name, executable, command, error=None):
        result = subprocess.run([executable, *command], cwd=root, capture_output=True,
                                encoding="utf-8", timeout=60)
        (root / f"{name}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        assert result.returncode == (2 if error else 0), (name, result.stdout, result.stderr)
        if error:
            assert error.lower() in result.stderr.lower(), (name, result.stderr)
        record = json.loads(result.stderr if error else result.stdout)
        checks[name] = record
        return record

    def pin(name, path, options):
        document = json.loads(path.read_bytes())
        plugin, = document["plugins"]
        assert not plugin["dependencies"]
        module = plugin["module"]["path"]
        plugin["options"] = options
        folder = root / name
        folder.mkdir()
        shutil.copyfile(path.parent / module, folder / module)
        assert sha(folder / module) == plugin["module"]["sha256"]
        manifest = folder / path.name
        manifest.write_text(json.dumps(document), encoding="utf-8")
        return manifest

    matrix = root / "counts.tsv"
    matrix.write_text("observation_id\tgene\nspot\t9007199254740993\n", encoding="utf-8")
    positions = root / "positions.tsv"
    positions.write_text("observation_id\tx_px\ty_px\nspot\t0.5\t0.5\n", encoding="utf-8")
    edge = root / "edge.tsv"
    edge.write_text("observation_id\tx_px\ty_px\nspot\t2.5\t0.5\n", encoding="utf-8")
    ppm = root / "legacy.ppm"
    ppm.write_bytes(b"P6\n4 2\n255\n" + bytes(range(24)))
    before = {str(path): sha(path) for path in (fits, apng, matrix, positions, edge, ppm)}

    def index(name, image, reader_manifest=None, flags=(), coords=positions, exe=args.host, error=None):
        output = root / f"{name}.qmx"
        start = ["--format-plugins", counts_manifest]
        if reader_manifest:
            start.extend(("--reader-plugins", reader_manifest))
        report = run(name, exe, [*start, "matrix-index", "org.qupathxx.counts-tsv", matrix,
                                 coords, image, output, *flags], error)
        if error:
            assert not output.exists()
        else:
            assert report["file_sha256"] == sha(output)
            assert report["features"] == report["observations"] == report["entries"] == 1
        return output, report

    legacy, _ = index("legacy-before", ppm, exe=args.previous)
    current, _ = index("legacy-automatic", ppm)
    explicit, _ = index("legacy-explicit", ppm, flags=("--reader", "ppm"))
    assert legacy.read_bytes() == current.read_bytes() == explicit.read_bytes()

    fits_reader = ("--reader", "org.qupathxx.fits")
    index("prior-select", fits, fits_manifest, (*fits_reader, "--reader-options", '{"hdu":2}'),
          edge, args.previous, "Unknown matrix import option")
    _, hdu = index("fits-hdu-2", fits, fits_manifest, (*fits_reader, "--reader-options", '{"hdu":2}'), edge)
    _, other_hdu = index("fits-hdu-1", fits, fits_manifest, (*fits_reader, "--reader-options", '{"hdu":1}'), edge)
    assert hdu["frame_sha256"] != other_hdu["frame_sha256"]
    index("fits-hdu-unavailable", fits, fits_manifest, (*fits_reader, "--reader-options", '{"hdu":63}'),
          edge, error="HDU")
    index("implicit-options", fits, fits_manifest, ("--reader-options", "{}"), error="explicit native reader plugin")
    index("built-in-options", ppm, flags=("--reader", "ppm", "--reader-options", "{}"),
          error="explicit native reader plugin")
    index("duplicate-reader", ppm, flags=("--reader", "ppm", "--reader", "ppm"), error="Duplicate")
    index("malformed-reader-options", fits, fits_manifest, (*fits_reader, "--reader-options", "{"),
          error="malformed JSON")

    store_positions = root / "store-positions.tsv"
    store_positions.write_text("observation_id\tx_px\ty_px\nspot-α\t0\t0\nspot-B\t1\t0\n"
                               "spot-C\t2\t0\nspot-D\t2.75\t0\n", encoding="utf-8")
    store_out = root / "selected-store.qmx"
    matrix_options = json.dumps({"matrix": "X", "observation_ids": "obs/_index",
                                 "feature_ids": "var/_index", "domain": "counts",
                                 "value_domain": "raw-counts", "feature_names": "var/feature_name"})
    report = run("selected-store", args.host, ["--reader-plugins", fits_manifest, "--format-plugins", ann_manifest,
                 "matrix-store-index", "org.qupathxx.anndata-zarr", store, ".zattrs", store_positions,
                 fits, store_out, *fits_reader, "--reader-options", '{"hdu":2}',
                 "--plugin-options-json", matrix_options])
    assert report["frame_sha256"] == hdu["frame_sha256"]
    assert report["source_scope"] == "directory_store" and report["file_sha256"] == sha(store_out)
    offline = run("provider-free-store-info", args.host, ["matrix-info", store_out])
    assert offline["frame_sha256"] == report["frame_sha256"] and offline["receipt"] == report["receipt"]

    pinned_fits = pin("pinned-fits", fits_manifest, {"hdu": 2})
    apng_reader = ("--reader", "org.qupathxx.apng")
    project_paths = []
    view_receipts = []
    for frame in (0, 1):
        pinned = pin(f"pinned-frame-{frame}", apng_manifest, {"frame": frame})
        project = root / f"frame-{frame}.qpx"
        command = [args.project_probe, "installed-create", project, pinned, apng_reader[1], apng,
                   pinned_fits, fits_reader[1], fits]
        fixture = subprocess.run(command, cwd=root, capture_output=True, encoding="utf-8", timeout=60)
        assert fixture.returncode == 0, (command, fixture.stderr)
        project_paths.append(project)
        qmx, receipt = index(f"view-{frame}", apng, apng_manifest,
                             (*apng_reader, "--reader-options", f'{{ "frame": {frame} }}'))
        default, _ = index(f"pinned-view-{frame}", apng, pinned, apng_reader)
        assert qmx.read_bytes() == default.read_bytes()
        view_receipts.append((qmx, receipt))
    assert view_receipts[0][1]["frame_sha256"] != view_receipts[1][1]["frame_sha256"]
    project_hashes = {path: sha(path) for path in project_paths}
    qmx, report = view_receipts[0]
    base = ["--reader-plugins", apng_manifest, "--reader-plugins", fits_manifest, "matrix-attach"]
    attached = root / "attached.qpx"
    result = run("matching-view-attach", args.host, [*base, project_paths[0], "1", qmx,
                 report["file_sha256"], attached])
    assert attached.exists() and result["frame_sha256"] == report["frame_sha256"]
    wrong = root / "wrong-view.qpx"
    run("alternate-view-attach", args.host, [*base, project_paths[1], "1", qmx,
        report["file_sha256"], wrong], "does not match the stored QMX frame")
    attached_store = root / "attached-store.qpx"
    result = run("matching-fits-store-attach", args.host, [*base, project_paths[0], "2", store_out,
                 sha(store_out), attached_store])
    assert attached_store.exists() and result["frame_sha256"] == hdu["frame_sha256"]

    project_command = [*base[:-1], "--format-plugins", counts_manifest, "matrix-project-index",
                       "org.qupathxx.counts-tsv", matrix, positions, project_paths[0], "1"]
    previous = root / "legacy-project-before.qmx"
    current = root / "project-indexed.qmx"
    older = run("legacy-project-before", args.prior_reader_frame_host, [*project_command, previous])
    current_report = run("project-indexed", args.host, [*project_command, current])
    assert current.read_bytes() == qmx.read_bytes()
    assert current_report["frame_sha256"] == report["frame_sha256"] != older["frame_sha256"]
    indexed_attached = root / "project-indexed-attached.qpx"
    indexed_saved = run("project-indexed-matching-attach", args.host, [*base, project_paths[0], "1",
                        current, sha(current), indexed_attached])
    assert indexed_saved["file_sha256"] == sha(current) and indexed_attached.exists()
    indexed_wrong = root / "project-indexed-wrong.qpx"
    run("project-indexed-alternate-attach", args.host, [*base, project_paths[1], "1", current,
        sha(current), indexed_wrong], "does not match the stored QMX frame")
    assert not indexed_wrong.exists()

    legacy_wrong = root / "legacy-wrong-attachment.qpx"
    legacy = run("legacy-wrong-attachment", args.prior_reader_frame_host, [*base, project_paths[1], "1",
                 previous, sha(previous), legacy_wrong])
    assert legacy_wrong.exists() and legacy["file_sha256"] == sha(previous)
    for frame, project in enumerate(project_paths):
        refused = root / f"legacy-refused-frame-{frame}.qpx"
        run(f"legacy-refused-frame-{frame}", args.host, [*base, project, "1", previous, sha(previous), refused],
            "requires a reader-bound QMX frame")
        assert not refused.exists()
    old_info = run("legacy-project-qmx-metadata", args.host, ["matrix-info", previous])
    assert old_info["file_sha256"] == older["file_sha256"] == sha(previous)
    assert old_info["frame_sha256"] == older["frame_sha256"]
    old_attached = root / "legacy-matching-attachment.qpx"
    old_saved = run("legacy-matching-attachment", args.prior_reader_frame_host, [
        *base, project_paths[0], "1", previous, sha(previous), old_attached])
    assert old_saved["file_sha256"] == sha(previous)

    project_store = root / "project-indexed-store.qmx"
    store_report = run("project-indexed-store", args.host, [*base[:-1], "--format-plugins", ann_manifest,
                       "matrix-project-store-index", "org.qupathxx.anndata-zarr", store, ".zattrs",
                       store_positions, project_paths[0], "2", project_store,
                       "--plugin-options-json", matrix_options])
    assert store_report["frame_sha256"] == hdu["frame_sha256"]
    assert project_store.read_bytes() == store_out.read_bytes()

    geometry_manifest = BUILD / "gui-aggregation/catalogue-closure/annotation-geometry/package-final/format-plugins.json"
    region = root / "roi.tsv"
    region.write_text("id\tkind\tgeometry\tc\tz\tt\tclass\nwhole\trectangle\t0,0,1,1\t-1\t0\t0\t\n",
                      encoding="utf-8")
    many_matrix = root / "paged-counts.tsv"
    many_positions = root / "paged-positions.tsv"
    many_matrix.write_text("observation_id\tgene\n" +
                           "".join(f"spot-{i:04}\t1\n" for i in range(1024)) +
                           "spot-1024\t9007199254740993\n", encoding="utf-8")
    many_positions.write_text("observation_id\tx_px\ty_px\n" +
                              "".join(f"spot-{i:04}\t1.5\t0.5\n" for i in range(1024)) +
                              "spot-1024\t0.5\t0.5\n", encoding="utf-8")
    paged = root / "paged.qmx"
    run("paged-project-index", args.host, [*project_command[:-4], many_matrix, many_positions,
        project_paths[0], "1", paged])
    paged_attached = root / "paged-attached.qpx"
    paged_saved = run("paged-project-attach", args.host, [*base, project_paths[0], "1",
                      paged, sha(paged), paged_attached])
    for name, project, attachment, version in (
        ("reader-bound", indexed_attached, indexed_saved, 2),
        ("legacy-owned", old_attached, old_saved, 1),
        ("second-page", paged_attached, paged_saved, 2),
    ):
        annotated = root / f"{name}-annotated.qpx"
        imported = run(f"{name}-import-roi", args.host, [*base[:-1], "--format-plugins", geometry_manifest,
                       "annotations-import", "org.qupathxx.geometry-tsv", project, "1", region, annotated])
        roi, = imported["objects"]
        spec = root / f"{name}-roi.json"
        spec.write_text(json.dumps({
            "schema": "qpx.expression-aggregation-spec/1", "kind": "regional-summary", "overlap": "independent",
            "frame_id": f"qpx.format-import-cli-image/{version}", "coordinates": "source",
            "regions": [{"id": "selected-roi", "roi": roi["object_id"]}],
        }), encoding="utf-8")
        aggregated = root / f"{name}-aggregated.qpx"
        saved = run(f"{name}-aggregate", args.batch, [*base[:-1], "--aggregate-expression", annotated,
                    aggregated, "--image", "1", "--matrix", attachment["association_id"], "--spec", spec])
        if name == "reader-bound":
            wrong_spec = root / "wrong-frame.json"
            wrong_spec.write_text(spec.read_text(encoding="utf-8").replace("cli-image/2", "cli-image/1"),
                                  encoding="utf-8")
            wrong_result = root / "wrong-frame-aggregate.qpx"
            run("wrong-frame-aggregate", args.batch, [*base[:-1], "--aggregate-expression", annotated,
                wrong_result, "--image", "1", "--matrix", attachment["association_id"], "--spec", wrong_spec],
                "No backed or indexed expression observations match")
            assert not wrong_result.exists()
        flags = ["--image", "1", "--record", saved["record"]]
        checked = run(f"{name}-offline-verify", args.batch, ["--verify-expression-aggregation", aggregated, *flags])
        assert checked["result_sha256"] == saved["result_sha256"]
        sums = root / f"{name}-sums.csv"
        run(f"{name}-offline-export", args.batch, ["--export-expression-aggregation", aggregated, sums, *flags])
        with sums.open(encoding="utf-8", newline="") as handle:
            row, = csv.DictReader(handle)
        assert (row["cohort_id"], row["observations"], row["sum"]) == ("selected-roi", "1", "9007199254740993")
    override = root / "project-override.qmx"
    run("saved-reader-override", args.host, [*project_command, override, *apng_reader],
        "Unknown matrix import option")
    assert not override.exists()
    assert not wrong.exists() and project_hashes == {path: sha(path) for path in project_paths}
    assert before == {path: sha(Path(path)) for path in before}
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "host_sha256": sha(args.host), "previous_sha256": sha(args.previous),
        "batch_sha256": sha(args.batch),
        "prior_reader_frame_host_sha256": sha(args.prior_reader_frame_host),
        "checks": checks, "input_sha256": before,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(checks)} exact and refusal checks")


if __name__ == "__main__":
    main()
