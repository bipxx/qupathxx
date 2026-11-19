# SPDX-License-Identifier: AGPL-3.0-only
"""Check the native N5 CLI against the pinned TensorStore producer fixture."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

from check_ngff05 import npy, sha, verify_inventory


PROVIDER = "org.qupathxx.n5"
CONTRACT = "n5-tensorstore-uint16/1"
WIDTH, HEIGHT, Z_PLANES, CHANNELS, TIMES = 11, 7, 3, 2, 2
BLOCK = (4, 3, 2, 1, 1)
AXES = list("xyzct")
CODECS = {"raw": {"type": "raw"},
          "gzip": {"type": "gzip", "level": 5, "useZlib": False},
          "zlib": {"type": "gzip", "level": 5, "useZlib": True}}
IMAGE = {"width": WIDTH, "height": HEIGHT,
         "levels": [{"width": WIDTH, "height": HEIGHT, "downsample": 1}],
         "pixel_width_um": 0.25, "pixel_height_um": 0.5}
RAW = {**IMAGE, "channels": CHANNELS, "sample_type": "uint16",
       "z_planes": Z_PLANES, "timepoints": TIMES, "series": 0, "series_count": 1,
       "dimension_order": "XYZCT", "z_spacing_um": 1.5, "time_interval_seconds": 2.5}
MISSING = "0/0/0/0/0"
CORNER = "2/2/1/1/1"
PRESENT = "1/0/0/0/0"
HEADER = struct.Struct(">HH5I")


def read_fixture(fixture, receipt):
    assert receipt["versions"] == {"tensorstore": "0.1.86", "numpy": "2.5.5"}
    assert set(receipt["datasets"]) == set(CODECS)
    stores = {}
    for codec, compression in CODECS.items():
        data = receipt["datasets"][codec]
        assert data["path"] == f"{codec}/cells"
        store = fixture / data["path"]
        members = data["members"]
        assert len(members) == 72 and MISSING not in members and CORNER in members
        verify_inventory(store, members)
        assert json.loads((store / "attributes.json").read_bytes()) == data["attributes"] == {
            "axes": AXES, "blockSize": list(BLOCK), "compression": compression, "dataType": "uint16",
            "dimensions": [WIDTH, HEIGHT, Z_PLANES, CHANNELS, TIMES],
            "resolution": [250, 500, 1500, 1, 2.5], "units": ["nm", "nm", "nm", "", "s"],
        }
        # This producer writes a full block even when the dataset ends inside it.
        assert HEADER.unpack_from((store / CORNER).read_bytes()) == (0, 5, *BLOCK)
        assert tuple(data["corner_block_shape"]) == BLOCK
        stores[codec] = store
    values = npy(fixture / "expected-tczyx.npy", (TIMES, CHANNELS, Z_PLANES, HEIGHT, WIDTH))
    assert hashlib.sha256(struct.pack(f"<{len(values)}H", *values)).hexdigest() == receipt["raw_le_u16_sha256"]
    return stores, values


def crop(values, t, c, z, x, y, width, height, channels):
    return tuple(values[(((t * CHANNELS + cc) * Z_PLANES + z) * HEIGHT + yy) * WIDTH + xx]
                 for yy in range(y, y + height) for xx in range(x, x + width)
                 for cc in range(c, c + channels))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("region", "manifest", "fixture", "output"):
        parser.add_argument(f"--{option}", type=Path, required=True)
    args = parser.parse_args()
    fixture = args.fixture.resolve(strict=True)
    region = args.region.resolve(strict=True)
    manifest = args.manifest.resolve(strict=True)
    output = args.output.resolve()
    if output.is_relative_to(fixture):
        parser.error("Output must be separate from the original fixture")
    output.mkdir(parents=True)
    receipt_path = fixture / "producer.json"
    receipt = json.loads(receipt_path.read_bytes())
    stores, values = read_fixture(fixture, receipt)
    entry, = (item for item in json.loads(manifest.read_bytes())["plugins"] if item["provider_id"] == PROVIDER)
    module = manifest.parent / entry["module"]["path"]
    assert module.is_file() and sha(module) == entry["module"]["sha256"]

    prefix = [region, "--reader-plugins", manifest]
    reader = ["--reader", PROVIDER]
    commands = []
    identities = {}

    def run(name, command, status=0):
        argv = [str(value) for value in command]
        result = subprocess.run(argv, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        commands.append({"name": name, "argv": argv, "exit_code": result.returncode,
                         "stdout": result.stdout, "stderr": result.stderr})
        assert result.returncode == status, (name, status, result.returncode, result.stdout, result.stderr)
        if status:
            assert result.stderr.strip(), name
        return result.stdout

    def raw(name, source, *, t=0, c=0, z=0, level=0, x=0, y=0, width=1, height=1, channels=1, status=0):
        dest = output / f"{name}.npy"
        result = run(name, [*prefix, *reader, "--raw", source, dest, "--level", level, "--time", t,
                            "--channel", c, "--z", z, "--x", x, "--y", y, "--width", width,
                            "--height", height, "--channels", channels], status)
        if status:
            assert not dest.exists(), name
            return
        expected = {"schema": "qpx.raw-region/2", "reader": PROVIDER, "dtype": "uint16",
                    "level": 0, "time": t, "first_channel": c, "z": z, "x": x, "y": y,
                    "width": width, "height": height, "channels": channels,
                    "valid_pixels": width * height}
        info = json.loads(result)
        assert all(info[key] == value for key, value in expected.items()), (name, info)
        snapshot = info["reader_snapshot"]
        assert snapshot["schema"] == "qpx.reader-plugin-link/2", name
        implementation = snapshot["implementation"]
        assert implementation["provider_id"] == PROVIDER and implementation["contract"] == CONTRACT
        assert implementation["module_sha256"] == entry["module"]["sha256"]
        assert implementation["capabilities"] == 11  # RGBA | RAW | SOURCE_STORE
        observed = snapshot["observed"]
        assert observed["scope"] == "directory_store"
        assert Path(observed["locator"]) == source and Path(observed["primary"]["path"]) == source / "attributes.json"
        digest = observed["directory_sha256"]
        assert len(digest) == 64 and len(bytes.fromhex(digest)) == 32
        assert identities.setdefault(source, digest) == digest
        assert all(snapshot["metadata"][key] == value for key, value in IMAGE.items()), snapshot["metadata"]
        assert all(snapshot["raw"][key] == value for key, value in RAW.items()), snapshot["raw"]
        assert npy(dest, (height, width, channels)) == crop(values, t, c, z, x, y, width, height, channels), name

    def variant(name, source):
        dest = output / "variants" / name / "cells"
        shutil.copytree(source, dest)
        return dest

    def bad_attributes(name, edit):
        dest = variant(name, stores["gzip"])
        attrs = dest / "attributes.json"
        value = json.loads(attrs.read_bytes())
        edit(value)
        attrs.write_text(json.dumps(value), encoding="utf-8")
        raw(name, dest, status=2)

    def bad_block(name, codec, edit):
        dest = variant(name, stores[codec])
        block = dest / PRESENT
        block.write_bytes(edit(block.read_bytes()))
        raw(name, dest, x=4, status=2)

    passed = False
    try:
        plain = json.loads(run("catalog-without-plugin", [region, "--readers"]))
        assert all(item["provider_id"] != PROVIDER for item in plain["readers"])
        catalog = json.loads(run("catalog-with-plugin", [*prefix, "--readers"]))
        provider, = (item for item in catalog["readers"] if item["provider_id"] == PROVIDER)
        assert provider["available"] and provider["display_rgba"] and provider["raw_samples"]
        assert provider["contract"] == CONTRACT and provider["module_sha256"] == entry["module"]["sha256"]

        for codec, source in stores.items():
            for t in range(TIMES):
                for z in range(Z_PLANES):
                    raw(f"{codec}-t{t}-z{z}", source, t=t, z=z, width=WIDTH, height=HEIGHT, channels=2)
            raw(f"{codec}-upper-edge", source, x=9, y=5, c=1, z=2, t=1, width=2, height=2)
            raw(f"{codec}-chunk-seams", source, x=3, y=2, z=1, width=6, height=3, channels=2)
            raw(f"{codec}-missing-block", source, width=4, height=3, channels=2)

            # The N5 specification also permits a smaller terminal block. Make
            # that exact block independently from the reread TensorStore values.
            name = f"{codec}-clipped-corner"
            clipped = variant(name, source)
            samples = crop(values, 1, 1, 2, 8, 6, 3, 1, 1)
            content = struct.pack(">3H", *samples)
            if codec == "gzip":
                content = gzip.compress(content, mtime=0)
            if codec == "zlib":
                content = zlib.compress(content)
            (clipped / CORNER).write_bytes(HEADER.pack(0, 5, 3, 1, 1, 1, 1) + content)
            raw(name, clipped, x=8, y=6, z=2, c=1, t=1, width=3)

        pam = output / "default-composite.pam"
        run("default-composite", [*prefix, *reader, stores["gzip"], 3, 2, 0, 4, 3, pam])
        header, encoded = pam.read_bytes().split(b"ENDHDR\n", 1)
        assert b"WIDTH 4\nHEIGHT 3\nDEPTH 4\n" in header
        rgb = tuple((value * 255 + 32767) // 65535 for value in crop(values, 0, 0, 0, 3, 2, 4, 3, 2))
        assert encoded == bytes(component for r, g in zip(rgb[::2], rgb[1::2]) for component in (r, g, 0, 255))

        for name, edit in (
            ("axes-missing", lambda a: a.pop("axes")),
            ("axes-duplicate", lambda a: a.update(axes=list("xycct"))),
            ("dtype-unadmitted", lambda a: a.update(dataType="float32")),
            ("dimension-zero", lambda a: a["dimensions"].__setitem__(0, 0)),
            ("block-budget", lambda a: a["blockSize"].__setitem__(0, 1 << 29)),
            ("codec-unadmitted", lambda a: a.update(compression={"type": "bzip2"})),
            ("axis-unit-unadmitted", lambda a: a["units"].__setitem__(0, "pixel")),
            ("axis-resolution-negative", lambda a: a["resolution"].__setitem__(2, -1500)),
            ("origin-not-representable", lambda a: a.update(origin=[1, 0, 0, 0, 0])),
        ):
            bad_attributes(name, edit)
        empty = variant("not-a-dataset", stores["raw"])
        raw("explicit-dataset-required", empty.parent, status=2)
        for name, axis in (("invalid-z", {"z": 3}), ("invalid-channel", {"c": 1, "channels": 2}),
                           ("invalid-time", {"t": 2}), ("invalid-level", {"level": 1}),
                           ("out-of-image", {"x": 11})):
            raw(name, stores["raw"], status=2, **axis)
        for name, codec, edit in (
            ("header-empty", "raw", lambda b: b""),
            ("header-truncated", "raw", lambda b: b[:HEADER.size - 1]),
            ("mode-unadmitted", "raw", lambda b: b"\x00\x01" + b[2:]),
            ("rank-differs", "raw", lambda b: b[:2] + b"\x00\x04" + b[4:]),
            ("block-shape-differs", "raw", lambda b: b[:4] + struct.pack(">I", 3) + b[8:]),
            ("raw-short", "raw", lambda b: b[:-1]),
            ("raw-trailing-byte", "raw", lambda b: b + b"\x00"),
            ("gzip-truncated", "gzip", lambda b: b[:-1]),
            ("gzip-trailing-frame", "gzip", lambda b: b + b[HEADER.size:]),
            ("zlib-truncated", "zlib", lambda b: b[:-1]),
            ("zlib-trailing-byte", "zlib", lambda b: b + b"\x00"),
        ):
            bad_block(name, codec, edit)

        protected = variant("protected-store", stores["raw"])
        inside = protected / "0" / "output.npy"
        run("output-inside-source", [*prefix, *reader, "--raw", protected, inside], 2)
        assert not inside.exists()
        verify_inventory(protected, receipt["datasets"]["raw"]["members"])
        for codec, source in stores.items():
            verify_inventory(source, receipt["datasets"][codec]["members"])
        passed = True
    finally:
        stamp = {"passed": passed, "fixture_sha256": sha(receipt_path), "region_sha256": sha(region),
                 "manifest_sha256": sha(manifest), "module_sha256": sha(module), "commands": commands}
        (output / "results.json").write_text(json.dumps(stamp, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(commands)} native N5 commands")


if __name__ == "__main__":
    main()
