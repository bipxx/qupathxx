"""Compare structured NPY against native and independently established values."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(region, retained, fits, primary, tiff, output):
    output.mkdir(parents=True)
    source = fits / "science-bundle.fits"
    manifest = fits / "package-1.1.0/reader-plugins.json"
    module = manifest.parent / "qpx_fits_plugin.dll"
    initial = {path: sha(path) for path in (region, retained, source, primary, manifest, module, tiff)}
    checks = []

    def run(name, binary, args, error=None):
        argv = list(map(str, [binary, *args]))
        result = subprocess.run(argv, cwd=output, capture_output=True, text=True, timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        checks.append({"name": name, "argv": argv, "exit": result.returncode})
        if error is not None:
            assert result.returncode == 2 and error in result.stderr, (name, result)
            return None
        assert result.returncode == 0, (name, result)
        return json.loads(result.stdout)

    def read_structure(path, expected):
        array = np.load(path, mmap_mode="r", allow_pickle=False)
        assert array.shape == expected.shape[:2] and array.dtype.names == ("values", "valid")
        assert array.dtype.itemsize == expected.dtype.itemsize * expected.shape[2] + 1
        values, valid = array["values"], array["valid"]
        assert values.dtype == expected.dtype and values.shape == expected.shape
        assert valid.dtype == np.dtype(bool) and valid.shape == expected.shape[:2]
        np.testing.assert_array_equal(values, expected, strict=True)
        return valid

    plugin = ["--reader-plugins", manifest, "--reader", "org.qupathxx.fits"]
    selected = [*plugin, "--reader-options", '{"hdu":2}', "--raw", source]
    refused = output / "unmasked.npy"
    run("plain-fits-refuses", region, [*selected, refused], error="requires complete valid samples")
    assert not refused.exists()
    structured = output / "complete.npy"
    report = run("structured-fits", region, [*selected, structured, "--with-validity"])
    assert report["schema"] == "qpx.raw-region/3" and report["array_layout"] == "values-valid"
    assert report["valid_pixels"] == 5 and report["reader_snapshot"]["implementation"]["options"] == {"hdu": 2}
    expected = np.array([[0.25, -1.5, 0], [5.5, 0, 7.25]], dtype=np.float32)[:, :, None]
    expected_mask = np.array([[True, True, True], [True, False, True]])
    np.testing.assert_array_equal(read_structure(structured, expected), expected_mask, strict=True)
    padded = output / "with-padding.npy"
    report = run("structured-padded", region, [*selected, padded, "--x", -1, "--width", 5,
                                                "--with-validity"])
    expected_pad = np.pad(expected, ((0, 0), (1, 1), (0, 0)))
    assert report["valid_pixels"] == 5
    np.testing.assert_array_equal(read_structure(padded, expected_pad),
                                  np.pad(expected_mask, ((0, 0), (1, 1))), strict=True)
    for binary, name in ((region, "current"), (retained, "retained")):
        target = output / (name + "-fits.npy")
        plain = run(name + "-fits-plain", binary, [*selected, target, "--width", 3, "--height", 1])
        assert plain["schema"] == "qpx.raw-region/2" and "array_layout" not in plain
    assert (output / "current-fits.npy").read_bytes() == (output / "retained-fits.npy").read_bytes()

    # Replace only the authored primary array payload, keeping its FITS framing.
    special = output / "special.fits"
    original = primary.read_bytes()
    bits = (0x80000000, 0x7f800000, 0xff800000, 0, 0x7fc00021, 1)
    special.write_bytes(original[:2880] + struct.pack(">6I", *bits) + original[2904:])
    target = output / "bits.npy"
    run("structured-float-bits", region, [*plugin, "--raw", special, target, "--with-validity"])
    native_bits = np.array((bits[0], bits[1], bits[2], bits[3], 0, bits[5]),
                           dtype=np.uint32).reshape(2, 3, 1)
    np.testing.assert_array_equal(read_structure(target, native_bits.view(np.float32)), expected_mask,
                                  strict=True)
    np.testing.assert_array_equal(np.load(target)["values"].view(np.uint32), native_bits, strict=True)

    # More than one writer batch, with missingness just before and after the boundary.
    height, width = 129, 257
    values = ((np.arange(height * width) % 65535) - 32767).astype(np.int16)
    positions = [0, 21844, 21845, len(values) - 1]
    values[positions] = -32768
    batch = output / "multiple-batches.fits"
    cards = [("SIMPLE", "T"), ("BITPIX", "16"), ("NAXIS", "2"),
             ("NAXIS1", str(width)), ("NAXIS2", str(height)), ("BLANK", "-32768")]
    header = "".join(f"{key:<8}= {value:>20}".ljust(80) for key, value in cards) + "END".ljust(80)
    payload = values.astype(">i2").tobytes()
    batch.write_bytes(header.encode("ascii").ljust(2880, b" ") + payload +
                      bytes(-len(payload) % 2880))
    target = output / "multiple-batches.npy"
    large_report = run("structured-multiple-batches", region, [*plugin, "--raw", batch, target,
                                                              "--with-validity"])
    assert large_report["valid_pixels"] == values.size - len(positions)
    expected_large = values.copy()
    expected_large[positions] = 0
    mask_large = values.reshape(height, width) != -32768
    np.testing.assert_array_equal(read_structure(target, expected_large.reshape(height, width, 1)),
                                  mask_large, strict=True)

    tiff_crop = ["--raw", tiff]
    tiff_args = ["--channels", 3, "--x", -1, "--width", 2, "--height", 2]
    for binary, name in ((region, "current"), (retained, "retained")):
        target = output / (name + "-tiff.npy")
        plain = run(name + "-tiff-plain", binary, [*tiff_crop, target, *tiff_args])
        assert plain["schema"] == "qpx.raw-region/1" and plain["valid_pixels"] == 2
    assert (output / "current-tiff.npy").read_bytes() == (output / "retained-tiff.npy").read_bytes()
    tiff_values = np.load(output / "retained-tiff.npy")
    target = output / "structured-tiff.npy"
    report = run("structured-tiff", region, [*tiff_crop, target, *tiff_args, "--with-validity"])
    assert report["schema"] == "qpx.raw-region/3" and report["valid_pixels"] == 2
    np.testing.assert_array_equal(read_structure(target, tiff_values),
                                  np.array([[False, True], [False, True]]), strict=True)
    run("occupied", region, [*selected, structured, "--with-validity"], error="already exists")
    alias = output / "alias-fits.npy"
    alias.hardlink_to(source)
    run("source-alias", region, [*selected, alias, "--with-validity"], error="already exists")
    duplicate = output / "duplicate.npy"
    run("duplicate", region, [*selected, duplicate, "--with-validity", "--with-validity"],
        error="Duplicate --with-validity")
    assert not duplicate.exists() and not list(output.glob("*.qpx-tmp-*"))
    assert all(sha(path) == digest for path, digest in initial.items())
    (output / "validation.json").write_text(json.dumps({
        "schema": "qpx.raw-validity-validation/1", "passed": True,
        "inputs": {str(path): digest for path, digest in initial.items()}, "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Passed {len(checks)} raw validity export checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("region", "retained", "fits", "primary", "tiff", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    verify(*(getattr(args, name).resolve() for name in
             ("region", "retained", "fits", "primary", "tiff", "output")))
