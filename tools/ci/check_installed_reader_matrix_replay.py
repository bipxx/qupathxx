"""Exercise two installed reader packs with native matrix and ROI consumers."""

import argparse
import csv
from hashlib import file_digest
import json
import os
from pathlib import Path
import shutil
import subprocess

import numpy as np


def sha(path):
    with path.open("rb") as stream:
        return file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("runtime", "metaimage-manifest", "metaimage-fixture", "vti-manifest",
                 "vti-fixture", "counts-manifest", "geometry-manifest", "output"):
        parser.add_argument("--" + name, type=lambda path: Path(path).resolve(), required=True)
    parser.add_argument("--probe", type=lambda path: Path(path).resolve(),
                        help="Create the fixture with the SDK probe for older hosts")
    parser.add_argument("--use-installed", action="store_true", help="Run directly from the supplied installation")
    parser.add_argument("--desktop", action="store_true", help="Also reopen the saved ROI project in the installed app")
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True)
    installed = args.runtime if args.use_installed else root / "Windows Native"
    dependencies = ("geos_c.dll", "geos.dll", "libopenslide-1.dll")
    files = ("qupathxx_region.exe", "qupathxx_format_import.exe", "qupathxx_batch.exe", *dependencies)
    probe = args.probe
    if not args.use_installed:
        installed.mkdir()
        for name in files:
            shutil.copyfile(args.runtime / name, installed / name)
        if probe:
            probe = installed / args.probe.name
            shutil.copyfile(args.probe, probe)
        if args.desktop:
            for name in ("ui", "fonts"):
                shutil.copytree(args.runtime / name, installed / name)
            shutil.copyfile(args.runtime / "qupathxx_app.exe", installed / "qupathxx_app.exe")

    def install_pack(label, manifest, selected):
        stage = manifest.parent if args.use_installed else root / "optional packs" / label
        target = stage / manifest.name
        if not args.use_installed:
            stage.mkdir(parents=True)
            shutil.copyfile(manifest, target)
        entries = json.loads(manifest.read_bytes())["plugins"]
        entry, = (entry for entry in entries if entry["provider_id"] == selected)
        for provider in entries:
            for asset in (provider["module"], *provider["dependencies"]):
                relative = Path(asset["path"])
                assert not relative.is_absolute() and ".." not in relative.parts
                destination = stage / relative
                if not args.use_installed:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(manifest.parent / relative, destination)
                assert sha(destination) == asset["sha256"]
        return target, entry

    meta_id = "org.qupathxx.metaimage"
    vti_id = "org.qupathxx.vti"
    counts_id = "org.qupathxx.counts-tsv"
    geometry_id = "org.qupathxx.geometry-tsv"
    meta, meta_entry = install_pack("MetaImage", args.metaimage_manifest, meta_id)
    vti, vti_entry = install_pack("VTK", args.vti_manifest, vti_id)
    counts, _ = install_pack("Counts", args.counts_manifest, counts_id)
    geometry, _ = install_pack("Geometry", args.geometry_manifest, geometry_id)
    package_files = {str(path): sha(path) for folder in (installed, meta.parent, vti.parent, counts.parent, geometry.parent)
                     for path in folder.rglob("*") if path.is_file()}
    readers = ["--reader-plugins", meta, "--reader-plugins", vti]
    reversed_readers = ["--reader-plugins", vti, "--reader-plugins", meta]
    formats = ["--format-plugins", counts, "--format-plugins", geometry]
    region, host, batch = (installed / name for name in files[:3])
    windows = Path(os.environ["SYSTEMROOT"])
    env = {key: value for key, value in os.environ.items()
           if not key.upper().startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, exe, *command, error=None, plain=False):
        result = subprocess.run([exe, *command], cwd=root, env=env, capture_output=True,
                                encoding="utf-8", timeout=60)
        (root / f"{label}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        assert result.returncode == (2 if error else 0), (label, result.stdout, result.stderr)
        checks[label] = result.returncode
        if error:
            assert error in result.stderr, (label, result.stderr)
            return None
        return result.stdout.strip() if plain else json.loads(result.stdout)

    source = root / "source data"
    source.mkdir()
    primary = source / "study.mhd"
    for name in (primary.name, "tail.zraw", "head.zraw"):
        shutil.copyfile(args.metaimage_fixture / name, source / name)
    vti_input = source / "arrays.vti"
    shutil.copyfile(args.vti_fixture, vti_input)
    matrix = source / "counts.tsv"
    matrix.write_text("observation_id\tgene\ninside\t9007199254740993\noutside\t5\n", encoding="utf-8")
    positions = source / "positions.tsv"
    positions.write_text("observation_id\tx_px\ty_px\noutside\t2.5\t0.5\ninside\t0.5\t0.5\n",
                         encoding="utf-8")
    roi = source / "roi.tsv"
    roi.write_text("id\tkind\tgeometry\tc\tz\tt\tclass\nregion\trectangle\t0,0,1,1\t-1\t0\t0\t\n",
                   encoding="utf-8")
    options = '{"spacing_unit":"mm","point_array":"Auxiliary"}'
    for label, reader, image, entry, flags, expected in (
        ("metaimage-z1", meta_id, primary, meta_entry, [], list(range(6, 12))),
        ("vti-auxiliary-z1", vti_id, vti_input, vti_entry, ["--reader-options", options],
         [1000 + 9 * code for code in range(6, 12)]),
    ):
        pixels = root / f"{label}.npy"
        report = run(label, region, *readers, "--reader", reader, *flags, "--raw", image, pixels, "--z", "1")
        raw = np.load(pixels, allow_pickle=False)
        assert raw.shape == (2, 3, 1) and raw.dtype == np.dtype("<u2") and raw.ravel().tolist() == expected
        assert report["valid_pixels"] == 6
        snapshot = report["reader_snapshot"]
        assert snapshot["implementation"]["module_sha256"] == entry["module"]["sha256"]
        assert tuple(snapshot["raw"][axis] for axis in ("pixel_width_um", "pixel_height_um", "z_spacing_um")) == (
            0.5, 1.5, 3.0)

    frame_digests = {}
    for point_array in ("Research samples", "Auxiliary"):
        qmx = root / f"{point_array}.qmx"
        report = run(point_array, host, *readers, *formats, "matrix-index", counts_id, matrix, positions,
                     vti_input, qmx, "--reader", vti_id, "--reader-options",
                     json.dumps({"spacing_unit": "mm", "point_array": point_array}))
        assert report["file_sha256"] == sha(qmx)
        info = run(point_array + "-frame", host, "matrix-info", qmx)
        frame = info["spatial_frame"]
        assert (frame["width"], frame["height"], frame["plane"]) == (
            3, 2, {"channel": -1, "z": 0, "time": 0})
        assert report["frame_sha256"] == info["frame_sha256"]
        frame_digests[point_array] = info["frame_sha256"]
    assert len(set(frame_digests.values())) == 2
    if not probe:
        picked = root / "selected-array.qpx"
        result = run("create-named-array-project", host, *reversed_readers, "project-create", vti_input, picked,
                     "--reader", vti_id, "--reader-options", options)
        assert (result["reader"], result["image_id"], result["width"], result["height"]) == (vti_id, 1, 3, 2)
        qmx = root / "Auxiliary.qmx"
        result = run("attach-matching-array", host, *readers, "matrix-attach", picked, "1",
                     qmx, sha(qmx), root / "matching-array.qpx")
        assert result["owned"] and result["frame_sha256"] == frame_digests["Auxiliary"]
        qmx = root / "Research samples.qmx"
        wrong = root / "wrong-array.qpx"
        run("refuse-other-array", host, *readers, "matrix-attach", picked, "1", qmx, sha(qmx), wrong,
            error="frame")
        assert not wrong.exists()
        picked_hash = sha(picked)
        run("refuse-existing-project", host, *readers, "project-create", vti_input, picked,
            "--reader", vti_id, "--reader-options", options, error="already exists")
        assert sha(picked) == picked_hash
        run("refuse-implicit-plugin-options", host, *readers, "project-create", vti_input, wrong,
            "--reader-options", options, error="explicit native reader plugin")
        assert not wrong.exists()

    project = source / "study.qpxproj"
    if probe:
        run("create-project", probe, "create", meta, source, "--slice-list", plain=True)
    else:
        result = run("create-project", host, *readers, "project-create", primary, project, "--reader", meta_id)
        assert result["reader"] == meta_id and result["image_id"] == 1
        companion = source / "head.zraw"
        before = sha(companion)
        run("protect-project-companion", host, *readers, "project-create", primary, companion,
            "--reader", meta_id, error="protect")
        assert sha(companion) == before
    original_hashes = {path.name: sha(path) for path in source.iterdir()}
    project_qmx = root / "project.qmx"
    index = ["matrix-project-index", counts_id, matrix, positions, project, "1"]
    indexed = run("project-index", host, *readers, *formats, *index, project_qmx)
    assert indexed["observations"] == indexed["entries"] == 2 and indexed["file_sha256"] == sha(project_qmx)
    attached = root / "attached.qpx"
    association = run("attach", host, *reversed_readers, "matrix-attach", project, "1", project_qmx,
                      indexed["file_sha256"], attached)
    assert association["owned"] and association["frame_sha256"] == indexed["frame_sha256"]
    project_qmx.rename(root / "original-index.qmx")
    annotated = root / "annotated.qpx"
    imported = run("roi-import", host, *reversed_readers, *formats, "annotations-import", geometry_id,
                   attached, "1", roi, annotated)
    if args.desktop:
        original_annotated = sha(annotated)
        run("desktop-reopen", installed / "qupathxx_app.exe", *readers, "--project", annotated,
            "--image-id", "1", "--smoke", plain=True)
        assert sha(annotated) == original_annotated
    annotation, = imported["objects"]
    spec = root / "aggregation.json"
    spec.write_text(json.dumps({
        "schema": "qpx.expression-aggregation-spec/1", "kind": "regional-summary", "overlap": "independent",
        "coordinates": "source", "regions": [{"id": "matched", "roi": annotation["object_id"]}],
    }), encoding="utf-8")
    saved = root / "aggregated.qpx"
    output = run("aggregate", batch, *readers, "--aggregate-expression", annotated, saved,
                 "--image", "1", "--matrix", association["association_id"], "--spec", spec)

    head = source / "head.zraw"
    replacement = source / "replacement.zraw"
    shutil.copyfile(head, replacement)
    replacement.replace(head)
    denied = root / "stale.qmx"
    run("reject-replacement", host, *readers, *formats, *index, denied, error="changed")
    assert not denied.exists() and original_hashes == {path.name: sha(path) for path in source.iterdir()}
    missing = root / "disconnected source data"
    source.rename(missing)
    flags = ["--image", "1", "--record", output["record"]]
    verified = run("offline-verify", batch, "--verify-expression-aggregation", saved, *flags)
    assert verified["result_sha256"] == output["result_sha256"]
    csv_path = root / "aggregation.csv"
    run("offline-export", batch, "--export-expression-aggregation", saved, csv_path, *flags)
    with csv_path.open(encoding="utf-8", newline="") as stream:
        row, = csv.DictReader(stream)
    assert (row["cohort_id"], row["observations"], row["sum"]) == ("matched", "1", "9007199254740993")
    assert original_hashes == {path.name: sha(path) for path in missing.iterdir()}
    assert package_files == {name: sha(Path(name)) for name in package_files}
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "checks": checks, "runtime_and_providers_sha256": package_files,
        "project_source_sha256": original_hashes, "vti_frame_sha256": frame_digests,
        "indexed_frame_sha256": indexed["frame_sha256"], "result_sha256": output["result_sha256"],
        "export_sha256": sha(csv_path), "exact_sum": row["sum"],
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Passed {len(checks)} installed reader/matrix/ROI checks")


if __name__ == "__main__":
    main()
