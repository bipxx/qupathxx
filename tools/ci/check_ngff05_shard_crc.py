# SPDX-License-Identifier: AGPL-3.0-only
"""Check outer Zarr 3 CRC32C without using the production checksum implementation."""

import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess

from check_ngff05 import (ARRAY_FORMATS, FLOAT64_CONTRACT, INT8_CONTRACT, INT32_CONTRACT,
                         INT64_CONTRACT, LEVELS, PROVIDER, SHARD_CRC32C_CONTRACT,
                         crc32c, crop, npy, read_oracle, sha, verify_inventory)


SHARD = "s0/c/0/0/0/0/0"


def wrap(body):
    return body + struct.pack("<I", crc32c(body))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ("fixture", "region", "manifest", "output"):
        parser.add_argument(f"--{flag}", type=Path, required=True)
    parser.add_argument("--contract", choices=(SHARD_CRC32C_CONTRACT, INT32_CONTRACT, INT64_CONTRACT,
                                              FLOAT64_CONTRACT, INT8_CONTRACT),
                        default=SHARD_CRC32C_CONTRACT)
    args = parser.parse_args()
    fixture, region, manifest = (value.resolve(strict=True) for value in (args.fixture, args.region, args.manifest))
    output = args.output.resolve()
    if output.is_relative_to(fixture):
        parser.error("Output must be separate from the fixture")
    output.mkdir(parents=True)
    store = fixture / "image.ome.zarr"
    receipt_path = fixture / "producer.json"
    receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
    assert (receipt["versions"]["ome-zarr"], receipt["versions"]["zarr"]) == ("0.18.0", "3.1.6")
    verify_inventory(store, receipt["store_member_sha256"])
    levels = [item["array"] for item in receipt["levels"]]
    dtype = levels[0]["data_type"]
    descr = ARRAY_FORMATS[dtype][0]
    arrays = read_oracle(fixture, receipt, dtype)
    for level in levels:
        assert level["codecs"][-1] == {"name": "crc32c"}
    config, = (item["configuration"] for item in levels[0]["codecs"] if item["name"] == "sharding_indexed")
    index_location = config["index_location"]
    assert index_location in ("start", "end")
    member = (store / SHARD).read_bytes()
    body = member[:-4]
    assert wrap(body) == member
    index_start = 0 if index_location == "start" else len(body) - (8 * 16 + 4)
    assert crc32c(body[index_start:index_start + 128]) == struct.unpack_from("<I", body, index_start + 128)[0]
    entry, = (plugin for plugin in json.loads(manifest.read_text(encoding="utf-8"))["plugins"]
              if plugin["provider_id"] == PROVIDER)
    module = manifest.parent / entry["module"]["path"]
    assert sha(module) == entry["module"]["sha256"]
    commands = []

    def raw(name, source=store, *, level=0, t=0, z=0, x=0, y=0, width=2, height=2,
            channels=1, diagnostic=None):
        destination = output / f"{name}.npy"
        argv = [region, "--reader-plugins", manifest, "--reader", PROVIDER, "--raw", source, destination,
                "--level", level, "--time", t, "--channel", 0, "--z", z, "--x", x, "--y", y,
                "--width", width, "--height", height, "--channels", channels]
        argv = list(map(str, argv))
        result = subprocess.run(argv, capture_output=True, encoding="utf-8", errors="replace", timeout=60)
        commands.append(dict(name=name, argv=argv, exit_code=result.returncode, stderr=result.stderr))
        if diagnostic:
            assert result.returncode == 2 and diagnostic in result.stderr and not destination.exists(), result.stderr
            return
        assert result.returncode == 0, result.stderr
        report = json.loads(result.stdout)
        implementation = report["reader_snapshot"]["implementation"]
        assert implementation["contract"] == args.contract and implementation["module_sha256"] == sha(module)
        assert report["dtype"] == dtype and report["valid_pixels"] == width * height
        assert npy(destination, (height, width, channels), descr) == crop(
            arrays, level, t, 0, z, x, y, width, height, channels)

    def variant(name):
        return Path(shutil.copytree(store, output / f"{name}.ome.zarr"))

    passed = False
    try:
        for level, geometry in enumerate(LEVELS):
            raw(f"full-level-{level}", level=level, width=geometry["width"], height=geometry["height"], channels=2)
            raw(f"plane-level-{level}", level=level, t=1, z=1, x=geometry["downsample"], channels=2)

        # Insert unused bytes beyond the selected chunks, leaving every index offset intact.
        padding_at = len(body) if index_location == "start" else index_start
        padded = body[:padding_at] + bytes(1024 * 1024 + 19) + body[padding_at:]
        large = variant("multi-slice")
        large_member = wrap(padded)
        (large / SHARD).write_bytes(large_member)
        raw("multi-slice", large)
        damaged = bytearray(large_member)
        damaged[padding_at + 1024 * 1024 + 11] ^= 1
        outside_roi = variant("outside-roi-corruption")
        (outside_roi / SHARD).write_bytes(damaged)
        raw("outside-roi-corruption", outside_roi, diagnostic="whole-shard CRC32C mismatch")

        invalid_index = bytearray(body)
        invalid_index[index_start + 128] ^= 1
        index = variant("index-corruption")
        (index / SHARD).write_bytes(wrap(invalid_index))
        raw("index-corruption", index, diagnostic="shard index CRC32C mismatch")

        if config["codecs"][-1]["name"] == "crc32c":
            pairs = struct.iter_unpack("<QQ", body[index_start:index_start + 128])
            offset, length = next((offset, length) for offset, length in pairs if offset != (1 << 64) - 1)
            invalid_inner = bytearray(body)
            invalid_inner[offset + length - 1] ^= 1
            inner = variant("inner-corruption")
            (inner / SHARD).write_bytes(wrap(invalid_inner))
            raw("inner-corruption", inner, width=12, height=8, channels=2,
                diagnostic="encoded chunk CRC32C mismatch")

        invalid_trailer = variant("bad-trailer")
        (invalid_trailer / SHARD).write_bytes(member[:-1])
        raw("bad-trailer", invalid_trailer, diagnostic="whole-shard CRC32C mismatch")
        too_short = variant("short-shard")
        (too_short / SHARD).write_bytes(bytes(4))
        raw("short-shard", too_short, diagnostic="shorter than its index and CRC32C")
        oversized = variant("oversized")
        with (oversized / SHARD).open("r+b") as file:
            file.truncate(128 * 1024 * 1024 + 1)
        raw("oversized", oversized, diagnostic="whole-shard CRC32C exceeds scan limit")

        for name, codec, message in (
            ("parameters", {"name": "crc32c", "configuration": {"endian": "little"}}, "CRC32C does not take parameters"),
            ("duplicate", {"name": "crc32c"}, "unsupported codecs after indexed sharding"),
        ):
            changed = variant(name)
            path = changed / "s0/zarr.json"
            metadata = json.loads(path.read_text(encoding="utf-8"))
            if name == "parameters":
                metadata["codecs"][-1] = codec
            else:
                metadata["codecs"].append(codec)
            path.write_text(json.dumps(metadata), encoding="utf-8")
            raw(name, changed, diagnostic=message)
        passed = True
    finally:
        result = dict(passed=passed, index_location=index_location, dtype=dtype, fixture_sha256=sha(receipt_path),
                      region_sha256=sha(region), manifest_sha256=sha(manifest), module_sha256=sha(module), commands=commands)
        (output / "results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
