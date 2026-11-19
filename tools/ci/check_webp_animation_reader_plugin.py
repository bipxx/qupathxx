"""Verify native WebP animation frame selection with writer and authored input."""
import argparse
import hashlib
from io import BytesIO
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

import numpy as np
from PIL import Image


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def le24(value):
    return value.to_bytes(3, "little")


def chunk(kind, payload):
    return kind + struct.pack("<I", len(payload)) + payload + b"\0" * (len(payload) % 2)


def riff(*chunks):
    body = b"WEBP" + b"".join(chunks)
    return b"RIFF" + struct.pack("<I", len(body)) + body


def still(pixels, lossless):
    stream = BytesIO()
    Image.fromarray(pixels).save(stream, format="WebP", lossless=lossless, exact=True, quality=95)
    data = stream.getvalue()
    with Image.open(BytesIO(data)) as source:
        oracle = np.asarray(source.convert("RGBA"))
    payload = []
    at = 12
    while at < len(data):
        size = struct.unpack_from("<I", data, at + 4)[0]
        end = at + 8 + size + size % 2
        if data[at:at+4] in (b"VP8L", b"VP8 ", b"ALPH"):
            payload.append(data[at:end])
        at = end
    return b"".join(payload), oracle


def frame(payload, width, height, x, y, flags):
    return chunk(b"ANMF", b"".join(le24(v) for v in (x//2, y//2, width-1, height-1, 37)) + bytes([flags]) + payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("region", "manifest", "probe", "baseline", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir()
    manifest = args.manifest.resolve()
    catalog = json.loads(manifest.read_bytes())
    entry, = catalog["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    identity = (sha(module), sha(manifest))
    assert entry["module"]["sha256"] == identity[0] and entry["provider_id"] == "org.qupathxx.webp-animation"
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", entry["provider_id"]]
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {key: [] for key in ("raw", "display", "refusals", "project")}

    def run(cmd, code=0):
        result = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == code, (cmd, result.stdout, result.stderr)
        return result.stdout, result.stderr

    def read(source, index, expected):
        h, w, _ = expected.shape
        label = source.stem + f"-{index}"
        cli = command + ["--reader-options", json.dumps({"frame": index})]
        target, pam = out / (label + ".npy"), out / (label + ".pam")
        stdout, _ = run(cli + ["--raw", source, target, "--channels", "4"])
        raw = np.load(target, allow_pickle=False)
        report = json.loads(stdout)
        snapshot = report["reader_snapshot"]
        assert raw.shape == expected.shape and raw.dtype == np.uint8 and raw.tobytes() == expected.tobytes(), label
        assert report["valid_pixels"] == w * h and snapshot["metadata"]["vendor"] == "WebP (rendered animation frame)"
        assert snapshot["implementation"]["options"] == {"frame": index} and snapshot["raw"]["timepoints"] == 1
        checks["raw"].append(label)
        run(cli + [source, "0", "0", "0", str(w), str(h), pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == expected.tobytes(), label
        checks["display"].append(label)

    baseline = args.baseline.resolve()
    assert sha(baseline) == "79b7221a30f1c2096caec8de9e57c9dfc4a715e0b51dc9930cb7e6bf8ee2515d"
    with Image.open(baseline) as source:
        assert source.n_frames == 3
        for i, color in enumerate(((11, 29, 89, 255), (192, 8, 47, 255), (4, 111, 17, 255))):
            source.seek(i)
            oracle = np.asarray(source.convert("RGBA"))
            assert (oracle == color).all()
            read(baseline, i, oracle)

    def solid(shape, color):
        return np.tile(np.array(color, dtype=np.uint8), (*shape, 1))

    extended = chunk(b"VP8X", b"\x12\0\0\0" + le24(7) + le24(5))
    background = (18, 31, 44, 255)
    ani = chunk(b"ANIM", bytes((44, 31, 18, 255, 2, 0)))
    red = solid((6, 8), (255, 0, 0, 255))
    first, _ = still(red, True)
    blue = solid((2, 2), (0, 0, 255, 128))
    second, blue_oracle = still(blue, True)
    assert blue_oracle.tobytes() == blue.tobytes()
    lossy = solid((2, 2), (12, 170, 34, 64))
    third, lossy_oracle = still(lossy, False)
    assert third.startswith(b"ALPH")
    records = (extended, ani, frame(first, 8, 6, 0, 0, 2), frame(second, 2, 2, 2, 2, 1),
               frame(third, 2, 2, 6, 0, 2))
    authored = out / "operations.webp"
    authored.write_bytes(riff(*records))
    read(authored, 0, red)
    blend = red.copy()
    blend[2:4, 2:4] = [187, 0, 188, 255]
    read(authored, 1, blend)
    disposed = red.copy()
    disposed[2:4, 2:4] = background
    disposed[:2, 6:] = lossy_oracle
    read(authored, 2, disposed)

    partial = out / "initial-background.webp"
    partial.write_bytes(riff(extended, ani, frame(second, 2, 2, 2, 2, 2)))
    initial = solid((6, 8), background)
    initial[2:4, 2:4] = blue
    read(partial, 0, initial)

    def refuse(label, data, diagnostic, options='{"frame":0}'):
        source = out / (label + ".webp")
        source.write_bytes(data)
        target = out / (label + ".npy")
        cli = command + (["--reader-options", options] if options else [])
        stdout, stderr = run(cli + ["--raw", source, target], 2)
        assert diagnostic in stdout + stderr and not target.exists() and source.read_bytes() == data, (label, stderr)
        checks["refusals"].append(label)

    body = authored.read_bytes()
    refuse("missing-frame-index", body, "explicit integer frame", None)
    refuse("outside-animation", body, "index is outside", '{"frame":3}')
    refuse("truncated", body[:-1], "RIFF header or extent")
    refuse("trailing", body + b"\0", "RIFF header or extent")
    refuse("frame-outside", riff(*records[:4], frame(third, 2, 2, 8, 0, 2)), "outside the canvas")
    refuse("encoded-size-mismatch", riff(*records[:4], frame(third, 3, 2, 0, 0, 2)), "raw alpha extent")
    refuse("encoded-size-vp8l", riff(*records[:4], frame(second, 3, 2, 0, 0, 2)), "encoded frame")
    refuse("bad-flags", riff(*records[:4], frame(third, 2, 2, 6, 0, 4)), "outside the canvas")
    refuse("unexpected-inner-chunk", riff(*records[:4], frame(first + chunk(b"JUNK", b""), 8, 6, 0, 0, 2)), "payload layout")
    refuse("unexpected-top-chunk", riff(*records, chunk(b"ICCP", b"ignored")), "requires ANMF")
    opaque_header = chunk(b"VP8X", b"\x02\0\0\0" + le24(7) + le24(5))
    refuse("undeclared-alpha", riff(opaque_header, ani, frame(second, 2, 2, 0, 0, 2)), "alpha differs")

    config_dir = out / "catalog"
    config_dir.mkdir()
    shutil.copyfile(module, config_dir / module.name)
    for mode, index in (("create", 1), ("reopen", 0)):
        entry["options"] = {"frame": index}
        spec = config_dir / (mode + ".json")
        spec.write_text(json.dumps(catalog), encoding="utf-8")
        stdout, _ = run([args.probe.resolve(), mode, spec, out / "selected.qpxproj", baseline])
        checks["project"].append(stdout.strip())
    assert identity == (sha(module), sha(manifest))
    (out / "validation.json").write_text(json.dumps({"passed": True, "module_sha256": identity[0],
        "manifest_sha256": identity[1], "baseline_sha256": sha(baseline), **checks}, indent=2) + "\n", encoding="utf-8")
    print({key: len(value) for key, value in checks.items()})


if __name__ == "__main__":
    main()
