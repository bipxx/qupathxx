# SPDX-License-Identifier: AGPL-3.0-only
"""Generate a synthetic image with the pinned upstream OME-Zarr 0.5 writer."""

import argparse
import hashlib
import json
from importlib.metadata import version
from pathlib import Path

import numpy as np
import zarr
from ome_zarr.writer import write_image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New fixture directory")
    parser.add_argument("--sharded", action="store_true", help="Write indexed shards with the upstream defaults")
    types = parser.add_mutually_exclusive_group()
    types.add_argument("--float32", action="store_true", help="Write signed Float32 intensity values")
    types.add_argument("--float64", action="store_true", help="Write Float64 intensity values")
    types.add_argument("--uint8", action="store_true", help="Write unsigned 8-bit intensity values")
    types.add_argument("--int8", action="store_true", help="Write signed 8-bit intensity values")
    types.add_argument("--int16", action="store_true", help="Write signed 16-bit intensity values")
    types.add_argument("--int32", action="store_true", help="Write signed 32-bit intensity values")
    types.add_argument("--uint32", action="store_true", help="Write unsigned 32-bit intensity values")
    types.add_argument("--int64", action="store_true", help="Write signed 64-bit intensity values")
    types.add_argument("--uint64", action="store_true", help="Write unsigned 64-bit intensity values")
    parser.add_argument("--codec", choices=("bytes", "gzip", "zstd", "blosc-lz4", "blosc-lz4hc", "blosc-lz",
                                            "blosc-zstd", "blosc-zlib"),
                        default="zstd",
                        help="Choose the chunk compressor, or bytes for no compressor")
    parser.add_argument("--big-endian", action="store_true", help="Write big-endian sample bytes")
    parser.add_argument("--omero-display", action="store_true", help="Write signed-off channel display metadata")
    parser.add_argument("--transpose", choices=("chunk", "shard"), help="Write a Zarr 3 transpose codec")
    parser.add_argument("--crc32c", choices=("before", "after", "both"),
                        help="Checksum each chunk before compression, after compression, or both")
    parser.add_argument("--shard-crc32c", choices=("start", "end"),
                        help="Checksum the entire serialized shard and set its index location")
    parser.add_argument("--index-location", choices=("start", "end"))
    parser.add_argument("--index-endian", choices=("little", "big"))
    parser.add_argument("--index-without-crc32c", action="store_true")
    args = parser.parse_args()
    single_byte = args.uint8 or args.int8
    if single_byte and args.big_endian:
        parser.error("Single-byte samples have no byte order")
    integer64 = args.int64 or args.uint64
    if args.omero_display and (single_byte or args.int16 or args.int32 or args.uint32 or integer64
                               or args.float32 or args.float64):
        parser.error("The display fixture uses UInt16 source samples")
    explicit_index = args.index_location or args.index_endian or args.index_without_crc32c
    if (args.transpose == "shard" or args.shard_crc32c or explicit_index) and not args.sharded:
        parser.error("Sharding is required for an outer transpose or checksum")
    if args.index_location and args.shard_crc32c and args.index_location != args.shard_crc32c:
        parser.error("Select the same location for --index-location and --shard-crc32c")
    if args.codec == "bytes" and args.crc32c in ("before", "both"):
        parser.error("Use --crc32c after with uncompressed bytes")
    versions = {name: version(name) for name in ("ome-zarr", "zarr", "numpy", "numcodecs", "dask", "scikit-image")}
    if (versions["ome-zarr"], versions["zarr"]) != ("0.18.0", "3.1.6"):
        parser.error("Use ome-zarr 0.18.0 and zarr 3.1.6")

    args.output.mkdir(parents=True)
    store = args.output / "image.ome.zarr"
    t, c, z, y, x = np.indices((2, 2, 2, 8, 12), dtype=np.uint16)
    samples = (t * 32000 + c * 16000 + z * 8000 + y * 100 + x).astype("<u2")
    if args.float32:
        samples = ((x.astype("<f4") + 3 * y + 5 * z + 11 * c + 17 * t - 13) / 32).astype("<f4")
        samples[0, 0, 0, 0, 6] = -0.0
    elif args.float64:
        samples = ((x.astype("<f8") + 3 * y + 5 * z + 11 * c + 17 * t - 13) / 37).astype("<f8")
    elif single_byte:
        values = (247 + 147 * t + 103 * c + 53 * z + 27 * y + 9 * x) % 256
        samples = (values.astype("i2") - 128).astype("i1") if args.int8 else values.astype("u1")
        if args.int8:
            samples[0, 0, 0, 0, 6:8] = (127, -128)
    elif args.int16:
        samples = ((127 + 39013 * t.astype("i4") + 22819 * c + 7189 * z + 1291 * y + 899 * x)
                   % 65536 - 32768).astype("<i2")
        samples[0, 0, 0, 0, 6:8] = (32767, -32768)
    elif args.int32 or args.uint32:
        samples = (t.astype("u8") * 1031958101 + c.astype("u8") * 705870994
                   + z.astype("u8") * 840395143 + y.astype("u8") * 79191937
                   + x.astype("u8") * 31983557) % (1 << 32)
        if args.int32:
            samples = (samples.astype("i8") - (1 << 31)).astype("<i4")
        else:
            samples = samples.astype("<u4")
        limits = np.iinfo(samples.dtype)
        samples[0, 0, 0, 0, 6:8] = limits.max, limits.min
    elif integer64:
        samples = (2**53 + 41 * t.astype("i8") + 23 * c + 17 * z + 13 * y + 3 * x)
        samples = (-samples).astype("<i8") if args.int64 else samples.astype("<u8")
    storage = {"chunks": (1, 1, 1, 4, 6)}
    if args.sharded:
        storage = {"chunks": (1, 1, 1, 2, 3), "shards": (1, 2, 1, 4, 6)}
        samples[0, 0, 0, :2, :3] = 0
    if args.codec == "bytes":
        storage["compressors"] = ()
    elif args.codec == "gzip":
        storage["compressors"] = (zarr.codecs.GzipCodec(level=1),)
    elif args.codec == "blosc-zstd":
        storage["compressors"] = (zarr.codecs.BloscCodec(),)
    elif args.codec == "blosc-zlib":
        storage["compressors"] = (zarr.codecs.BloscCodec(cname="zlib"),)
    elif args.codec.startswith("blosc-"):
        compressor = {"blosc-lz4": "lz4", "blosc-lz4hc": "lz4hc", "blosc-lz": "blosclz"}[args.codec]
        shuffle = {"blosc-lz4": "shuffle", "blosc-lz4hc": "bitshuffle", "blosc-lz": "noshuffle"}[args.codec]
        storage["compressors"] = (zarr.codecs.BloscCodec(
            cname=compressor, clevel=5, shuffle=shuffle, typesize=samples.dtype.itemsize),)
    if args.crc32c:
        compressor = storage.get("compressors", (zarr.codecs.ZstdCodec(level=0),))
        before = (zarr.codecs.Crc32cCodec(),) if args.crc32c in ("before", "both") else ()
        after = (zarr.codecs.Crc32cCodec(),) if args.crc32c in ("after", "both") else ()
        storage["compressors"] = before + compressor + after
    if args.big_endian:
        storage["serializer"] = zarr.codecs.BytesCodec(endian="big")
    chunk_order = (4, 2, 3, 1, 0)
    shard_order = (0, 3, 2, 1, 4)
    if args.transpose == "chunk" and not (args.shard_crc32c or explicit_index):
        storage["filters"] = (zarr.codecs.TransposeCodec(order=chunk_order),)
    if args.transpose == "shard" or args.shard_crc32c or explicit_index:
        inner_shape = storage["chunks"]
        outer_shape = storage.pop("shards")
        outer_order = shard_order if args.transpose == "shard" else tuple(range(len(inner_shape)))
        inner_filters = (zarr.codecs.TransposeCodec(order=chunk_order),) if args.transpose else ()
        codecs = (
            *inner_filters,
            zarr.codecs.BytesCodec(endian=None if single_byte else "big" if args.big_endian else "little"),
            *storage.get("compressors", (zarr.codecs.ZstdCodec(level=0),)),
        )
        index_codecs = (zarr.codecs.BytesCodec(endian=args.index_endian or "little"),)
        if not args.index_without_crc32c:
            index_codecs += (zarr.codecs.Crc32cCodec(),)
        storage.update(chunks=outer_shape,
                       filters=(zarr.codecs.TransposeCodec(order=outer_order),) if args.transpose == "shard" else (),
                       serializer=zarr.codecs.ShardingCodec(
                           chunk_shape=tuple(inner_shape[axis] for axis in outer_order), codecs=codecs,
                           index_codecs=index_codecs,
                           index_location=args.index_location or args.shard_crc32c or "end"),
                       compressors=(zarr.codecs.Crc32cCodec(),) if args.shard_crc32c else ())
    display = {}
    if args.omero_display:
        display["omero"] = {
            "channels": [
                {"label": "DAPI", "color": "FF0080", "active": True,
                 "window": {"min": 0, "max": 65535, "start": 5000, "end": 36000}},
                {"label": "Mélange", "color": "4080FF", "active": True,
                 "window": {"min": 0, "max": 65535, "start": 10000, "end": 54000}},
            ],
            "rdefs": {"defaultT": 1, "defaultZ": 1, "model": "color"},
        }
    write_image(
        samples,
        str(store),
        scale_factors=(2,),
        axes="tczyx",
        scale={"t": 3, "c": 1, "z": 2, "y": 0.75, "x": 0.5},
        axes_units={"t": "second", "z": "micrometer", "y": "micrometer", "x": "micrometer"},
        storage_options=storage,
        **display,
    )

    root = json.loads((store / "zarr.json").read_text(encoding="utf-8"))
    ome = root["attributes"]["ome"]
    if args.omero_display:
        assert ome["omero"]["channels"] == display["omero"]["channels"]
        assert ome["omero"]["rdefs"] == display["omero"]["rdefs"]
    if (root["zarr_format"], ome["version"]) != (3, "0.5"):
        raise ValueError("Unexpected default writer format")
    datasets = ome["multiscales"][0]["datasets"]
    if len(datasets) != 2:
        raise ValueError("Expected exactly two pyramid levels")
    if integer64:
        # Write the endpoints after the producer builds the reduced level so
        # its resampler does not round them through binary64.
        limits = np.iinfo(samples.dtype)
        adjacent = -(2**53) if args.int64 else 2**53
        samples[0, 0, 0, 0, 6:10] = limits.max, limits.min, adjacent, adjacent + 1
        group = zarr.open_group(store, mode="r+")
        group[datasets[0]["path"]][0, 0, 0, 0, 6:10] = samples[0, 0, 0, 0, 6:10]
    elif args.float64:
        # Exercise encodings that cannot survive narrowing or resampling.
        samples.view("<u8")[0, 0, 0, 0, 3:10] = (
            0x8000000000000000, 1, 0x3FF0000000000001, 0x7FF0000000000042,
            0xFFF8000000000084, 0x7FF0000000000000, 0xFFF0000000000000,
        )
        group = zarr.open_group(store, mode="r+")
        group[datasets[0]["path"]][0, 0, 0, 0, 3:10] = samples[0, 0, 0, 0, 3:10]
    group = zarr.open_group(store, mode="r")
    members = {}
    for member in sorted(store.rglob("*")):
        if member.is_file():
            members[member.relative_to(store).as_posix()] = hashlib.sha256(member.read_bytes()).hexdigest()
    levels = []
    for index, dataset in enumerate(datasets):
        key = dataset["path"]
        decoded = group[key][:]
        if index == 0:
            bits = "u4" if args.float32 else "u8" if args.float64 else samples.dtype
            np.testing.assert_array_equal(decoded.view(bits), samples.view(bits))
        metadata = json.loads((store / key / "zarr.json").read_text(encoding="utf-8"))
        codecs = metadata["codecs"]
        if args.transpose == "shard":
            assert codecs[0] == {"name": "transpose", "configuration": {"order": list(shard_order)}}
            codecs = codecs[1:]
        if args.sharded:
            if args.shard_crc32c:
                assert codecs[-1]["name"] == "crc32c"
                codecs = codecs[:-1]
                assert codecs[0]["configuration"]["index_location"] == args.shard_crc32c
            assert len(codecs) == 1 and codecs[0]["name"] == "sharding_indexed"
            codecs = codecs[0]["configuration"]["codecs"]
        if args.transpose:
            assert codecs[0] == {"name": "transpose", "configuration": {"order": list(chunk_order)}}
            codecs = codecs[1:]
        expected_codecs = ["bytes"]
        if args.crc32c in ("before", "both"):
            expected_codecs.append("crc32c")
        if args.codec != "bytes":
            expected_codecs.append("blosc" if args.codec.startswith("blosc-") else args.codec)
        if args.crc32c in ("after", "both"):
            expected_codecs.append("crc32c")
        assert [item["name"] for item in codecs] == expected_codecs
        endian = codecs[0].get("configuration", {}).get("endian")
        if single_byte:
            assert endian is None
        else:
            assert endian == ("big" if args.big_endian else "little")
        dtype = samples.dtype.str
        decoded = decoded.astype(dtype, copy=False)
        np.save(args.output / f"expected-level-{index}.npy", decoded, allow_pickle=False)
        digest_key = {"<f4": "raw_le_f32_sha256", "|u1": "raw_u8_sha256", "|i1": "raw_i8_sha256",
                      "<i2": "raw_le_i16_sha256", "<u2": "raw_le_u16_sha256",
                      "<i4": "raw_le_i32_sha256", "<u4": "raw_le_u32_sha256",
                      "<i8": "raw_le_i64_sha256", "<u8": "raw_le_u64_sha256",
                      "<f8": "raw_le_f64_sha256"}[dtype]
        levels.append({"dataset": dataset, "array": metadata,
                       digest_key: hashlib.sha256(decoded.tobytes()).hexdigest()})

    receipt = {"versions": versions, "root": root, "levels": levels, "store_member_sha256": members}
    (args.output / "producer.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
