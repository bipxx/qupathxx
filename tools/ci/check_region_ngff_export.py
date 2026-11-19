#!/usr/bin/env python3
"""Check installed raw CLI exports against literal authored NGFF 0.4 samples."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

import numpy as np


def verify(region, fixtures, output):
    output.mkdir(parents=True)
    checks = []
    original = json.loads((fixtures / "manifest.json").read_text(encoding="utf8"))["files"]

    def run(name, source, target, *options, error=None):
        argv = [region, "--reader", "ome-zarr", "--raw", source, target, *options]
        result = subprocess.run(argv, capture_output=True, text=True, encoding="utf8", timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf8")
        checks.append({"name": name, "exit": result.returncode})
        if error:
            assert result.returncode == 2 and error in result.stderr, (name, result)
            assert not target.exists(), name
            return None
        assert result.returncode == 0, (name, result)
        report = json.loads(result.stdout)
        observed = report["reader_source"]
        assert report["reader"] == "ome-zarr" and "reader_snapshot" not in report
        assert observed["schema"] == "qpx.reader-source/3" and observed["scope"] == "directory_store"
        assert Path(observed["locator"]) == source and Path(observed["primary"]["path"]) == source / ".zattrs"
        assert len(bytes.fromhex(observed["directory_sha256"])) == 32
        return report

    for name in ("plain", "little-c-zlib", "big-f-gzip"):
        source = fixtures / name
        target = output / (name + ".npy")
        report = run(name, source, target, "--level", "1", "--z", "1", "--time", "1",
                     "--channel", "1", "--channels", "2")
        expected = np.array([[[10100 + c * 1000 + 20 * y + 2 * x for c in (1, 2)]
                              for x in range(4)] for y in range(3)], dtype="<u2")
        np.testing.assert_array_equal(np.load(target, allow_pickle=False), expected, strict=True)
        assert report["schema"] == "qpx.raw-region/2" and report["valid_pixels"] == 12
        assert report["level_downsample"] == 2
        assert (report["pixel_width_um"], report["pixel_height_um"], report["z_spacing_um"],
                report["time_interval_seconds"]) == (3.0, 2.0, 4.0, 0.5)

    coarse = output / "coarse-structured.npy"
    coarse_report = run("coarse-structured", fixtures / "plain", coarse, "--level", "1", "--x", "2", "--y", "2",
                        "--width", "2", "--height", "1", "--with-validity")
    coarse_array = np.load(coarse, allow_pickle=False)
    np.testing.assert_array_equal(coarse_array["values"], np.array([[[22], [24]]], dtype="<u2"), strict=True)
    assert coarse_array["valid"].all() and coarse_report["schema"] == "qpx.raw-region/3"
    assert (coarse_report["level_downsample"], coarse_report["pixel_width_um"],
            coarse_report["pixel_height_um"]) == (2, 3.0, 2.0)
    assert coarse_report["source_grid"] == {
        "dataset": "1", "level_x": 1, "level_y": 1,
        "axes": ["t", "c", "z", "y", "x"],
        "units": ["second", "", "micrometer", "micrometer", "micrometer"],
        "scale": [0.5, 1, 4, 2, 3], "translation": [0, 0, 4, 23, 44],
    }

    translated = output / "translated.zarr"
    shutil.copytree(fixtures / "plain", translated)
    attrs = translated / ".zattrs"
    doc = json.loads(attrs.read_text(encoding="utf8"))
    level = doc["multiscales"][0]["datasets"][1]
    level["coordinateTransformations"][1]["translation"][-2:] = [9.5, 21.5]
    attrs.write_text(json.dumps(doc), encoding="utf8")
    dest = output / "translated.npy"
    report = run("translated", translated, dest, "--level", "1", "--x", "1", "--y", "1",
                 "--width", "3", "--height", "1", "--with-validity")
    assert (report["x"], report["y"]) == (1, 1)
    assert report["coordinate_space"] == "level-zero"
    assert report["source_grid"] == {**coarse_report["source_grid"], "level_x": -1,
                                     "translation": [0, 0, 4, 22, 47]}
    array = np.load(dest, allow_pickle=False)
    np.testing.assert_array_equal(array["values"], np.array([[[0], [20], [22]]], dtype="<u2"), strict=True)
    np.testing.assert_array_equal(array["valid"], [[False, True, True]])

    for structured in (False, True):
        layout = "structured" if structured else "plain"
        target = output / f"full-level-{layout}.npy"
        flags = ("--with-validity",) if structured else ()
        full = run(f"full-level-{layout}", translated, target, "--level", "1",
                   "--level-coordinates", "--z", "1", "--time", "1", "--channels", "2", *flags)
        grid = full["source_grid"]
        assert full["coordinate_space"] == "level-array" and full["valid_pixels"] == 12
        assert (full["x"], full["y"], grid["level_x"], grid["level_y"]) == (0, 0, 0, 0)
        assert grid["translation"] == [0, 0, 4, 22, 47]
        actual = np.load(target, allow_pickle=False)
        if structured:
            assert actual["valid"].all()
            actual = actual["values"]
        expected = np.array([[[10100 + c * 1000 + 20 * y + 2 * x for c in (0, 1)]
                              for x in range(4)] for y in range(3)], dtype="<u2")
        np.testing.assert_array_equal(actual, expected, strict=True)

    exact_target = output / "exact-level-crop.npy"
    exact = run("exact-level-crop", translated, exact_target, "--level", "1", "--level-coordinates",
                "--x", "-1", "--y", "1", "--width", "3", "--height", "1", "--with-validity")
    assert exact["coordinate_space"] == "level-array" and exact["source_grid"] == report["source_grid"]
    assert exact_target.read_bytes() == dest.read_bytes()
    run("level-coordinate-overflow", translated, output / "coordinate-overflow.npy", "--level", "1",
        "--level-coordinates", "--x", str(2**63 - 1), error="level-array coordinate overflow")
    run("duplicate-level-coordinates", translated, output / "duplicate-coordinates.npy",
        "--level-coordinates", "--level-coordinates", error="Duplicate --level-coordinates")

    source = fixtures / "plain"
    crop = ("--time", "1", "--channel", "1", "--channels", "2", "--x", "4", "--y", "3",
            "--width", "4", "--height", "2")
    target = output / "sparse-and-padded.npy"
    report = run("sparse-and-padded", source, target, *crop, "--with-validity")
    array = np.load(target, allow_pickle=False)
    assert report["schema"] == "qpx.raw-region/3" and report["array_layout"] == "values-valid"
    assert report["valid_pixels"] == 6
    assert (report["level_downsample"], report["pixel_width_um"], report["pixel_height_um"]) == (1, 1.5, 1.0)
    assert array.dtype == np.dtype([("values", "<u2", (2,)), ("valid", "?")])
    expected = np.array([[[11034, 7], [11035, 7], [11036, 7], [0, 0]],
                         [[11044, 7], [11045, 7], [11046, 7], [0, 0]]], dtype="<u2")
    np.testing.assert_array_equal(array["values"], expected, strict=True)
    np.testing.assert_array_equal(array["valid"], [[True, True, True, False]] * 2)
    in_bounds = run("same-source", source, output / "same-source.npy", "--width", "1", "--height", "1")
    assert in_bounds["reader_source"]["directory_sha256"] == report["reader_source"]["directory_sha256"]
    run("plain-padded-refused", source, output / "refused.npy", *crop, error="complete valid samples")
    run("bad-channel", source, output / "bad-channel.npy", "--channel", "3", error="channel")
    copied = output / "damaged.zarr"
    shutil.copytree(source, copied)
    run("inside-store", copied, copied / "new-export.npy", error="outside the protected input directory")
    (copied / "0" / "0.0.0.0.0").write_bytes(b"\0")
    run("damaged-chunk", copied, output / "damaged.npy", "--width", "1", "--height", "1", error="chunk")

    unknown = run("unknown-units", fixtures / "u8", output / "unknown-units.npy")
    assert unknown["level_downsample"] == 1
    assert "pixel_width_um" not in unknown and "pixel_height_um" not in unknown

    large = output / "large-spacing.zarr"
    shutil.copytree(source, large)
    attrs = large / ".zattrs"
    doc = json.loads(attrs.read_text(encoding="utf8"))
    multiscale = doc["multiscales"][0]
    multiscale["axes"][-1]["unit"] = "meter"
    multiscale["coordinateTransformations"][0]["scale"][-1] = 1
    multiscale["datasets"][0]["coordinateTransformations"][0]["scale"][-1] = 1e302
    multiscale["datasets"][1]["coordinateTransformations"][0]["scale"][-1] = 2e302
    attrs.write_text(json.dumps(doc), encoding="utf8")
    control = run("large-base-spacing", large, output / "large-base-spacing.npy", "--width", "1", "--height", "1")
    assert control["pixel_width_um"] == 1e308
    run("unrepresentable-spacing", large, output / "overflow.npy", "--level", "1",
        error="outside the physical calibration range")

    for entry in original:
        path = fixtures / entry["path"]
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"], path
    assert not list(output.rglob("*.qpx-tmp-*")) and not list(source.rglob("*.qpx-tmp-*"))
    (output / "validation.json").write_text(json.dumps({
        "schema": "qpx.raw-region-ngff-validation/1", "passed": True,
        "region_sha256": hashlib.sha256(region.read_bytes()).hexdigest(),
        "fixtures_unchanged": len(original), "checks": checks,
    }, indent=2) + "\n", encoding="utf8")
    print(f"Passed {len(checks)} built-in OME-Zarr raw export checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("region", type=Path)
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    verify(args.region.resolve(), args.fixtures.resolve(), args.output.resolve())
