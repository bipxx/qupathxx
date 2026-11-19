"""Verify an installed JPEG XL module with imagecodecs-written integer stills."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

import imagecodecs
import numpy as np
from PIL import Image


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("region", "probe", "manifest", "baseline", "fragment-baseline", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--fragment-reference", type=Path, required=True)
    parser.add_argument("--orientation-tools", type=Path, required=True)
    parser.add_argument("--orientation-baseline", type=Path, required=True)
    parser.add_argument("--xyb-baseline", type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir()
    manifest = args.manifest.resolve()
    entry, = json.loads(manifest.read_bytes())["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    module_hash, manifest_hash = sha(module), sha(manifest)
    assert entry["module"]["sha256"] == module_hash and entry["provider_id"] == "org.qupathxx.jpegxl"
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", entry["provider_id"]]
    env = os.environ.copy()
    system = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    rows = {"raw": [], "display": [], "refusals": [], "project": [], "reference": [], "orientation": []}

    def run(cmd, code=0):
        p = subprocess.run(cmd, cwd=root, env=env, capture_output=True, text=True, timeout=60)
        assert p.returncode == code, (cmd, p.returncode, p.stdout, p.stderr)
        return p.stdout, p.stderr

    def verify(source, original, bits, oracle_source=None, linear=None, decoded_srgb=False):
        if original.ndim == 2:
            original = original[..., None]
        height, width, channels = original.shape
        if not decoded_srgb:
            observed = imagecodecs.jpegxl_decode((oracle_source or source).read_bytes())
            assert observed.dtype == original.dtype and observed.tobytes() == original.tobytes()
        options = {"allow_xyb": True} if decoded_srgb else {}
        selected = command + (["--reader-options", json.dumps(options)] if options else [])
        maximum = (1 << bits) - 1
        target = root / (source.stem + ".npy")
        stdout, _ = run(selected + ["--raw", source, target, "--channels", str(channels)])
        report = json.loads(stdout)
        actual = np.load(target, allow_pickle=False)
        assert actual.shape == original.shape and actual.dtype == original.dtype
        assert actual.tobytes() == original.tobytes() and report["valid_pixels"] == height * width
        snapshot = report["reader_snapshot"]
        assert snapshot["observed"]["scope"] == "single_file"
        assert snapshot["implementation"]["module_sha256"] == module_hash
        assert snapshot["implementation"]["options"] == options
        assert snapshot["metadata"]["vendor"] == ("JPEG XL (decoded sRGB)" if decoded_srgb else "JPEG XL")
        names = {1: ["Gray"], 2: ["Gray", "Alpha"], 3: ["Red", "Green", "Blue"],
                 4: ["Red", "Green", "Blue", "Alpha"]}
        assert snapshot["raw"]["channel_names"] == names[channels]
        rows["raw"].append(source.stem)
        display = ((original.astype(np.uint32) * 255 + maximum // 2) // maximum).astype(np.uint8)
        out = np.full((height, width, 4), 255, dtype=np.uint8)
        color = original[..., :1] if channels < 3 else original[..., :3]
        if linear is None:
            linear = bits > 8  # imagecodecs uses linear sRGB for its UInt16 input.
        if linear:
            t = color.astype(np.float64) / maximum
            out[..., :3] = np.floor(255 * np.where(t <= 0.0031308, 12.92*t, 1.055*t**(1/2.4)-0.055) + .5)
        else:
            out[..., :3] = display[..., :1] if channels < 3 else display[..., :3]
        if channels % 2 == 0:
            out[..., 3] = display[..., -1]
        pam = root / (source.stem + ".pam")
        run(selected + [source, "0", "0", "0", str(width), str(height), pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == out.tobytes()
        rows["display"].append(source.stem)

    baseline = args.baseline.resolve()
    original = np.arange(24, dtype=np.uint8).reshape(2, 4, 3)
    verify(baseline, original, 8)
    for bits in (1, 8, 10, 12, 16):
        limit = (1 << bits) - 1
        for channels in (1, 2, 3, 4):
            if bits == 1 and channels > 1:
                continue
            shape = (4, 5, channels) if channels > 1 else (4, 5)
            dtype = np.uint8 if bits <= 8 else np.uint16
            samples = np.random.default_rng(bits + channels).integers(0, limit + 1, size=shape, dtype=dtype)
            samples.reshape(-1)[0] = limit
            if channels in (2, 4):
                samples[0, 0, -1] = 0
            source = root / f"jxl-{bits}bit-{channels}c.jxl"
            source.write_bytes(imagecodecs.jpegxl_encode(samples, lossless=True, usecontainer=False, bitspersample=bits))
            verify(source, samples, bits)

    encoded = baseline.read_bytes()
    wrapped = root / "jxl-boxed.jxl"
    wrapper = imagecodecs.jpegxl_encode(original, lossless=True, usecontainer=True)
    wrapped.write_bytes(wrapper)
    verify(wrapped, original, 8)
    verify(args.fragment_baseline.resolve(), original, 8)

    signature = struct.pack(">I4s4s", 12, b"JXL ", b"\r\n\x87\n")

    def file_type(minor):
        return signature + struct.pack(">I4s4sI4s", 20, b"ftyp", b"jxl ", minor, b"jxl ")

    def part(index, payload, final=False, layout="normal"):
        data = struct.pack(">I", index | (1 << 31 if final else 0)) + payload
        if layout == "wide":
            return struct.pack(">I4sQ", 1, b"jxlp", 16 + len(data)) + data
        if layout == "eof":
            return struct.pack(">I4s", 0, b"jxlp") + data
        return struct.pack(">I4s", 8 + len(data), b"jxlp") + data

    def fixture(name, data):
        source = root / (name + ".jxl")
        source.write_bytes(data)
        return source

    first, middle, last = encoded[:1], encoded[1:9], encoded[9:]
    parts = part(0, first), part(1, middle), part(2, last, True)
    verify(fixture("wide-and-eof-fragments",
                   file_type(0) + part(0, first, layout="wide") + parts[1] + part(2, last, True, "eof")),
           original, 8)
    reordered = fixture("reordered-fragments", file_type(1) + parts[2] + parts[0] + parts[1])
    verify(reordered, original, 8, oracle_source=args.fragment_baseline.resolve())
    for fixture_path in (args.fragment_baseline.resolve(), reordered):
        stdout, _ = run([args.fragment_reference.resolve(), fixture_path])
        rows["reference"].append(f"{fixture_path.name}: {stdout.strip()}")

    source_16 = root / "jxl-16bit-4c.jxl"
    wrapped_16 = source_16.read_bytes()
    marker = wrapped_16.index(b"jxlc")
    before = wrapped_16[:marker - 4]
    samples_16 = wrapped_16[marker + 4:]
    midpoint = len(samples_16) // 2
    fragment_16 = fixture("fragmented-16bit-rgba", before + part(0, samples_16[:midpoint]) +
                         part(1, samples_16[midpoint:], True))
    verify(fragment_16, imagecodecs.jpegxl_decode(wrapped_16), 16)

    tools = args.orientation_tools.resolve()
    encoder, decoder = (tools / f"{name}.exe" for name in ("cjxl", "djxl"))
    assert "cjxl v0.13.0" in "".join(run([encoder, "--version"]))
    rotated_baseline = args.orientation_baseline.resolve()
    verify(rotated_baseline, np.rot90(original, -1), 8)

    orientations = (
        original,
        original[:, ::-1],
        original[::-1, ::-1],
        original[::-1],
        original.transpose(1, 0, 2),
        np.rot90(original, -1),
        original.transpose(1, 0, 2)[::-1, ::-1],
        np.rot90(original, 1),
    )
    for orientation, expected in enumerate(orientations, 1):
        label = f"orientation-{orientation}"
        input_png, output_jxl, output_png = (root / filename for filename in
                                            (label + ".png", label + ".jxl", label + "-reference.png"))
        exif = Image.Exif()
        exif[274] = orientation
        Image.fromarray(original).save(input_png, exif=exif)
        run([encoder, input_png, output_jxl, "--container=0", "--distance=0", "--num_threads=1", "--quiet"])
        assert output_jxl.read_bytes().startswith(b"\xff\x0a")
        run([decoder, output_jxl, output_png, "--quiet"])
        with Image.open(output_png) as reference:
            assert np.array_equal(np.asarray(reference), expected)
        verify(output_jxl, expected, 8)
        rows["orientation"].append({"orientation": orientation, "expected_shape": list(expected.shape),
                                    "input_sha256": sha(input_png), "codestream_sha256": sha(output_jxl)})

    alpha16 = np.random.default_rng(891).integers(0, 65536, dtype=np.uint16, size=(3, 5, 4))
    alpha16[0, 0, 3] = 0
    input_png = root / "orientation-alpha16.png"
    output_jxl = root / "orientation-alpha16.jxl"
    output_png = root / "orientation-alpha16-reference.png"
    exif_path = root / "orientation-7.tiff-data"
    exif = Image.Exif()
    exif[274] = 7
    exif_path.write_bytes(exif.tobytes()[6:])  # cjxl expects the TIFF payload.
    input_png.write_bytes(imagecodecs.png_encode(alpha16))
    run([encoder, input_png, output_jxl, "--container=0", "--distance=0", "--num_threads=1",
         "--strip_alpha=0", "-x", f"exif={exif_path}", "--quiet"])
    run([decoder, output_jxl, output_png, "--quiet"])
    expected = alpha16.transpose(1, 0, 2)[::-1, ::-1]
    assert np.array_equal(imagecodecs.png_decode(output_png.read_bytes()), expected)
    verify(output_jxl, expected, 16, linear=False)
    rows["orientation"].append({"orientation": 7, "bit_depth": 16, "alpha": True,
                                "expected_shape": list(expected.shape), "input_sha256": sha(input_png),
                                "exif_sha256": sha(exif_path), "codestream_sha256": sha(output_jxl)})

    xyb_baseline = args.xyb_baseline.resolve()
    xyb_reference = root / "default-lossy-reference.png"
    run([decoder, xyb_baseline, xyb_reference, "--quiet"])
    xyb_rgb = imagecodecs.png_decode(xyb_reference.read_bytes())
    assert xyb_rgb.dtype == np.uint8 and xyb_rgb.shape == original.shape and np.any(xyb_rgb != original)
    verify(xyb_baseline, xyb_rgb, 8, linear=False, decoded_srgb=True)
    for bits, channels in ((8, 1), (8, 4), (16, 3)):
        label = f"xyb-{bits}bit-{channels}c"
        shape = (17, 13, channels) if channels > 1 else (17, 13)
        dtype = np.uint8 if bits == 8 else np.uint16
        samples = np.random.default_rng(bits + channels + 315).integers(0, 1 << bits, shape, dtype=dtype)
        if channels == 4:
            samples[:3, :3, 3] = 0
        input_png, output_jxl, output_png = (root / filename for filename in
                                            (label + ".png", label + ".jxl", label + "-reference.png"))
        input_png.write_bytes(imagecodecs.png_encode(samples))
        run([encoder, input_png, output_jxl, "--container=0", "--distance=1", "--num_threads=1",
             "--strip_alpha=0", "--quiet"])
        run([decoder, output_jxl, output_png, "--quiet"])
        reconstructed = imagecodecs.png_decode(output_png.read_bytes())
        assert reconstructed.shape == shape and reconstructed.dtype == dtype and np.any(reconstructed != samples)
        if channels == 4:
            assert np.array_equal(reconstructed[..., 3], samples[..., 3])
        verify(output_jxl, reconstructed, bits, linear=False, decoded_srgb=True)

    for label, data, diagnostic in (
        ("bad-signature", b"\xff\xff" + encoded[2:], "Unrecognized JPEG XL"),
        ("truncated", encoded[:-2], "failed or input is truncated"),
        ("trailing", encoded + bytes(8), "has trailing bytes"),
        ("container-tail", wrapper + struct.pack(">I4s", 8, b"free"), "one complete codestream box"),
        ("box-overrun", wrapper[:12] + struct.pack(">I", 2**31) + wrapper[16:], "Invalid JPEG XL box extent"),
        ("xyb", imagecodecs.jpegxl_encode(original, lossless=False, distance=1, usecontainer=False),
         "requires explicit allow_xyb"),
        ("animation", imagecodecs.jpegxl_encode(np.stack([original, original[::-1]]), lossless=True,
                                                usecontainer=False), "Unsupported JPEG XL sample"),
        ("fragment-reorder-v0", file_type(0) + parts[2] + parts[0] + parts[1], "fragment sequence"),
        ("fragment-duplicate", file_type(1) + parts[0] + parts[0] + parts[2], "fragment sequence"),
        ("fragment-gap", file_type(1) + parts[0] + parts[2], "fragment sequence"),
        ("fragment-no-final", file_type(0) + parts[0] + parts[1] + part(2, last), "final marker"),
        ("fragment-early-final", file_type(0) + part(0, first, True) + parts[1] + parts[2], "final marker"),
        ("fragment-empty", file_type(0) + part(0, b"", True), "requires nonempty codestream fragments"),
        ("fragment-box-overrun", file_type(0) + parts[0] + parts[1] + parts[2][:-1], "Invalid JPEG XL box extent"),
        ("fragment-type-mix", file_type(0) + parts[0] + wrapper[32:], "requires nonempty codestream fragments"),
        ("fragment-extra-crossing-end", file_type(0) + parts[0] + parts[1] + part(2, last + b"!", True),
         "has trailing bytes"),
        ("fragment-profile-version", file_type(2) + b"".join(parts), "Unsupported JPEG XL container version"),
        ("fragment-count-limit", file_type(0) + b"".join(part(i, b"x") for i in range(4097)),
         "fragment limit exceeded"),
    ):
        source = root / f"{label}.jxl"
        source.write_bytes(data)
        target = root / f"{label}.npy"
        stdout, stderr = run(command + ["--raw", source, target], 2)
        assert diagnostic in stdout + stderr and not target.exists() and source.read_bytes() == data
        rows["refusals"].append(label)
    for label, options, diagnostic in (
        ("xyb-no-opt-in", [], "requires explicit allow_xyb"),
        ("xyb-opt-out", ["--reader-options", '{"allow_xyb":false}'], "requires explicit allow_xyb"),
        ("xyb-invalid-option", ["--reader-options", '{"allow_xyb":1}'], "require a boolean allow_xyb"),
    ):
        target = root / (label + ".npy")
        stdout, stderr = run(command + options + ["--raw", xyb_baseline, target], 2)
        assert diagnostic in stdout + stderr and not target.exists(), (label, stdout, stderr)
        rows["refusals"].append(label)
    for label, source in (("codestream", baseline), ("ordered", args.fragment_baseline.resolve()),
                          ("reordered", reordered), ("rotated", rotated_baseline)):
        for mode in ("create", "reopen"):
            geometry = ["rot90"] if label == "rotated" else []
            stdout, _ = run([args.probe.resolve(), mode, manifest, root / (label + ".qpxproj"), source, *geometry])
            rows["project"].append(label + ": " + stdout.strip())
    configured = json.loads(manifest.read_bytes())
    configured["plugins"][0]["options"] = {"allow_xyb": True}
    xyb_manifest = root / "reader-plugins-xyb.json"
    xyb_manifest.write_text(json.dumps(configured), encoding="ascii")
    shutil.copyfile(module, root / module.name)
    expected_xyb = root / "expected-srgb.raw"
    expected_xyb.write_bytes(xyb_rgb.tobytes())
    for mode, catalog in (("create", xyb_manifest), ("reopen", manifest)):
        stdout, _ = run([args.probe.resolve(), mode, catalog, root / "decoded.qpxproj",
                         xyb_baseline, "decoded", expected_xyb])
        rows["project"].append("decoded sRGB: " + stdout.strip())
    assert (sha(module), sha(manifest)) == (module_hash, manifest_hash)
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": module_hash, "manifest_sha256": manifest_hash,
        "baseline_sha256": sha(baseline), "fragment_baseline_sha256": sha(args.fragment_baseline),
        "orientation_baseline_sha256": sha(rotated_baseline),
        "xyb_baseline_sha256": sha(xyb_baseline), "xyb_reference_sha256": sha(expected_xyb),
        "orientation_tools_sha256": {"cjxl": sha(encoder), "djxl": sha(decoder)},
        "oracle": imagecodecs.jpegxl_version(), **rows,
    }, indent=2) + "\n", encoding="utf-8")
    print({key: len(values) for key, values in rows.items()})


if __name__ == "__main__":
    main()
