# SPDX-License-Identifier: AGPL-3.0-only
"""Check production Blosc frames beyond small memcpy-only fixtures."""

import argparse
from hashlib import sha256
from importlib.metadata import version
import json
from pathlib import Path
import shutil
import struct
import subprocess

import numpy as np
from ome_zarr.writer import write_image
import zarr
from check_ngff05 import crc32c


def encode_snappy_frames(store, sharded):
    import pyarrow as pa

    def frame(raw):
        encoded = pa.compress(raw, codec="snappy", asbytes=True)
        assert len(encoded) < len(raw)
        assert pa.decompress(encoded, len(raw), codec="snappy", asbytes=True) == raw
        # Blosc v2, Snappy v1, one unsplit block, no shuffle.
        return struct.pack("<BBBBIIIII", 2, 1, 0x50, 1, len(raw), len(raw),
                           24 + len(encoded), 20, len(encoded)) + encoded

    codec = {"name": "blosc", "configuration": {
        "cname": "snappy", "clevel": 5, "shuffle": "noshuffle", "blocksize": 0,
    }}
    for path in store.glob("*/zarr.json"):
        document = json.loads(path.read_bytes())
        pipeline = document["codecs"]
        if sharded:
            pipeline = pipeline[0]["configuration"]["codecs"]
        pipeline.append(codec)
        path.write_text(json.dumps(document), encoding="utf-8")
        for chunk in (path.parent / "c").rglob("*"):
            if not chunk.is_file():
                continue
            raw = chunk.read_bytes()
            if not sharded:
                chunk.write_bytes(frame(raw))
                continue
            index = raw[-(8 * 16 + 4):-4]
            assert struct.unpack("<I", raw[-4:])[0] == crc32c(index)
            data = bytearray()
            entries = []
            for offset, length in struct.iter_unpack("<QQ", index):
                if offset == length == (1 << 64) - 1:
                    entries.append((offset, length))
                else:
                    encoded = frame(raw[offset:offset + length])
                    entries.append((len(data), len(encoded)))
                    data.extend(encoded)
            index = b"".join(struct.pack("<QQ", *entry) for entry in entries)
            chunk.write_bytes(data + index + struct.pack("<I", crc32c(index)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("region", "manifest", "output"):
        parser.add_argument(f"--{key}", type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--with-zstd", action="store_true")
    parser.add_argument("--all-families", action="store_true")
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True)
    y, x = np.indices((64, 96))
    values = (2**15 + 11 * (y // 4) + 13 * (x // 6)).astype("<u2")
    samples = np.stack((values, (values + 513).astype("<u2")))
    checks = {}

    def read(name, store, expected=None, *, error=None):
        output = root / f"{name}.npy"
        command = [args.region, "--reader-plugins", args.manifest, "--reader",
                   "org.qupathxx.ngff05", "--raw", store, output, "--width", "96",
                   "--height", "64", "--channels", "2"]
        result = subprocess.run(command, capture_output=True, encoding="utf-8", timeout=60)
        if error:
            assert result.returncode == 2 and error in result.stderr and not output.exists(), result.stderr
            checks[name] = result.stderr.strip()
            return
        assert result.returncode == 0, result.stderr
        receipt = json.loads(result.stdout)
        assert receipt["valid_pixels"] == 64 * 96
        np.testing.assert_array_equal(np.load(output), expected.transpose(1, 2, 0))
        checks[name] = sha256(output.read_bytes()).hexdigest()

    profiles = [("lz4", "shuffle", False), ("lz4hc", "bitshuffle", True),
                ("blosclz", "noshuffle", False)]
    if args.with_zstd or args.all_families:
        profiles += [("zstd", None, False), ("zstd", None, True)]
    if args.all_families:
        profiles += [("zlib", None, False), ("zlib", None, True),
                     ("snappy", "noshuffle", False), ("snappy", "noshuffle", True)]
    for cname, shuffle, sharded in profiles:
        profile = f"{cname}-shard" if sharded else cname
        store = root / f"{profile}.ome.zarr"
        compressor = zarr.codecs.BloscCodec(cname=cname, shuffle=shuffle, typesize=2, clevel=5)
        if cname in ("zstd", "zlib"):
            compressor = zarr.codecs.BloscCodec() if cname == "zstd" else zarr.codecs.BloscCodec(cname="zlib")
        storage = {"chunks": (1, 16, 24), "compressors": () if cname == "snappy" else (compressor,)}
        if sharded:
            storage["shards"] = (2, 32, 48)
        write_image(samples, str(store), scale_factors=(2,), axes="cyx",
                    scale={"c": 1, "y": .75, "x": .5},
                    axes_units={"y": "micrometer", "x": "micrometer"}, storage_options=storage)
        group = zarr.open_group(store, mode="r")
        level = group.attrs["ome"]["multiscales"][0]["datasets"][0]["path"]
        decoded = group[level][:]
        np.testing.assert_array_equal(decoded, samples)
        if cname == "snappy":
            encode_snappy_frames(store, sharded)
        key = f"{level}/c/0/0/0"
        member = (store / key).read_bytes()
        if sharded:
            index = member[-(8 * 16 + 4):-4]
            offset, length = struct.unpack_from("<QQ", index)
            member = member[offset:offset + length]
        # Verify that the producer actually compressed rather than copying.
        assert member[0] == 2 and not member[2] & 2
        assert struct.unpack_from("<I", member, 4)[0] == 16 * 24 * 2
        read(profile, store, decoded)
        if cname in ("zlib", "snappy") and not sharded:
            plain, blocksize, extent = struct.unpack_from("<III", member, 4)
            assert plain == blocksize and extent == len(member)
            split, = struct.unpack_from("<I", member, 16)
            count = 1 if member[2] & 0x10 else member[3]
            for _ in range(count):
                size_offset = split
                size, = struct.unpack_from("<I", member, size_offset)
                split += 4 + size
            assert len(member) == split
            target = root / f"trailing-inner-{cname}.ome.zarr"
            shutil.copytree(store, target)
            trailing = bytearray(member + b"\0")
            struct.pack_into("<I", trailing, 12, len(trailing))
            struct.pack_into("<I", trailing, size_offset, size + 1)
            (target / key).write_bytes(trailing)
            read(f"trailing-inner-{cname}", target, error="corrupt Blosc chunk")
        if cname == "zstd" and not sharded:
            block, = struct.unpack_from("<I", member, 16)
            size, = struct.unpack_from("<I", member, block)
            stream = block + 4
            assert size < 16 * 24 * 2 and member[stream:stream + 4] == b"\x28\xb5\x2f\xfd"
            target = root / "bad-inner-zstd.ome.zarr"
            shutil.copytree(store, target)
            broken = bytearray(member)
            broken[stream] ^= 1
            (target / key).write_bytes(broken)
            read("bad-inner-zstd", target, error="corrupt Blosc chunk")
        if sharded or cname != "lz4":
            continue
        for name, malformed in (("truncated", member[:-1]), ("trailing", member + b"\x00")):
            target = root / f"{name}.ome.zarr"
            shutil.copytree(store, target)
            (target / key).write_bytes(malformed)
            read(name, target, error="Blosc frame")
        for field, setting, diagnostic in (
            ("typesize", 4, "frame differs"),
            ("shuffle", "bitshuffle", "frame differs"),
            ("cname", "blosclz", "frame differs"),
            ("cname", "unknown-codec", "unsupported Zarr3 Blosc"),
            ("blocksize", (4 << 20) + 1, "invalid Zarr3 Blosc"),
        ):
            name = f"{field}-{setting}"
            target = root / f"{name}.ome.zarr"
            shutil.copytree(store, target)
            path = target / level / "zarr.json"
            document = json.loads(path.read_bytes())
            codec = next(codec for codec in document["codecs"] if codec["name"] == "blosc")
            codec["configuration"][field] = setting
            path.write_text(json.dumps(document), encoding="utf-8")
            read(name, target, error=diagnostic)
    receipt = {"passed": True, "checks": checks, "versions": {
        name: version(name) for name in ("ome-zarr", "zarr", "numpy", "numcodecs")
    }, "manifest_sha256": sha256(args.manifest.read_bytes()).hexdigest()}
    if args.all_families:
        receipt["versions"]["pyarrow"] = version("pyarrow")
    (root / "results.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(checks)} Blosc frame cases")


if __name__ == "__main__":
    main()
