"""Verify the optional OpenEXR reader with upstream-authored scanline images."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

import numpy as np
import OpenEXR


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def display(raw):
    linear = np.maximum(raw[..., :3].astype(np.float64), 0)
    opacity = raw[..., 3:].astype(np.float64) if raw.shape[2] == 4 else np.ones((*raw.shape[:2], 1))
    linear = np.divide(linear, opacity, out=np.zeros_like(linear), where=opacity > 0)
    mapped = linear / (1 + linear.max(axis=2, keepdims=True))
    encoded = np.where(mapped <= 0.0031308, 12.92 * mapped, 1.055 * mapped ** (1 / 2.4) - 0.055)
    return np.concatenate((np.floor(encoded * 255 + 0.5).astype(np.uint8),
                           np.floor(opacity * 255 + 0.5).astype(np.uint8)), axis=2)


def layout(data):
    p, entries = 8, {}
    while data[p]:
        name_end = data.index(0, p)
        name = data[p:name_end].decode()
        size_at = data.index(0, name_end + 1) + 1
        start = size_at + 4
        entries[name] = start
        p = start + struct.unpack_from("<I", data, size_at)[0]
    return entries, p + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("region", "manifest", "probe", "baseline", "alpha-baseline", "oracle-wheel", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir()
    manifest = args.manifest.resolve()
    entry, = json.loads(manifest.read_bytes())["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    module_hash, manifest_hash = sha(module), sha(manifest)
    assert entry["provider_id"] == "org.qupathxx.openexr" and entry["module"]["sha256"] == module_hash
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", entry["provider_id"]]
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {kind: [] for kind in ("raw", "display", "refusals", "project")}

    def run(cmd, code=0):
        result = subprocess.run(cmd, cwd=output, env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == code, (cmd, result.returncode, result.stdout, result.stderr)
        return result.stdout, result.stderr

    def read(source, expected, bounds=None):
        height, width, channels = expected.shape
        left, top = bounds or (0, 0)
        label = source.stem + ("-crop" if bounds else "")
        target = output / (label + ".npy")
        stdout, _ = run(command + ["--raw", source, target, "--channels", str(channels), "--x", str(left), "--y", str(top),
                                   "--width", str(width), "--height", str(height)])
        report = json.loads(stdout)
        actual = np.load(target, allow_pickle=False)
        assert actual.shape == expected.shape and actual.dtype == np.dtype("<f4")
        assert actual.tobytes() == expected.tobytes() and report["valid_pixels"] == width * height, label
        snapshot = report["reader_snapshot"]
        assert snapshot["implementation"]["module_sha256"] == module_hash and snapshot["implementation"]["options"] == {}
        assert snapshot["observed"]["scope"] == "single_file"
        assert snapshot["metadata"]["vendor"] == ("OpenEXR (linear associated RGBA)" if channels == 4 else "OpenEXR (linear)")
        assert snapshot["raw"]["channel_names"] == ["Red", "Green", "Blue", "Alpha"][:channels]
        checks["raw"].append(label)
        pam = output / (label + ".pam")
        run(command + [source, str(left), str(top), "0", str(width), str(height), pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == display(expected).tobytes(), label
        checks["display"].append(label)

    def write(name, channels, compression=OpenEXR.ZIP_COMPRESSION, **attrs):
        path = output / (name + ".exr")
        header = {"type": OpenEXR.scanlineimage, "compression": compression, **attrs}
        writer_channels = {key: np.ascontiguousarray(value) for key, value in channels.items()}
        with OpenEXR.File(header, writer_channels) as file:
            file.write(str(path))
        return path

    assert OpenEXR.__version__ == "3.5.4"
    assert sha(args.oracle_wheel) == "d55fe6b60e923917bc5104808df2d515a82b0fb053b0e038788626f0fd2b780f"
    baseline = args.baseline.resolve()
    red = np.exp2(np.arange(-40, 40, dtype=np.float32).reshape(20, 4) / np.float32(4))
    rgb = np.stack((red, -red / 2, red / 4), axis=-1)
    with OpenEXR.File(str(baseline)) as file:
        assert file.header()["compression"] == OpenEXR.ZIP_COMPRESSION
        assert file.channels()["RGB"].pixels.tobytes() == rgb.tobytes()
    data = baseline.read_bytes()
    attrs, table = layout(data)
    chunk = struct.unpack_from("<Q", data, table)[0]
    assert struct.unpack_from("<I", data, chunk + 4)[0] < 16 * 4 * 12
    read(baseline, rgb)
    read(baseline, rgb[14:18, 1:3], (1, 14))
    read(write("descending", {"RGB": rgb}, lineOrder=OpenEXR.DECREASING_Y), rgb)

    float_values = np.array([0, 0x80000000, 1, 0x80000001, 0x7F7FFFFF, 0xFF7FFFFF], dtype="<u4").view("<f4")
    float_values = float_values.reshape(2, 3)
    half = np.array([0x8000, 1, 0x8001, 0x7BFF, 0xFBFF, 0x3C00], dtype="<u2").view("<f2").reshape(2, 3)
    channels = {"R": float_values, "G": half, "B": np.flip(half, axis=1).copy()}
    mixed = np.stack(tuple(channels.values()), axis=-1).astype("<f4")
    read(write("mixed-none", channels, OpenEXR.NO_COMPRESSION), mixed)
    read(write("mixed-zips", channels, OpenEXR.ZIPS_COMPRESSION), mixed)
    repeated = np.broadcast_to(np.array([0.25, -0.5, 2.0], dtype="<f4"), (3, 160, 3)).copy()
    zips = write("compressed-zips", {"RGB": repeated}, OpenEXR.ZIPS_COMPRESSION)
    zips_bytes = zips.read_bytes()
    _, zips_table = layout(zips_bytes)
    for y in range(repeated.shape[0]):
        block = struct.unpack_from("<Q", zips_bytes, zips_table + 8 * y)[0]
        assert struct.unpack_from("<I", zips_bytes, block + 4)[0] < repeated.shape[1] * 12
    read(zips, repeated)
    single = np.array([[[0.25, -0.25, 0.5]]], dtype="<f4")
    verbatim = write("verbatim-zip", {"RGB": single})
    verbatim_body = verbatim.read_bytes()
    _, verbatim_table = layout(verbatim_body)
    verbatim_chunk = struct.unpack_from("<Q", verbatim_body, verbatim_table)[0]
    assert struct.unpack_from("<I", verbatim_body, verbatim_chunk + 4)[0] == 12
    read(verbatim, single)

    alpha_baseline = args.alpha_baseline.resolve()
    rgba = np.array([[[1, .25, .5, 1], [.5, .125, .25, .5], [0, 0, 0, 0]],
                     [[.25, .125, 0, .25], [4, 2, .5, 0], [-.125, .25, 0, .5]]], dtype="<f4")
    with OpenEXR.File(str(alpha_baseline)) as file:
        assert file.channels()["RGBA"].pixels.tobytes() == rgba.tobytes()
    read(alpha_baseline, rgba)
    read(alpha_baseline, rgba[:, 1:2], (1, 0))
    # Signed zero in HALF alpha also preserves nonzero source color and renders transparent black.
    half_alpha = np.array([[1, .5, 0], [.25, -0.0, 2 ** -24]], dtype="<f2")
    mixed_alpha = {"R": rgba[..., 0], "G": rgba[..., 1].astype("<f2"), "B": rgba[..., 2], "A": half_alpha}
    mixed_source = write("mixed-rgba-zips", mixed_alpha, OpenEXR.ZIPS_COMPRESSION)
    mixed_expected = np.stack(tuple(mixed_alpha.values()), axis=-1).astype("<f4")
    with OpenEXR.File(str(mixed_source), separate_channels=True) as file:
        observed = np.stack([file.channels()[name].pixels for name in "RGBA"], axis=-1).astype("<f4")
        assert observed.tobytes() == mixed_expected.tobytes()
    read(mixed_source, mixed_expected)

    def refuse(label, source, diagnostic):
        if isinstance(source, (bytes, bytearray)):
            path = output / (label + ".exr")
            path.write_bytes(source)
            source = path
        target = output / (label + ".npy")
        before = sha(source)
        stdout, stderr = run(command + ["--raw", source, target, "--channels", "3"], 2)
        assert diagnostic in stdout + stderr and not target.exists() and sha(source) == before, (label, stderr)
        checks["refusals"].append(label)

    def changed(offset, value):
        return data[:offset] + value + data[offset + len(value):]

    refuse("wrong-signature", changed(0, b"abcd"), "Unrecognized OpenEXR signature")
    refuse("multipart-flag", changed(4, struct.pack("<I", 0x1002)), "single-part scanline")
    refuse("other-compression", changed(attrs["compression"], bytes([4])), "NONE, ZIPS or ZIP")
    refuse("different-window", changed(attrs["displayWindow"] + 8, struct.pack("<I", 4)), "identical zero-origin")
    shifted = bytearray(data)
    for at in (attrs["dataWindow"], attrs["displayWindow"]):
        struct.pack_into("<4i", shifted, at, -2, 3, 1, 22)
    refuse("shifted-window", shifted, "identical zero-origin")
    refuse("non-square", changed(attrs["pixelAspectRatio"], struct.pack("<f", 2)), "square pixels")
    refuse("subsampled", changed(attrs["channels"] + 10, struct.pack("<I", 2)), "unsampled HALF or FLOAT")
    refuse("bad-offset", changed(table, struct.pack("<Q", chunk + 1)), "scanline offsets")
    refuse("block-origin", changed(chunk, struct.pack("<I", 1)), "wrong scanline")
    refuse("block-size", changed(chunk + 4, struct.pack("<I", 0)), "scanline block size")
    refuse("bad-zlib", changed(chunk + 8, b"\0"), "ZIP block")
    refuse("truncated", data[:-1], "Truncated OpenEXR data")
    refuse("trailing", data + b"\0", "trailing OpenEXR data")
    invalid = rgb.copy()
    invalid[0, 0, 0] = np.float32(np.inf)
    refuse("nonfinite", write("nonfinite-image", {"RGB": invalid}), "Non-finite OpenEXR sample")
    refuse("unknown-channel", write("rgb-depth", {"RGB": rgb, "Z": red}), "Unexpected OpenEXR channel data")
    for tag, invalid_alpha in (("negative", -0.125), ("above-one", 1.125)):
        modified = rgba.copy()
        modified[0, 0, 3] = invalid_alpha
        refuse("alpha-" + tag, write("rgba-" + tag, {"RGBA": modified}), "alpha must be in [0, 1]")

    for profile, source in (("rgb", baseline), ("rgba", alpha_baseline)):
        for mode in ("create", "reopen"):
            stdout, _ = run([args.probe.resolve(), mode, manifest, output / (profile + ".qpxproj"), source, profile])
            checks["project"].append(stdout.strip())

    assert (sha(module), sha(manifest)) == (module_hash, manifest_hash)
    (output / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": module_hash, "manifest_sha256": manifest_hash,
        "baseline_sha256": sha(baseline), "alpha_baseline_sha256": sha(alpha_baseline),
        "oracle_wheel_sha256": sha(args.oracle_wheel), **checks,
    }, indent=2) + "\n", encoding="utf-8")
    print({key: len(cases) for key, cases in checks.items()})


if __name__ == "__main__":
    main()
