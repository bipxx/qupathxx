"""Check the native AVIF provider with authored planes and upstream writer output."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

import imagecodecs
import numpy as np
from PIL import Image


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("region", "manifest", "probe", "oracle", "baseline", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir()
    manifest = args.manifest.resolve()
    config = json.loads(manifest.read_bytes())
    entry, = config["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    module_hash, manifest_hash = sha(module), sha(manifest)
    assert entry["provider_id"] == "org.qupathxx.avif" and entry["module"]["sha256"] == module_hash
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", entry["provider_id"]]
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    rows = {"raw": [], "display": [], "refusals": [], "project": []}
    encoder, decoder = [args.oracle.resolve() / (name + ".exe") for name in ("avifenc", "avifdec")]
    yuv = {"allow_yuv": True}

    def run(cmd, code=0):
        p = subprocess.run(cmd, cwd=output, env=env, capture_output=True, text=True, timeout=45)
        assert p.returncode == code, (cmd, p.returncode, p.stdout, p.stderr)
        return p.stdout, p.stderr

    def raw(source, expected, bits, options=None):
        if expected.ndim == 2:
            expected = expected[..., None]
        height, width, channels = expected.shape
        selected = command + (["--reader-options", json.dumps(options)] if options else [])
        target = output / (source.stem + ".npy")
        stdout, _ = run(selected + ["--raw", source, target, "--channels", str(channels)])
        report = json.loads(stdout)
        actual = np.load(target, allow_pickle=False)
        assert actual.shape == expected.shape and actual.dtype == expected.dtype
        assert actual.tobytes() == expected.tobytes() and report["valid_pixels"] == width * height, source
        snapshot = report["reader_snapshot"]
        assert snapshot["implementation"]["module_sha256"] == module_hash
        assert snapshot["implementation"]["options"] == (options or {})
        assert snapshot["observed"]["scope"] == "single_file"
        assert snapshot["metadata"]["vendor"] == ("AVIF (decoded sRGB)" if options else "AVIF")
        names = {1: ["Gray"], 2: ["Gray", "Alpha"], 3: ["Red", "Green", "Blue"],
                 4: ["Red", "Green", "Blue", "Alpha"]}
        assert snapshot["raw"]["channel_names"] == names[channels]
        rows["raw"].append(source.stem)
        maximum = (1 << bits) - 1
        scaled = ((expected.astype(np.uint32) * 255 + maximum // 2) // maximum).astype(np.uint8)
        rgba = np.full((height, width, 4), 255, dtype=np.uint8)
        rgba[..., :3] = scaled[..., :1] if channels < 3 else scaled[..., :3]
        if channels % 2 == 0:
            rgba[..., 3] = scaled[..., -1]
        pam = output / (source.stem + ".pam")
        run(selected + [source, "0", "0", "0", str(width), str(height), pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba.tobytes(), source
        rows["display"].append(source.stem)

    def encode(source, name, *flags):
        target = output / (name + ".avif")
        run([encoder, "-j", "1", "-s", "8", "-c", "aom", *flags, source, target])
        return target

    def decode(source):
        png = output / (source.stem + "-oracle.png")
        run([decoder, "-j", "1", "-c", "dav1d", "-u", "bilinear", source, png])
        return imagecodecs.png_decode(png.read_bytes())

    def y4m(path, samples, bits, mono=False):
        height, width = samples.shape[:2]
        mode = "mono" if mono else "444"
        if bits > 8:
            mode += str(bits) if mono else ("p" + str(bits))
        with path.open("wb") as file:
            file.write(f"YUV4MPEG2 W{width} H{height} F1:1 Ip A1:1 C{mode} XCOLORRANGE=FULL\nFRAME\n".encode())
            if mono:
                file.write(samples.tobytes())
            else:
                for channel in (1, 2, 0):  # Identity coding stores G/B/R in Y/U/V.
                    file.write(samples[..., channel].tobytes())

    original = np.arange(60, dtype=np.uint8).reshape(4, 5, 3)
    baseline = args.baseline.resolve()
    reference = decode(baseline)
    raw(baseline, reference, 8, yuv)
    controls = {}
    for bits in (8, 10, 12):
        dtype = np.uint8 if bits == 8 else np.dtype("<u2")
        values = original if bits == 8 else np.random.default_rng(bits).integers(0, 1 << bits, original.shape, dtype=dtype)
        source = output / f"identity-{bits}.y4m"
        y4m(source, values, bits)
        target = encode(source, f"identity-{bits}", "--lossless", "--cicp", "1/13/0")
        raw(target, values, bits)
        controls[bits] = target

    source = output / "gray.y4m"
    gray = np.arange(20, dtype="<u2").reshape(4, 5) * 53
    y4m(source, gray, 10, mono=True)
    raw(encode(source, "gray", "--lossless", "--cicp", "1/13/0"), gray, 10)

    rgba = np.concatenate((original, np.arange(20, dtype=np.uint8).reshape(4, 5, 1) * 13), axis=2)
    source_png = output / "input-alpha.png"
    Image.fromarray(rgba).save(source_png)
    raw(encode(source_png, "identity-alpha", "--lossless", "--cicp", "1/13/0"), rgba, 8)

    yuv_alpha = encode(source_png, "yuv-alpha", "--cicp", "1/13/1", "-y", "420", "-q", "85")
    converted = decode(yuv_alpha)
    assert converted.shape == rgba.shape
    raw(yuv_alpha, converted, 8, yuv)

    def refuse(label, source, diagnostic, options=None):
        target = output / (label + ".npy")
        selected = command + (["--reader-options", json.dumps(options)] if options else [])
        before = sha(source)
        stdout, stderr = run(selected + ["--raw", source, target], 2)
        assert diagnostic in stdout + stderr and not target.exists() and sha(source) == before, (label, stdout, stderr)
        rows["refusals"].append(label)

    refuse("missing-opt-in", baseline, "requires explicit allow_yuv")
    refuse("explicit-opt-out", baseline, "requires explicit allow_yuv", {"allow_yuv": False})
    refuse("wrong-option", baseline, "require a boolean allow_yuv", {"allow_yuv": "true"})
    invalid = output / "truncated.avif"
    invalid.write_bytes(controls[8].read_bytes()[:-12])
    refuse("truncated", invalid, "AVIF image decode failed or the data is truncated")
    invalid = output / "not-avif.avif"
    invalid.write_bytes(bytes(12))
    refuse("not-avif", invalid, "Unrecognized AVIF file type")
    refuse("unadmitted-transfer", encode(source_png, "linear", "--cicp", "1/8/1"), "requires SDR sRGB", yuv)
    refuse("nonidentity-geometry", encode(source_png, "rotated", "--cicp", "1/13/1", "--irot", "1"),
           "geometry, Exif or premultiplied", yuv)

    sequence = output / "sequence.avif"
    Image.fromarray(original).save(sequence, save_all=True, append_images=[Image.fromarray(original[::-1])], duration=50)
    refuse("sequence", sequence, "Invalid AVIF container or image count", yuv)

    configured = output / "reader-plugins-allow-yuv.json"
    config["plugins"][0]["options"] = yuv
    configured.write_text(json.dumps(config), encoding="utf-8")
    shutil.copyfile(module, output / module.name)
    for profile, source, expected, catalog in (
        ("native", controls[8], original, manifest), ("decoded", baseline, reference, configured),
    ):
        expected_path = output / f"{profile}.rgb"
        expected_path.write_bytes(expected.tobytes())
        for mode, chosen in (("create", catalog), ("reopen", manifest)):
            stdout, _ = run([args.probe.resolve(), mode, chosen, output / (profile + ".qpxproj"),
                             source, expected_path, profile])
            rows["project"].append(profile + ": " + stdout.strip())

    assert (sha(module), sha(manifest)) == (module_hash, manifest_hash)
    (output / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": module_hash, "manifest_sha256": manifest_hash,
        "oracle_sha256": {tool.name: sha(tool) for tool in (encoder, decoder)},
        "baseline_sha256": sha(baseline), **rows,
    }, indent=2) + "\n", encoding="utf-8")
    print({key: len(cases) for key, cases in rows.items()})


if __name__ == "__main__":
    main()
