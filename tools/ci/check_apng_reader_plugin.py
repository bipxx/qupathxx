"""Check native APNG frames using a retained writer image and authored streams."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

import numpy as np
from PIL import Image


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def chunk(name, payload=b""):
    return struct.pack(">I", len(payload)) + name + payload + struct.pack(">I", zlib.crc32(name + payload))


def raw_scanlines(canvas):
    return zlib.compress(b"".join(b"\0" + line.tobytes() for line in canvas))


def animation(canvas, frames, separate=False):
    height, width, channels = canvas.shape
    header = struct.pack(">IIBBBBB", width, height, 8, 6 if channels == 4 else 2, 0, 0, 0)
    records = [(b"IHDR", header), (b"acTL", struct.pack(">II", len(frames), 2)), (b"sRGB", b"\x00")]
    sequence = 0

    def append(kind, encoded):
        nonlocal sequence
        for payload in (encoded[:4], encoded[4:]):
            if kind == b"fdAT":
                payload = struct.pack(">I", sequence) + payload
                sequence += 1
            records.append((kind, payload))

    if separate:
        append(b"IDAT", raw_scanlines(canvas))
    for index, (pixels, x, y, dispose, blend) in enumerate(frames):
        h, w = pixels.shape[:2]
        records.append((b"fcTL", struct.pack(">IIIIIHHBB", sequence, w, h, x, y, index + 1, 11, dispose, blend)))
        sequence += 1
        append(b"IDAT" if not separate and index == 0 else b"fdAT", raw_scanlines(pixels))
    return records + [(b"IEND", b"")]


def encode(records):
    return b"\x89PNG\r\n\x1a\n" + b"".join(chunk(name, data) for name, data in records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("region", "manifest", "probe", "baseline", "output"):
        parser.add_argument("--" + option, type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir()
    manifest = args.manifest.resolve()
    catalog = json.loads(manifest.read_bytes())
    entry, = catalog["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    identity = (sha(module), sha(manifest))
    assert entry["module"]["sha256"] == identity[0] and entry["provider_id"] == "org.qupathxx.apng"
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", entry["provider_id"]]
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {key: [] for key in ("raw", "display", "refusals", "project")}

    def run(cmd, code=0):
        result = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == code, (cmd, result.stdout, result.stderr)
        return result.stdout, result.stderr

    def read(source, frame, pixels):
        h, w, _ = pixels.shape
        label = source.stem + f"-{frame}"
        cli = command + ["--reader-options", json.dumps({"frame": frame})]
        raw_path, pam_path = out / (label + ".npy"), out / (label + ".pam")
        stdout, _ = run(cli + ["--raw", source, raw_path, "--channels", "4"])
        raw = np.load(raw_path, allow_pickle=False)
        report = json.loads(stdout)
        snapshot = report["reader_snapshot"]
        assert raw.shape == pixels.shape and raw.dtype == np.uint8 and raw.tobytes() == pixels.tobytes(), label
        assert report["valid_pixels"] == w * h and snapshot["metadata"]["vendor"] == "APNG (rendered frame)"
        assert snapshot["implementation"]["options"] == {"frame": frame} and snapshot["raw"]["timepoints"] == 1
        checks["raw"].append(label)
        run(cli + [source, "0", "0", "0", str(w), str(h), pam_path])
        assert pam_path.read_bytes().split(b"ENDHDR\n", 1)[1] == pixels.tobytes(), label
        checks["display"].append(label)

    baseline = args.baseline.resolve()
    assert sha(baseline) == "2d1e02642a40a090d58353a1c313a29e77aae83259373d712cedbd85e989a41a"
    with Image.open(baseline) as writer_image:
        assert writer_image.n_frames == 2
        for index, expected in enumerate(((0, 255, 0, 255), (255, 0, 0, 255))):
            writer_image.seek(index)
            oracle = np.asarray(writer_image.convert("RGBA"))
            assert (oracle == expected).all()
            read(baseline, index, oracle)

    def solid(color, shape=(1, 1)):
        return np.tile(np.array(color, dtype=np.uint8), (*shape, 1))

    canvas = solid([255, 0, 0, 255], (2, 4))
    records = animation(canvas, [(canvas, 0, 0, 0, 0), (solid([0, 0, 255, 128]), 1, 0, 2, 1),
                        (solid([0, 255, 0, 255]), 2, 0, 1, 0), (solid([9, 14, 33, 0]), 3, 1, 0, 0)])
    author = out / "operations.png"
    author.write_bytes(encode(records))
    over = canvas.copy()
    over[0, 1] = [187, 0, 188, 255]
    restored = canvas.copy()
    restored[0, 2] = [0, 255, 0, 255]
    cleared = canvas.copy()
    cleared[0, 2] = 0
    cleared[1, 3] = [9, 14, 33, 0]
    for index, pixels in enumerate((canvas, over, restored, cleared)):
        read(author, index, pixels)

    separate = out / "separate-rgb.png"
    separate.write_bytes(encode(animation(canvas[..., :3], [(solid([4, 17, 249]), 3, 1, 2, 0),
        (solid([12, 34, 56]), 0, 1, 0, 0)], separate=True)))
    expected = np.zeros_like(canvas)
    expected[1, 3] = [4, 17, 249, 255]
    read(separate, 0, expected)
    expected[1, 3] = 0
    expected[1, 0] = [12, 34, 56, 255]
    read(separate, 1, expected)

    def refuse(label, data, diagnostic, options='{"frame":0}'):
        source = out / (label + ".png")
        source.write_bytes(data)
        target = out / (label + ".npy")
        cmd = command + (["--reader-options", options] if options else [])
        stdout, stderr = run(cmd + ["--raw", source, target], 2)
        assert diagnostic in stdout + stderr and not target.exists() and source.read_bytes() == data, (label, stdout, stderr)
        checks["refusals"].append(label)

    data = author.read_bytes()
    refuse("missing-index", data, "explicit nonnegative integer", None)
    refuse("negative-index", data, "outside the supported range", '{"frame":-1}')
    refuse("absent-index", data, "outside the animation", '{"frame":4}')
    refuse("truncated", data[:-1], "Truncated APNG chunk")
    refuse("trailing", data + b"\x00", "declared frame count")
    crc = bytearray(data)
    crc[-1] ^= 1
    refuse("bad-crc", crc, "chunk CRC")
    fdat = next(i for i, (kind, _) in enumerate(records) if kind == b"fdAT")
    kind, payload = records[fdat]
    refuse("bad-sequence", encode(records[:fdat] + [(kind, b"\0" * 4 + payload[4:])] + records[fdat + 1:]),
           "image data sequence")
    damaged = records.copy()
    damaged[-2] = (b"fdAT", damaged[-2][1][:4] + b"bad compressed data")
    refuse("damaged-later-frame", encode(damaged), "damaged APNG frame")
    padded = records.copy()
    padded[-2] = (b"fdAT", padded[-2][1] + zlib.compress(b""))
    refuse("extra-zlib-stream", encode(padded), "damaged APNG frame")
    first = next(i for i, (kind, _) in enumerate(records) if kind == b"fcTL")
    ctrl = bytearray(records[first][1])
    ctrl[24] = 4
    refuse("bad-disposal", encode(records[:first] + [(b"fcTL", ctrl)] + records[first + 1:]), "render operation")
    refuse("other-color-profile", encode(records[:2] + [(b"gAMA", struct.pack(">I", 30000))] + records[2:]),
           "outside the RGB8 animation profile")

    config_dir = out / "catalog"
    config_dir.mkdir()
    shutil.copyfile(module, config_dir / module.name)
    for mode, index in (("create", 1), ("reopen", 0)):
        entry["options"] = {"frame": index}
        chosen = config_dir / (mode + ".json")
        chosen.write_text(json.dumps(catalog), encoding="utf-8")
        stdout, _ = run([args.probe.resolve(), mode, chosen, out / "selected.qpxproj", baseline])
        checks["project"].append(stdout.strip())

    assert identity == (sha(module), sha(manifest))
    (out / "validation.json").write_text(json.dumps({"passed": True, "module_sha256": identity[0],
        "manifest_sha256": identity[1], "baseline_sha256": sha(baseline), **checks}, indent=2) + "\n", encoding="utf-8")
    print({key: len(value) for key, value in checks.items()})


if __name__ == "__main__":
    main()
