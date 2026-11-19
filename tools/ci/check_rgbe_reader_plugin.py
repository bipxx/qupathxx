"""Verify native Radiance RGBE against authored codes and an independent writer."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

import imagecodecs
import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def header(width, height, content=b"", ending=b"\n"):
    resolution = f"-Y {height} +X {width}".encode()
    return b"\n".join((b"#?RADIANCE", b"FORMAT=32-bit_rle_rgbe", content, resolution, b"")).replace(b"\n", ending)


def decode(codes):
    exponents = codes[..., 3:].astype(np.int32)
    centers = codes[..., :3].astype(np.float64) + 0.5
    return np.where(exponents, np.ldexp(centers, exponents - 136), 0).astype("<f4")


def display(raw):
    linear = raw.astype(np.float64)
    mapped = linear / (1 + linear.max(axis=2, keepdims=True))
    encoded = np.where(mapped <= 0.0031308, 12.92 * mapped, 1.055 * mapped ** (1 / 2.4) - 0.055)
    return np.concatenate((np.floor(encoded * 255 + 0.5).astype(np.uint8),
                           np.full((*raw.shape[:2], 1), 255, dtype=np.uint8)), axis=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("region", "manifest", "probe", "baseline", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir()
    manifest = args.manifest.resolve()
    entry, = json.loads(manifest.read_bytes())["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    module_hash, manifest_hash = sha(module), sha(manifest)
    assert entry["provider_id"] == "org.qupathxx.rgbe" and entry["module"]["sha256"] == module_hash
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", entry["provider_id"]]
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {kind: [] for kind in ("raw", "display", "refusals", "project")}

    def run(cmd, code=0):
        result = subprocess.run(cmd, cwd=output, env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == code, (cmd, result.returncode, result.stdout, result.stderr)
        return result.stdout, result.stderr

    def fixture(name, contents):
        path = output / (name + ".hdr")
        path.write_bytes(contents)
        return path

    def read(source, expected, bounds=None):
        height, width, _ = expected.shape
        left, top = bounds or (0, 0)
        label = source.stem + ("-crop" if bounds else "")
        target = output / (label + ".npy")
        stdout, _ = run(command + ["--raw", source, target, "--channels", "3", "--x", str(left), "--y", str(top),
                                   "--width", str(width), "--height", str(height)])
        report = json.loads(stdout)
        actual = np.load(target, allow_pickle=False)
        assert actual.shape == expected.shape and actual.dtype == expected.dtype
        assert actual.tobytes() == expected.tobytes() and report["valid_pixels"] == width * height, label
        snapshot = report["reader_snapshot"]
        assert snapshot["implementation"]["module_sha256"] == module_hash and snapshot["implementation"]["options"] == {}
        assert snapshot["observed"]["scope"] == "single_file"
        assert snapshot["metadata"]["vendor"] == "Radiance RGBE (linear)"
        assert snapshot["raw"]["channel_names"] == ["Red", "Green", "Blue"]
        checks["raw"].append(label)
        pam = output / (label + ".pam")
        run(command + [source, str(left), str(top), "0", str(width), str(height), pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == display(expected).tobytes(), label
        checks["display"].append(label)

    exposure = np.exp2(np.linspace(-8, 12, 48, dtype=np.float32)).reshape(4, 12)
    ramp = np.stack((exposure, exposure * 0.5, exposure * 0.25), axis=-1)
    baseline = args.baseline.resolve()
    assert imagecodecs.rgbe_encode(ramp, header=True, rle=True) == baseline.read_bytes()
    _, exponent = np.frexp(exposure)
    mantissas = np.ldexp(ramp, (8 - exponent)[..., None]).astype(np.uint8)
    reconstructed = decode(np.concatenate((mantissas, (exponent + 128).astype(np.uint8)[..., None]), axis=2))
    read(baseline, reconstructed)
    read(baseline, reconstructed[1:3, 3:9], (3, 1))

    codes = np.array([[[128, 64, 32, 129], [255, 32, 32, 0], [0, 0, 128, 1]],
                      [[255, 128, 0, 255], [1, 0, 255, 129], [128, 129, 130, 128]]], dtype=np.uint8)
    flat = fixture("flat", header(3, 2) + codes.tobytes())
    read(flat, decode(codes))
    alternate = header(3, 2, b"# authored data\n", b"\r\n").replace(b"#?RADIANCE", b"#?RGBE")
    read(fixture("rgbe-crlf", alternate + codes.tobytes()), decode(codes))

    width = 128
    red = np.arange(128, 256, dtype=np.uint8)
    # Literal 128, maximum repeat 127, a one-byte run, and a changing exponent.
    payload = bytes([2, 2, 0, width, 128]) + red.tobytes()
    payload += bytes([255, 64, 129, 0, 192, 32, 192, 128, 192, 128, 192, 133])
    row = np.stack((red, [64] * 127 + [0], [32] * 64 + [128] * 64, [128] * 64 + [133] * 64), axis=-1)
    second = np.tile([192, 0, 16, 137], (width, 1))
    mixed = fixture("mixed-rows", header(width, 2) + payload + second.astype(np.uint8).tobytes())
    read(mixed, decode(np.stack((row, second)).astype(np.uint8)))

    def refuse(label, contents, diagnostic):
        source = fixture(label, contents)
        target = output / (label + ".npy")
        before = sha(source)
        stdout, stderr = run(command + ["--raw", source, target, "--channels", "3"], 2)
        assert diagnostic in stdout + stderr and not target.exists() and sha(source) == before, (label, stderr)
        checks["refusals"].append(label)

    refuse("truncated-flat", flat.read_bytes()[:-1], "Truncated RGBE scanline")
    refuse("truncated-rle", header(width, 1) + payload[:-1], "Truncated RGBE scanline")
    refuse("wrong-scanline-width", header(12, 1) + bytes([2, 2, 0, 13]), "scanline width does not match")
    refuse("zero-run", header(12, 1) + bytes([2, 2, 0, 12, 0]), "packet extent")
    refuse("overrun", header(12, 1) + bytes([2, 2, 0, 12, 141, 200]), "packet extent")
    refuse("trailing", flat.read_bytes() + b"\x00", "trailing RGBE data")
    refuse("legacy-repeat", header(3, 1) + bytes([128, 64, 32, 129, 1, 1, 1, 2, 0, 0, 0, 0]), "Legacy RGBE")
    refuse("unsupported-order", flat.read_bytes().replace(b"-Y", b"+Y"), "top-left scanline order")
    refuse("unsupported-xyze", flat.read_bytes().replace(b"_rgbe", b"_xyze"), "Unsupported or duplicate RGBE format")
    refuse("duplicate-format", header(1, 1, b"FORMAT=32-bit_rle_rgbe\n"), "duplicate RGBE format")
    refuse("unknown-signature", flat.read_bytes().replace(b"#?RADIANCE", b"#?UNKNOWN"), "Unrecognized RGBE signature")
    refuse("unbounded-header", header(1, 1, b"#" + b"X" * 65536 + b"\n"), "oversized RGBE header line")
    refuse("dimension-limit", header(32768, 1), "RGBE dimension must be between")
    refuse("budget", header(32767, 32767), "Decoded image exceeds reader work budget")
    for key in (b"EXPOSURE=2", b"COLORCORR=1 2 3", b"PRIMARIES=.64", b"PIXASPECT=2"):
        refuse(key.split(b"=")[0].decode().lower(), header(1, 1, key + b"\n"), "separate interpretation profile")

    for mode in ("create", "reopen"):
        stdout, _ = run([args.probe.resolve(), mode, manifest, output / "flat.qpxproj", flat])
        checks["project"].append(stdout.strip())

    assert (sha(module), sha(manifest)) == (module_hash, manifest_hash)
    (output / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": module_hash, "manifest_sha256": manifest_hash,
        "baseline_sha256": sha(baseline), **checks,
    }, indent=2) + "\n", encoding="utf-8")
    print({key: len(cases) for key, cases in checks.items()})


if __name__ == "__main__":
    main()
