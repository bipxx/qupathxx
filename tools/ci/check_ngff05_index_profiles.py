# SPDX-License-Identifier: AGPL-3.0-only
"""Verify producer shard-index bytes, raw samples and range validation."""

import argparse
import json
from math import prod
from pathlib import Path
import shutil
import struct
import subprocess

from check_ngff05 import (ARRAY_FORMATS, PROVIDER, crc32c, crop, npy, read_oracle,
                          sha, sharding_config, verify_inventory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("region", "manifest", "fixture", "output"):
        parser.add_argument(f"--{option}", type=lambda value: Path(value).resolve(), required=True)
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True)
    receipt_path = args.fixture / "producer.json"
    receipt = json.loads(receipt_path.read_bytes())
    assert (receipt["versions"]["ome-zarr"], receipt["versions"]["zarr"]) == ("0.18.0", "3.1.6")
    store = args.fixture / "image.ome.zarr"
    verify_inventory(store, receipt["store_member_sha256"])
    level = receipt["levels"][0]["array"]
    dtype = level["data_type"]
    arrays = read_oracle(args.fixture, receipt, dtype)
    profile = sharding_config(level)
    endian = profile["index_codecs"][0]["configuration"]["endian"]
    pair = "<QQ" if endian == "little" else ">QQ"
    checksum = len(profile["index_codecs"]) == 2
    at_start = profile["index_location"] == "start"
    outer = level["codecs"][-1]["name"] == "crc32c"
    entries = prod(level["chunk_grid"]["configuration"]["chunk_shape"]) // prod(profile["chunk_shape"])
    index_size = 16 * entries + 4 * checksum
    shard_path = "s0/c/0/0/0/0/0"
    shard = (store / shard_path).read_bytes()
    if outer:
        body, trailer = shard[:-4], shard[-4:]
        assert trailer == struct.pack("<I", crc32c(body))
    else:
        body = shard
    index, data = (body[:index_size], body[index_size:]) if at_start else (body[-index_size:], body[:-index_size])
    pairs = index[:-4] if checksum else index
    if checksum:
        assert index[-4:] == struct.pack("<I", crc32c(pairs))
    ranges = list(struct.iter_unpack(pair, pairs))
    missing = (1 << 64) - 1
    assert ranges[0] == (missing, missing) and len(ranges) == entries
    present = [i for i, record in enumerate(ranges) if record != (missing, missing)]
    assert len(present) >= 2
    dest_type = ARRAY_FORMATS[dtype][0]
    entry, = (item for item in json.loads(args.manifest.read_bytes())["plugins"]
              if item["provider_id"] == PROVIDER)
    assert entry["version"] == "1.17.0"
    assert entry["module"]["sha256"] == sha(args.manifest.parent / entry["module"]["path"])
    checks = {}

    def read(name, source=store, *, x=0, y=0, width=12, height=8, channel=0, z=0, t=0,
             level_index=0, error=None):
        output = root / f"{name}.npy"
        argv = [args.region, "--reader-plugins", args.manifest, "--reader", PROVIDER,
                "--raw", source, output, "--x", x, "--y", y, "--width", width, "--height", height,
                "--channels", 2, "--channel", channel, "--z", z, "--time", t, "--level", level_index]
        result = subprocess.run(list(map(str, argv)), capture_output=True, encoding="utf-8", timeout=60)
        if error:
            assert result.returncode == 2 and error in result.stderr and not output.exists(), result.stderr
            checks[name] = result.stderr.strip()
            return
        assert result.returncode == 0, result.stderr
        report = json.loads(result.stdout)
        assert report["valid_pixels"] == width * height
        assert report["reader_snapshot"]["implementation"]["module_sha256"] == entry["module"]["sha256"]
        assert npy(output, (height, width, 2), dest_type) == crop(
            arrays, level_index, t, channel, z, x, y, width, height, 2)
        checks[name] = sha(output)

    read("both-channels")
    read("later-plane-and-time", x=1, y=1, width=9, height=6, z=1, t=1)
    read("pyramid", x=1, y=1, width=4, height=3, level_index=1)

    def variant(name):
        return Path(shutil.copytree(store, root / f"{name}.ome.zarr"))

    def wrap(payload):
        return payload + struct.pack("<I", crc32c(payload)) if outer else payload

    def encode(index_bytes):
        framed = index_bytes + struct.pack("<I", crc32c(index_bytes)) if checksum else index_bytes
        return wrap(framed + data if at_start else data + framed)

    def malformed(name, payload, message):
        changed = variant(name)
        (changed / shard_path).write_bytes(payload)
        read(name, changed, width=6, height=4, error=message)

    malformed("short-index", wrap(index[:-1]), "shard is shorter")
    begin, end = (index_size, len(body)) if at_start else (0, len(data))
    selected, neighbor = present[:2]
    for name, index_entry, value, message in (
        ("half-fill-sentinel", 0, (missing, 0), "invalid encoded inner chunk range"),
        ("zero-length", selected, (ranges[selected][0], 0), "invalid encoded inner chunk range"),
        ("overlap", selected, ranges[neighbor], "overlapping encoded inner chunk ranges"),
        ("index-overlap", selected, (begin - 1, 2) if at_start else (end - 1, 2),
         "invalid encoded inner chunk range"),
        ("offset-overflow", selected, (missing - 1, 64), "invalid encoded inner chunk range"),
    ):
        damaged = ranges.copy()
        damaged[index_entry] = value
        malformed(name, encode(b"".join(struct.pack(pair, *item) for item in damaged)), message)
    if checksum:
        damaged = index[:-1] + bytes((index[-1] ^ 1,))
        malformed("index-crc32c-mismatch", wrap(damaged + data if at_start else data + damaged),
                  "shard index CRC32C mismatch")
    if outer:
        malformed("outer-crc32c-mismatch", shard[:-1] + bytes((shard[-1] ^ 1,)),
                  "whole-shard CRC32C mismatch")
    for name, change, error in (
        ("missing-index-endian", lambda pipeline: pipeline[0]["configuration"].clear(), "endian"),
        ("unknown-index-endian", lambda pipeline: pipeline[0]["configuration"].update(endian="native"),
         "invalid shard index byte order"),
        ("variable-width-index", lambda pipeline: pipeline.__setitem__(
            slice(1, None), [{"name": "gzip", "configuration": {"level": 1}}]), "optional crc32c"),
    ):
        changed = variant(name)
        metadata = changed / "s0/zarr.json"
        document = json.loads(metadata.read_bytes())
        change(sharding_config(document)["index_codecs"])
        metadata.write_text(json.dumps(document), encoding="utf-8")
        read(name, changed, width=4, height=3, error=error)
    results = {"passed": True, "checks": checks, "index_at_start": at_start, "index_endian": endian,
               "index_crc32c": checksum, "outer_crc32c": outer, "producer_sha256": sha(receipt_path),
               "module_sha256": entry["module"]["sha256"], "manifest_sha256": sha(args.manifest),
               "region_sha256": sha(args.region)}
    (root / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(checks)} shard-index cases")


if __name__ == "__main__":
    main()
