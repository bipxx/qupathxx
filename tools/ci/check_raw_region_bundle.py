"""Verify atomic native region bundles using Python's ZIP and NumPy readers."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

import numpy as np
from check_omero_bundle_verifier import change_member


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify(region, tiff, fits, zarr, output, legacy=None):
    output.mkdir(parents=True)
    checks = []
    roots = {tiff: sha(tiff.read_bytes()), fits / "science-bundle.fits": sha((fits / "science-bundle.fits").read_bytes())}
    roots.update({p: sha(p.read_bytes()) for p in zarr.rglob("*") if p.is_file()})

    def run(name, reader_args, source, dest, extra, error=None):
        argv = [region, *reader_args, "--raw", source, dest, *extra]
        result = subprocess.run(argv, capture_output=True, text=True, encoding="utf8", timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf8")
        checks.append({"name": name, "exit": result.returncode})
        if error:
            assert result.returncode == 2 and error in result.stderr, (name, result)
            return None
        assert result.returncode == 0, (name, result)
        return json.loads(result.stdout)

    def verify_zip(name, archive, error=None):
        before = sha(archive.read_bytes())
        result = subprocess.run([region, "--verify-raw-bundle", archive], capture_output=True,
                                text=True, encoding="utf8", timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf8")
        checks.append({"name": name, "exit": result.returncode, "archive_sha256": before})
        assert sha(archive.read_bytes()) == before
        if error:
            assert result.returncode == 2 and error in result.stderr and not result.stdout, (name, result)
        else:
            assert result.returncode == 0 and not result.stderr, (name, result)
            with zipfile.ZipFile(archive) as source:
                assert json.loads(result.stdout) == json.loads(source.read("receipt.json"))

    def extract_zip(name, archive, target, error=None):
        before = sha(archive.read_bytes())
        existed = target.is_file()
        previous = sha(target.read_bytes()) if existed else None
        result = subprocess.run([region, "--extract-raw-bundle", archive, target], capture_output=True,
                                text=True, encoding="utf8", timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf8")
        checks.append({"name": name, "exit": result.returncode, "archive_sha256": before})
        assert sha(archive.read_bytes()) == before
        if error:
            assert result.returncode == 2 and error in result.stderr and not result.stdout, (name, result)
            assert target.is_file() == existed
            if existed:
                assert sha(target.read_bytes()) == previous
        else:
            assert result.returncode == 0 and not result.stderr, (name, result)
            with zipfile.ZipFile(archive) as source:
                assert target.read_bytes() == source.read("raw.npy")
                assert json.loads(result.stdout) == json.loads(source.read("receipt.json"))
        assert not list(target.parent.glob(target.name + ".qpx-tmp-*"))

    tiff_opts = ("--channel", "0", "--channels", "3", "--x", "-1", "--width", "2", "--height", "2")
    fits_args = ("--reader-plugins", fits / "package-1.1.0/reader-plugins.json", "--reader", "org.qupathxx.fits",
                 "--reader-options", '{"hdu":2}')
    zarr_args = ("--reader", "ome-zarr")

    def bundle_case(name, reader_args, source, opts, expected_schema, scope):
        plain = output / (name + ".npy")
        bundled = output / (name + ".zip")
        previous = run(name + "-standalone", reader_args, source, plain, opts)
        report = run(name + "-bundle", reader_args, source, bundled, (*opts, "--bundle"))
        assert report["schema"] == previous["schema"] == expected_schema
        with zipfile.ZipFile(bundled) as archive:
            assert archive.namelist() == ["raw.npy", "receipt.json"] and archive.testzip() is None
            assert all(info.compress_type == zipfile.ZIP_STORED for info in archive.infolist())
            payload = archive.read("raw.npy")
            assert payload == plain.read_bytes()
            array = np.load(io.BytesIO(payload), allow_pickle=False)
            expected = np.load(plain, allow_pickle=False)
            assert array.dtype == expected.dtype and array.shape == expected.shape
            receipt = json.loads(archive.read("receipt.json"))
        assert receipt["schema"] == "qpx.raw-region-bundle/2" and receipt["array"] == "raw.npy"
        assert receipt["array_sha256"] == sha(payload) and len(receipt["receipt_sha256"]) == 64
        assert receipt["region"] == report
        observed = receipt["reader_source"]
        assert observed["scope"] == scope and Path(observed["locator"]) == source
        if "reader_snapshot" in report:
            assert observed == report["reader_snapshot"]["observed"]
        if "reader_source" in report:
            assert observed == report["reader_source"]
        checks[-1].update(array_sha256=sha(payload), archive_sha256=sha(bundled.read_bytes()))
        verify_zip(name + "-verify", bundled)
        extract_zip(name + "-extract", bundled, output / (name + "-extracted.npy"))
        return array, receipt

    bundle_case("plain-tiff", (), tiff, tiff_opts, "qpx.raw-region/1", "single_file")
    padded, _ = bundle_case("padded-tiff", (), tiff, (*tiff_opts, "--with-validity"),
                            "qpx.raw-region/3", "single_file")
    np.testing.assert_array_equal(padded["valid"], [[False, True], [False, True]])
    multi_chunk, _ = bundle_case("multi-chunk-validity", (), tiff,
                                 ("--channels", "3", "--x", "-1", "--width", "10000", "--height", "2",
                                  "--with-validity"), "qpx.raw-region/3", "single_file")
    assert multi_chunk.nbytes > 2 * 65536 and not multi_chunk["valid"][:, -1].any()
    measured, _ = bundle_case("fits-validity", fits_args, fits / "science-bundle.fits", ("--with-validity",),
                              "qpx.raw-region/3", "single_file")
    assert measured["valid"].any() and not measured["valid"].all()
    zarr_opts = ("--level", "1", "--level-coordinates", "--z", "1", "--time", "1", "--channels", "2")
    coarse, receipt = bundle_case("registered-zarr", zarr_args, zarr, zarr_opts,
                                 "qpx.raw-region/2", "directory_store")
    assert coarse.shape == (3, 4, 2) and tuple(coarse[2, 3]) == (10146, 11146)
    assert receipt["region"]["source_grid"]["dataset"] == "1"
    assert (receipt["calibration"]["base_pixel_width_um"], receipt["calibration"]["base_pixel_height_um"],
            receipt["calibration"]["level_downsample"]) == (1.5, 1, 2)

    isolated = output / "offline-source.ome.tif"
    shutil.copyfile(tiff, isolated)
    offline = output / "offline.zip"
    run("offline-create", (), isolated, offline, (*tiff_opts, "--with-validity", "--bundle"))
    isolated.rename(output / "moved-source.ome.tif")
    verify_zip("offline-verify", offline)
    extract_zip("offline-extract", offline, output / "offline-extracted.npy")

    def flip_last(data):
        return data[:-1] + bytes([data[-1] ^ 1])

    def alter_receipt(data):
        key = b'"receipt_sha256":"'
        index = data.index(key) + len(key)
        return data[:index] + (b"1" if data[index] == ord("0") else b"0") + data[index + 1:]

    archive = (output / "plain-tiff.zip").read_bytes()
    negative = {
        "bad-array-crc": (change_member(archive, "raw.npy", flip_last, False), "NPY CRC differs"),
        "bad-array-digest": (change_member(archive, "raw.npy", flip_last), "NPY digest differs"),
        "bad-receipt-digest": (change_member(archive, "receipt.json", alter_receipt), "receipt digest differs"),
        "bad-npy-type": (change_member(archive, "raw.npy", lambda a: a.replace(b"<u2", b"<i2", 1)),
                         "NPY type, shape or extent differs"),
        "extra-archive-byte": (archive + b"\0", "unexpected bytes after ZIP directory"),
        "short-archive": (archive[:-1], "truncated archive"),
    }
    if legacy:
        for prior in sorted(legacy.glob("*.zip")):
            verify_zip("legacy-" + prior.stem, prior)
            extract_zip("legacy-" + prior.stem + "-extract", prior, output / ("legacy-" + prior.stem + ".npy"))
        prior = (legacy / "padded-tiff.zip").read_bytes()
        negative["bad-legacy-valid-count"] = (change_member(prior, "raw.npy", flip_last), "validity count differs")
    for name, (data, diagnostic) in negative.items():
        invalid = output / (name + ".zip")
        invalid.write_bytes(data)
        verify_zip(name, invalid, diagnostic)
        extract_zip(name + "-extract", invalid, output / (name + "-extracted.npy"), diagnostic)

    existing = output / "plain-tiff.zip"
    prior = sha(existing.read_bytes())
    run("occupied", (), tiff, existing, ("--bundle",), error="already exists")
    assert sha(existing.read_bytes()) == prior
    extract_zip("occupied-extraction", existing, output / "plain-tiff-extracted.npy", "already exists")
    extract_zip("archive-as-destination", existing, existing, "already exists")
    alias = output / "archive-hardlink.npy"
    alias.hardlink_to(existing)
    extract_zip("archive-hardlink", existing, alias, "already exists")
    extract_zip("extract-missing-parent", existing, output / "absent" / "extract.npy", "cannot create export temporary file")
    run("source-alias", (), tiff, tiff, ("--bundle",), error="already exists")
    copy = output / "source.zarr"
    shutil.copytree(zarr, copy)
    inside = copy / "output.zip"
    run("source-directory", zarr_args, copy, inside, ("--bundle",), error="outside the protected input directory")
    missing = output / "missing" / "output.zip"
    run("missing-parent", (), tiff, missing, ("--bundle",), error="export")
    duplicate = output / "duplicate.zip"
    run("duplicate", (), tiff, duplicate, ("--bundle", "--bundle"), error="Duplicate --bundle")
    assert not inside.exists() and not missing.exists() and not duplicate.exists()
    assert not list(output.rglob("*.qpx-tmp-*"))
    assert all(sha(path.read_bytes()) == digest for path, digest in roots.items())
    (output / "validation.json").write_text(json.dumps({
        "schema": "qpx.raw-region-bundle-validation/1", "passed": True,
        "region_sha256": sha(region.read_bytes()), "inputs_unchanged": len(roots), "checks": checks,
    }, indent=2) + "\n", encoding="utf8")
    print(f"Passed {len(checks)} raw region bundle checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("region", "tiff", "fits", "zarr", "output"):
        parser.add_argument(name, type=Path)
    parser.add_argument("--legacy", type=Path)
    args = parser.parse_args()
    verify(*(getattr(args, n).resolve() for n in ("region", "tiff", "fits", "zarr", "output")),
           legacy=args.legacy.resolve() if args.legacy else None)
