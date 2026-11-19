"""Verify exact indices and rendered colors through the installed GIF plugin."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

import numpy as np
from PIL import Image, __version__ as pillow_version

COLORS = bytes([30, 50, 70, 255, 0, 10, 0, 255, 0, 4, 5, 255])


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def subblocks(data):
    return b"".join(bytes([len(data[i:i+255])]) + data[i:i+255] for i in range(0, len(data), 255)) + b"\0"


def packed_codes(codes):
    # Minimum code size 2. Carry width across dictionary growth and clear codes.
    result = bytearray()
    width, pending, used, next_code = 3, 0, 0, 6
    previous = False
    for code in codes:
        assert 0 <= code < 1 << width
        pending |= code << used
        used += width
        while used >= 8:
            result.append(pending & 255)
            pending >>= 8
            used -= 8
        if code == 4:
            width, next_code, previous = 3, 6, False
        elif code != 5:
            if previous and next_code < 4096:
                next_code += 1
                if next_code == 1 << width and width < 12:
                    width += 1
            previous = True
    if used:
        result.append(pending)
    return bytes(result)


def frame(width, height, compressed, *, version=b"GIF89a", local=False, interlace=False,
          transparency=None, aspect=0, prefix=b"", trailer=b";", left=0, minimum=2):
    flags = 0 if local else 0x81
    data = version + struct.pack("<HHBBB", width, height, flags, 0, aspect)
    if not local:
        data += COLORS
    data += prefix
    if transparency is not None:
        data += b"!\xf9\x04\x01\0\0" + bytes([transparency]) + b"\0"
    data += b"," + struct.pack("<HHHHB", left, 0, width, height, (0x81 if local else 0) | (64 if interlace else 0))
    if local:
        data += COLORS
    return data + bytes([minimum]) + subblocks(compressed) + trailer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("region", "probe", "manifest", "baseline", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir()
    manifest = args.manifest.resolve()
    entry, = json.loads(manifest.read_bytes())["plugins"]
    module = manifest.with_name(entry["module"]["path"])
    module_hash, manifest_hash = digest(module), digest(manifest)
    assert entry["provider_id"] == "org.qupathxx.gif" and entry["module"]["sha256"] == module_hash
    command = [args.region.resolve(), "--reader-plugins", manifest, "--reader", entry["provider_id"]]
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    rows = {"raw": [], "display": [], "refusals": [], "project": []}

    def run(cmd, code=0):
        result = subprocess.run(cmd, cwd=root, env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == code, (cmd, result.returncode, result.stdout, result.stderr)
        return result.stdout, result.stderr

    def verify(source, expected):
        height, width = expected.shape
        with Image.open(source) as oracle:
            assert oracle.n_frames == 1 and np.array_equal(np.asarray(oracle), expected)
            rgba = oracle.convert("RGBA").tobytes()
        target = root / (source.stem + ".npy")
        stdout, _ = run(command + ["--raw", source, target])
        report = json.loads(stdout)
        values = np.load(target, allow_pickle=False)
        assert values.shape == (height, width, 1) and values.dtype == np.uint8
        assert values.tobytes() == expected.tobytes() and report["valid_pixels"] == expected.size
        snapshot = report["reader_snapshot"]
        raw_meta = snapshot["raw"]
        assert raw_meta["channel_names"] == ["Palette index"]
        assert raw_meta["pixel_width_um"] is None and raw_meta["pixel_height_um"] is None
        assert snapshot["observed"]["scope"] == "single_file"
        assert snapshot["implementation"]["module_sha256"] == module_hash
        rows["raw"].append(source.stem)
        pam = root / (source.stem + ".pam")
        run(command + [source, "0", "0", "0", str(width), str(height), pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == rgba
        rows["display"].append(source.stem)

    def author(name, data):
        path = root / (name + ".gif")
        path.write_bytes(data)
        return path

    def refuse(name, data, message):
        source = author(name, data)
        target = root / (name + ".npy")
        stdout, stderr = run(command + ["--raw", source, target], 2)
        assert message in stdout + stderr and not target.exists() and source.read_bytes() == data, (name, stdout, stderr)
        rows["refusals"].append(name)

    baseline = args.baseline.resolve()
    verify(baseline, np.array([[0, 1, 2, 3], [3, 2, 1, 0]], dtype=np.uint8))
    for height, width, interlace, transparent in (
        (4, 6, False, False), (19, 17, True, False), (193, 257, True, True), (1, 1, False, True),
    ):
        rng = np.random.default_rng(height + width)
        expected = rng.integers(0, 256, (height, width), dtype=np.uint8)
        palette = rng.integers(0, 256, 768, dtype=np.uint8).tobytes()
        source = root / f"pillow-{height}x{width}.gif"
        image = Image.fromarray(expected)
        image.putpalette(palette)
        options = {"transparency": 10} if transparent else {}
        image.save(source, optimize=False, interlace=interlace, **options)
        data = source.read_bytes()
        descriptor = data.index(b",", 13 + 768)
        assert bool(data[descriptor + 9] & 64) == interlace
        verify(source, expected)

    for height in (1, 2, 3, 5, 9):
        expected = np.arange(height * 3, dtype=np.uint8).reshape(height, 3) % 4
        order = [y for start, step in ((0, 8), (4, 8), (2, 4), (1, 2)) for y in range(start, height, step)]
        codes = [v for value in expected[order].ravel() for v in (4, int(value))] + [5]
        source = author(f"local-interlace-{height}",
                        frame(3, height, packed_codes(codes), local=True, interlace=True, transparency=1,
                              prefix=b"!\xfe" + subblocks(b"authored comment")))
        verify(source, expected)
    repeated = np.ones((1, 3), dtype=np.uint8)
    verify(author("next-code", frame(3, 1, packed_codes([4, 1, 6, 5]), version=b"GIF87a", aspect=49)), repeated)
    saturation = np.arange(4160, dtype=np.uint16).astype(np.uint8).reshape(40, 104) % 4
    codes = [4, *map(int, saturation.ravel()), 5]
    verify(author("deferred-clear", frame(104, 40, packed_codes(codes))), saturation)

    valid_data = frame(3, 1, packed_codes([4, 1, 6, 5]))
    for name, data, message in (
        ("signature", b"GIF90a" + valid_data[6:], "Invalid GIF signature"),
        ("truncated-screen", valid_data[:8], "input is truncated"),
        ("no-trailer", valid_data[:-1], "input is truncated"),
        ("extra-after-trailer", valid_data + b"x", "final trailer"),
        ("partial-canvas", frame(3, 1, packed_codes([4, 1, 6, 5]), left=1), "entire logical screen"),
        ("nonsquare", frame(3, 1, packed_codes([4, 1, 6, 5]), aspect=1), "pixel aspect ratio"),
        ("invalid-transparency", frame(3, 1, packed_codes([4, 1, 6, 5]), transparency=4), "transparency index"),
        ("missing-table", valid_data[:10] + b"\0" + valid_data[11:13] + valid_data[25:], "valid color table"),
        ("min-code", frame(3, 1, packed_codes([4, 1, 6, 5]), minimum=1), "minimum code size"),
        ("no-initial-clear", frame(3, 1, packed_codes([1, 5])), "initial clear"),
        ("undefined-code", frame(3, 1, packed_codes([4, 1, 7, 5])), "Invalid GIF LZW code"),
        ("early-terminator", frame(3, 1, b""), "before the end code"),
        ("few-samples", frame(3, 1, packed_codes([4, 1, 5])), "too few indices"),
        ("many-samples", frame(3, 1, packed_codes([4, 1, 6, 2, 5])), "too many indices"),
        ("after-eoi", frame(3, 1, packed_codes([4, 1, 6, 5]) + b"\0"), "after the end code"),
        ("second-frame", valid_data[:-1] + valid_data[25:], "exactly one image"),
        ("animation-extension", frame(3, 1, packed_codes([4, 1, 6, 5]), prefix=b"!\xff"), "application and plain text"),
        ("timed-control", frame(3, 1, packed_codes([4, 1, 6, 5]), prefix=b"!\xf9\x04\0\x01\0\0\0"), "untimed image"),
    ):
        refuse(name, data, message)

    project = root / "study.qpxproj"
    for mode in ("create", "reopen"):
        stdout, _ = run([args.probe.resolve(), mode, manifest, project, baseline])
        rows["project"].append(stdout.strip())
    assert (digest(module), digest(manifest)) == (module_hash, manifest_hash)
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": module_hash, "manifest_sha256": manifest_hash,
        "baseline_sha256": digest(baseline), "pillow": pillow_version, **rows,
    }, indent=2) + "\n", encoding="utf-8")
    print({kind: len(cases) for kind, cases in rows.items()})


if __name__ == "__main__":
    main()
