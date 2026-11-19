# SPDX-License-Identifier: AGPL-3.0-only
"""Exercise the installed NGFF 0.5 image provider with the pinned producer store."""

import argparse
import ast
import gzip
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess


PROVIDER = "org.qupathxx.ngff05"
CONTRACT = "ngff05-zarr3-uint16/1"
SHARD_CONTRACT = "ngff05-zarr3-uint16/2"
TYPED_CONTRACT = "ngff05-zarr3-image/3"
CODEC_CONTRACT = "ngff05-zarr3-image/4"
UINT8_CONTRACT = "ngff05-zarr3-image/5"
INT16_CONTRACT = "ngff05-zarr3-image/6"
DISPLAY_CONTRACT = "ngff05-zarr3-image/7"
TRANSPOSE_CONTRACT = "ngff05-zarr3-image/8"
CRC32C_CONTRACT = "ngff05-zarr3-image/9"
SHARD_CRC32C_CONTRACT = "ngff05-zarr3-image/10"
INT32_CONTRACT = "ngff05-zarr3-image/11"
INT64_CONTRACT = "ngff05-zarr3-image/12"
FLOAT64_CONTRACT = "ngff05-zarr3-image/13"
INT8_CONTRACT = "ngff05-zarr3-image/14"
BLOSC_CONTRACT = "ngff05-zarr3-image/15"
BLOSC_ZSTD_CONTRACT = "ngff05-zarr3-image/16"
BLOSC_ALL_CONTRACT = "ngff05-zarr3-image/17"
FLEXIBLE_INDEX_CONTRACT = "ngff05-zarr3-image/18"
ARRAY_FORMATS = {
    "uint8": ("|u1", "B", "raw_u8_sha256"),
    "int8": ("|i1", "b", "raw_i8_sha256"),
    "uint16": ("<u2", "H", "raw_le_u16_sha256"),
    "int16": ("<i2", "h", "raw_le_i16_sha256"),
    "int32": ("<i4", "i", "raw_le_i32_sha256"),
    "uint32": ("<u4", "I", "raw_le_u32_sha256"),
    "int64": ("<i8", "q", "raw_le_i64_sha256"),
    "uint64": ("<u8", "Q", "raw_le_u64_sha256"),
    "float32": ("<f4", "I", "raw_le_f32_sha256"),
    "float64": ("<f8", "Q", "raw_le_f64_sha256"),
}
PACK_CODES = {descr: code for descr, code, _ in ARRAY_FORMATS.values()}
LEVELS = [
    {"width": 12, "height": 8, "downsample": 1},
    {"width": 6, "height": 4, "downsample": 2},
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def npy(path, shape, dtype="<u2"):
    with path.open("rb") as file:
        assert file.read(8) == b"\x93NUMPY\x01\x00", path
        length, = struct.unpack("<H", file.read(2))
        assert ast.literal_eval(file.read(length).decode("ascii")) == {
            "descr": dtype, "fortran_order": False, "shape": shape,
        }, path
        values = tuple(value for value, in struct.iter_unpack("<" + PACK_CODES[dtype], file.read()))
    count = 1
    for size in shape:
        count *= size
    assert len(values) == count, path
    return values


def verify_inventory(store, members):
    observed = {p.relative_to(store).as_posix() for p in store.rglob("*") if p.is_file()}
    assert observed == members.keys(), (observed ^ members.keys())
    assert all(sha(store / key) == digest for key, digest in members.items())


def read_oracle(fixture, receipt, dtype):
    arrays = []
    descr, code, digest = ARRAY_FORMATS[dtype]
    for level, geometry in enumerate(LEVELS):
        shape = (2, 2, 2, geometry["height"], geometry["width"])
        values = npy(fixture / f"expected-level-{level}.npy", shape, descr)
        raw_hash = hashlib.sha256(struct.pack(f"<{len(values)}{code}", *values)).hexdigest()
        assert raw_hash == receipt["levels"][level][digest]
        arrays.append(values)
    return arrays


def crop(arrays, level, t, c, z, x, y, width, height, channels):
    geometry = LEVELS[level]
    w, h, ds = (geometry[key] for key in ("width", "height", "downsample"))
    x0, y0 = x // ds, y // ds
    return tuple(arrays[level][(((t * 2 + cc) * 2 + z) * h + yy) * w + xx]
                 for yy in range(y0, y0 + height) for xx in range(x0, x0 + width)
                 for cc in range(c, c + channels))


def crc32c(data):
    crc = 0xffffffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82f63b78 if crc & 1 else 0)
    return crc ^ 0xffffffff


def sharding_config(metadata):
    codec, = (item for item in metadata["codecs"] if item["name"] == "sharding_indexed")
    return codec["configuration"]

def storage_codec_lists(metadata):
    outer = metadata["codecs"]
    for codec in outer:
        if codec["name"] == "sharding_indexed":
            return outer, codec["configuration"]["codecs"]
    return outer, outer


def storage_profile(metadata):
    outer, inner = storage_codec_lists(metadata)
    sharded = outer is not inner
    transpose = None
    if sharded and any(codec["name"] == "transpose" for codec in outer):
        transpose = "shard"
    elif any(codec["name"] == "transpose" for codec in inner):
        transpose = "chunk"
    return sharded, transpose, [codec for codec in inner if codec["name"] != "transpose"]

def data_pipeline(codecs):
    names = [codec["name"] for codec in codecs]
    compressor = next((name for name in names if name in ("gzip", "zstd", "blosc")), "bytes")
    if "crc32c" not in names:
        return compressor, None
    if compressor == "bytes":
        assert names == ["bytes", "crc32c"]
        return compressor, "after"
    index = names.index(compressor)
    before = "crc32c" in names[:index]
    after = "crc32c" in names[index + 1:]
    expected = ["bytes"] + (["crc32c"] if before else []) + [compressor] + (["crc32c"] if after else [])
    assert names == expected
    return compressor, "both" if before and after else "before" if before else "after"


def check_shards(store, receipt, arrays, raw, variant, edit_json):
    shard_key = "s0/c/0/0/0/0/0"
    shard = (store / shard_key).read_bytes()
    config = sharding_config(receipt["levels"][0]["array"])
    assert config["index_location"] == "end"
    assert config["index_codecs"] == [{"name": "bytes", "configuration": {"endian": "little"}},
                                      {"name": "crc32c"}]
    assert config["chunk_shape"] == [1, 1, 1, 2, 3]
    assert receipt["levels"][0]["array"]["chunk_grid"]["configuration"]["chunk_shape"] == [1, 2, 1, 4, 6]
    index_size = 8 * 16 + 4
    data, index, checksum = shard[:-index_size], shard[-index_size:-4], shard[-4:]
    sentinel = (1 << 64) - 1
    assert struct.unpack("<I", checksum)[0] == crc32c(index)
    entries = list(struct.iter_unpack("<QQ", index))
    # The official producer elides the all-fill first inner chunk while its
    # sibling channel and other chunks in the same shard remain present.
    assert entries[0] == (sentinel, sentinel) and all(length > 0 for _, length in entries[1:])
    assert all(offset + length <= len(data) for offset, length in entries[1:])

    raw("cross-inner-chunk-xy-and-channel", x=2, y=1, width=3, height=3, channels=2)
    raw("cross-outer-shard-xy-and-channel", x=5, y=3, width=3, height=3, channels=2)
    raw("cross-inner-chunk-reduced-level", level=1, x=5, y=3, width=3, height=3, channels=2)

    changed = variant("shard-declared-inner-fill")
    edit_json(changed, "s0/zarr.json", lambda doc: doc.update(fill_value=4321))
    sibling = crop(arrays, 0, 0, 1, 0, 0, 0, 3, 2, 1)
    fill = tuple(component for sample in sibling for component in (4321, sample))
    raw("shard-declared-inner-fill", changed, width=3, height=2, channels=2, expected=fill)
    changed = variant("shard-declared-outer-fill")
    (changed / shard_key).unlink()
    edit_json(changed, "s0/zarr.json", lambda doc: doc.update(fill_value=4321))
    raw("shard-declared-outer-fill", changed, width=6, height=4, channels=2, expected=(4321,) * 48)

    def broken_shard(name, value):
        changed = variant(name)
        (changed / shard_key).write_bytes(value)
        # Request a present inner chunk so a corrupt payload cannot be
        # accepted by reusing the neighboring declared fill.
        raw(name, changed, x=3, status=2)

    def encode_index(changed):
        index_bytes = b"".join(struct.pack("<QQ", *entry) for entry in changed)
        return index_bytes + struct.pack("<I", crc32c(index_bytes))

    def indexed(payload, changed):
        return payload + encode_index(changed)

    def replaced(entry_num, offset, length):
        changed = entries.copy()
        changed[entry_num] = (offset, length)
        return changed

    broken_shard("shard-invalid-index-crc32c", shard[:-1] + bytes([shard[-1] ^ 1]))
    broken_shard("shard-truncated-index", shard[:index_size - 1])
    broken_shard("shard-half-sentinel", indexed(data, replaced(0, sentinel, 0)))
    broken_shard("shard-zero-length", indexed(data, replaced(1, entries[1][0], 0)))
    broken_shard("shard-range-enters-footer", indexed(data, replaced(1, len(data) - 1, 2)))
    broken_shard("shard-range-overflow", indexed(data, replaced(1, sentinel - 1, 3)))
    broken_shard("shard-overlapping-ranges", indexed(data, replaced(1, *entries[2])))
    offset, _ = entries[1]
    bad_data = bytearray(data)
    bad_data[offset] ^= 1
    broken_shard("shard-invalid-inner-zstd", bytes(bad_data) + index + checksum)

    header = variant("shards-with-index-at-start")
    edit_json(header, "s0/zarr.json", lambda doc: sharding_config(doc).update(index_location="start"))
    for key in receipt["store_member_sha256"]:
        if key.startswith("s0/c/"):
            path = header / key
            member = path.read_bytes()
            ranges = struct.iter_unpack("<QQ", member[-index_size:-4])
            shifted = [(start + index_size, length) if start != sentinel else (start, length)
                       for start, length in ranges]
            path.write_bytes(encode_index(shifted) + member[:-index_size])
    raw("shard-index-at-start", header, x=5, y=3, width=3, height=3, channels=2)
    raw("shard-mixed-index-locations", header, level=1, x=5, y=3, width=3, height=3, channels=2)
    bad_header = variant("shard-range-enters-header")
    edit_json(bad_header, "s0/zarr.json", lambda doc: sharding_config(doc).update(index_location="start"))
    start_ranges = [(start + index_size, length) if start != sentinel else (start, length)
                    for start, length in entries]
    start_ranges[1] = (index_size - 1, 2)
    (bad_header / shard_key).write_bytes(encode_index(start_ranges) + data)
    raw("shard-range-enters-header", bad_header, x=3, status=2)

    for name, change in (
        ("shard-indivisible-grid", lambda config: config["chunk_shape"].__setitem__(4, 4)),
        ("shard-unadmitted-index-codec", lambda config: config["index_codecs"][1].update(name="gzip")),
        ("shard-unknown-index-location", lambda config: config.update(index_location="middle")),
        ("shard-unadmitted-inner-codec", lambda config: config["codecs"][1].update(name="sharding_indexed")),
    ):
        changed = variant(name)
        edit_json(changed, "s0/zarr.json", lambda doc: change(sharding_config(doc)))
        raw(name, changed, status=2)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("region", "manifest", "fixture", "output"):
        parser.add_argument(f"--{option}", type=Path, required=True)
    parser.add_argument("--sharded", action="store_true", help="Check the producer's indexed-shard fixture")
    types = parser.add_mutually_exclusive_group()
    types.add_argument("--float32", action="store_true", help="Compare IEEE binary32 storage bits")
    types.add_argument("--float64", action="store_true", help="Compare IEEE binary64 storage bits")
    types.add_argument("--uint8", action="store_true", help="Compare exact UInt8 storage")
    types.add_argument("--int8", action="store_true", help="Compare exact Int8 storage")
    types.add_argument("--int16", action="store_true", help="Compare exact signed Int16 storage")
    types.add_argument("--int32", action="store_true", help="Compare exact signed Int32 storage")
    types.add_argument("--uint32", action="store_true", help="Compare exact UInt32 storage")
    types.add_argument("--int64", action="store_true", help="Compare exact signed Int64 storage")
    types.add_argument("--uint64", action="store_true", help="Compare exact UInt64 storage")
    parser.add_argument("--codec", choices=("bytes", "gzip", "zstd", "blosc"), default="zstd")
    parser.add_argument("--big-endian", action="store_true")
    parser.add_argument("--omero-display", action="store_true")
    parser.add_argument("--transpose", choices=("chunk", "shard"))
    parser.add_argument("--crc32c", choices=("before", "after", "both"))
    parser.add_argument("--contract", choices=(CONTRACT, SHARD_CONTRACT, TYPED_CONTRACT,
                                               CODEC_CONTRACT, UINT8_CONTRACT, INT16_CONTRACT, DISPLAY_CONTRACT,
                                               TRANSPOSE_CONTRACT, CRC32C_CONTRACT, SHARD_CRC32C_CONTRACT,
                                               INT32_CONTRACT, INT64_CONTRACT, FLOAT64_CONTRACT, INT8_CONTRACT,
                                               BLOSC_CONTRACT, BLOSC_ZSTD_CONTRACT, BLOSC_ALL_CONTRACT,
                                               FLEXIBLE_INDEX_CONTRACT),
                        help="Expected contract; defaults to the fixture profile")
    args = parser.parse_args()
    single_byte = args.uint8 or args.int8
    if single_byte and args.big_endian:
        parser.error("Single-byte samples have no byte order")
    integer32 = args.int32 or args.uint32
    integer64 = args.int64 or args.uint64
    contract = args.contract or (BLOSC_CONTRACT if args.codec == "blosc" else
                                 INT8_CONTRACT if args.int8 else FLOAT64_CONTRACT if args.float64 else
                                 INT64_CONTRACT if integer64 else INT32_CONTRACT if integer32 else
                                 CRC32C_CONTRACT if args.crc32c else TRANSPOSE_CONTRACT if args.transpose else
                                 DISPLAY_CONTRACT if args.omero_display else INT16_CONTRACT if args.int16 else
                                 UINT8_CONTRACT if args.uint8 else
                                 CODEC_CONTRACT if args.codec != "zstd" else
                                 TYPED_CONTRACT if args.float32 else SHARD_CONTRACT if args.sharded else CONTRACT)
    dtype = next((name for name in ARRAY_FORMATS if getattr(args, name, False)), "uint16")
    endian = None if single_byte else "big" if args.big_endian else "little"
    descr, sample_code, _ = ARRAY_FORMATS[dtype]
    integer_width = 64 if integer64 else 32
    wide_integer = integer32 or integer64
    is_signed = args.int32 or args.int64
    minimum = -(1 << (integer_width - 1)) if is_signed else 0
    maximum = (1 << (integer_width - int(is_signed))) - 1
    integer_fill = ((minimum if is_signed else maximum) if wide_integer else
                    255 if args.uint8 else -128 if args.int8 else -32768 if args.int16 else 4321)
    fill_value, fill_word = (-0.25, 0xBFD0000000000000) if args.float64 else (
        (-0.25, 0xBE800000) if args.float32 else (integer_fill, integer_fill))
    fixture = args.fixture.resolve(strict=True)
    manifest = args.manifest.resolve(strict=True)
    region = args.region.resolve(strict=True)
    output = args.output.resolve()
    store = fixture / "image.ome.zarr"
    if output.is_relative_to(fixture):
        parser.error("Output must be separate from the fixture")
    output.mkdir(parents=True)
    receipt = json.loads((fixture / "producer.json").read_text(encoding="utf-8"))
    producer_display = receipt["root"]["attributes"]["ome"].get("omero")
    assert (producer_display is not None) == args.omero_display
    if args.omero_display:
        assert [record["label"] for record in producer_display["channels"]] == ["DAPI", "Mélange"]
        assert producer_display["rdefs"] == {"defaultT": 1, "defaultZ": 1, "model": "color"}
    assert receipt["versions"]["ome-zarr"] == "0.18.0" and receipt["versions"]["zarr"] == "3.1.6"
    members = receipt["store_member_sha256"]
    if args.sharded and not args.transpose:
        assert len(members) == 23, "Unexpected upstream sharded fixture membership"
    verify_inventory(store, members)
    arrays = read_oracle(fixture, receipt, dtype)
    assert all(level["array"]["data_type"] == dtype for level in receipt["levels"])
    for level in receipt["levels"]:
        sharded, transpose, codecs = storage_profile(level["array"])
        assert (sharded, transpose) == (args.sharded, args.transpose)
        assert data_pipeline(codecs) == (args.codec, args.crc32c)
        assert codecs[0].get("configuration", {}).get("endian") == endian
    entry, = (item for item in json.loads(manifest.read_text(encoding="utf-8"))["plugins"]
              if item["provider_id"] == PROVIDER)
    module = manifest.parent / entry["module"]["path"]
    assert sha(module) == entry["module"]["sha256"]

    commands = []
    prefix = [region, "--reader-plugins", manifest]
    reader = ["--reader", PROVIDER]

    def run(name, command, status=0):
        argv = [str(value) for value in command]
        result = subprocess.run(argv, capture_output=True, text=True, encoding="utf-8", errors="replace",
                                timeout=60)
        commands.append({"name": name, "argv": argv, "exit_code": result.returncode,
                         "stdout": result.stdout, "stderr": result.stderr})
        assert result.returncode == status, (name, status, result.returncode, result.stdout, result.stderr)
        return result.stdout

    observed_sha = None

    def raw(name, source=store, *, level=0, t=0, c=0, z=0, x=0, y=0, width=1, height=1,
            channels=1, status=0, expected=None, diagnostic=None):
        nonlocal observed_sha
        dest = output / (name + ".npy")
        result = run(name, [*prefix, *reader, "--raw", source, dest, "--level", level, "--time", t,
                            "--channel", c, "--z", z, "--x", x, "--y", y, "--width", width,
                            "--height", height, "--channels", channels], status)
        if status:
            assert not dest.exists(), name
            assert commands[-1]["stderr"].strip(), name
            if diagnostic:
                assert diagnostic in commands[-1]["stderr"], commands[-1]
            return
        info = json.loads(result)
        assert {key: info[key] for key in (
            "schema", "reader", "dtype", "level", "time", "first_channel", "z", "x", "y", "width", "height",
            "channels", "valid_pixels",
        )} == {
            "schema": "qpx.raw-region/2", "reader": PROVIDER, "dtype": dtype, "level": level, "time": t,
            "first_channel": c, "z": z, "x": x, "y": y, "width": width, "height": height,
            "channels": channels, "valid_pixels": width * height,
        }, name
        # CLI reports the in-memory payload, including one validity byte per pixel.
        assert info["raw_bytes"] == width * height * (channels * struct.calcsize("<" + sample_code) + 1)
        snapshot = info["reader_snapshot"]
        assert snapshot["schema"] == "qpx.reader-plugin-link/2", name
        implementation = snapshot["implementation"]
        assert implementation["provider_id"] == PROVIDER and implementation["contract"] == contract
        assert implementation["module_sha256"] == entry["module"]["sha256"]
        assert implementation["capabilities"] == 11  # RGBA | RAW | SOURCE_STORE
        observed = snapshot["observed"]
        assert observed["scope"] == "directory_store"
        assert Path(observed["locator"]) == source and Path(observed["primary"]["path"]) == source / "zarr.json"
        assert len(bytes.fromhex(observed["directory_sha256"])) == 32
        if source == store:
            if observed_sha is None:
                observed_sha = observed["directory_sha256"]
            assert observed["directory_sha256"] == observed_sha
        calibration = {"pixel_width_um": 0.5, "pixel_height_um": 0.75}
        dimensions = {"width": 12, "height": 8, "levels": LEVELS, **calibration}
        assert all(snapshot["metadata"][key] == val for key, val in dimensions.items())
        raw_metadata = {**dimensions, "channels": 2, "sample_type": dtype, "z_planes": 2, "timepoints": 2,
                        "series": 0, "series_count": 1, "dimension_order": "TCZYX",
                        "z_spacing_um": 2, "time_interval_seconds": 3}
        if args.omero_display:
            raw_metadata["channel_names"] = ["DAPI", "Mélange"]
        assert all(snapshot["raw"][key] == val for key, val in raw_metadata.items()), snapshot["raw"]
        if expected is None:
            expected = crop(arrays, level, t, c, z, x, y, width, height, channels)
        assert npy(dest, (height, width, channels), descr) == expected, name

    def variant(name):
        copy = output / name / "changed.ome.zarr"
        shutil.copytree(store, copy)
        return copy

    def edit_json(source, key, edit):
        path = source / key
        metadata = json.loads(path.read_text(encoding="utf-8"))
        edit(metadata)
        path.write_text(json.dumps(metadata), encoding="utf-8")

    def omero_colors(records, values):
        """Independent byte oracle using the declared linear display data."""
        sums = [0.0, 0.0, 0.0]
        for record, value in zip(records, values):
            if not record.get("active", True):
                continue
            window = record["window"]
            fraction = min(1, max(0, (value - window["start"]) / (window["end"] - window["start"])))
            if record.get("inverted", False):
                fraction = 1 - fraction
            for component, weight in enumerate(bytes.fromhex(record["color"])):
                sums[component] += fraction * weight
        return tuple(math.floor(min(255, component) + .5) for component in sums) + (255,)

    def display_variant(name, edit):
        source = variant(name)
        edit_json(source, "zarr.json", lambda doc: edit(doc["attributes"]["ome"]["omero"]))
        return source

    def display_check(name, source, expected=None, status=0, x=5, y=3, width=1, height=1):
        pam = output / f"{name}.pam"
        run(name, [*prefix, *reader, source, x, y, 0, width, height, pam], status)
        if status:
            assert not pam.exists()
            assert "OMERO" in commands[-1]["stderr"]
        else:
            assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == bytes(expected)

    def wide_integer_checks():
        prefix_name = f"integer{integer_width}"
        limits = (maximum, minimum)
        raw(f"{prefix_name}-extreme-samples", x=6, width=2, expected=limits)
        if integer64:
            adjacent = -(2**53) if is_signed else 2**53
            raw("integer64-adjacent-samples", x=8, width=2, expected=(adjacent, adjacent + 1))
            beyond = -(2**53 + 1) if is_signed else 2**53 + 1
            name = "integer64-missing-fill-beyond-binary64"
            source = variant(name)
            (source / "s0/c/0/0/0/0/0").unlink()
            edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=beyond))
            raw(name, source, expected=(beyond,))
        green = crop(arrays, 0, 0, 1, 0, 6, 0, 2, 1, 1)
        expected = [component for red, value in zip((255, 0), green)
                    for component in (red, normalize_integer(value, minimum, maximum), 0, 255)]
        display_check(f"{prefix_name}-extreme-display", store, expected, x=6, y=0, width=2)
        for fill in limits:
            name = f"{prefix_name}-missing-fill-{fill}"
            source = variant(name)
            (source / "s0/c/0/0/0/0/0").unlink()
            edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=fill))
            raw(name, source, expected=(fill,))
        if args.sharded:
            name = f"{prefix_name}-missing-inner-fill"
            source = variant(name)
            edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=maximum))
            sibling = crop(arrays, 0, 0, 1, 0, 0, 0, 1, 1, 1)
            raw(name, source, channels=2, expected=(maximum, *sibling))
        for index, fill in enumerate((minimum - 1, maximum + 1, 1.25, 7.0, True, None, "7")):
            name = f"{prefix_name}-bad-fill-{index}"
            source = variant(name)
            edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=fill))
            raw(name, source, status=2, diagnostic="integer fill")
        name = f"{prefix_name}-missing-endian"
        source = variant(name)
        def remove_endian(doc):
            _, codecs = storage_codec_lists(doc)
            next(codec for codec in codecs if codec["name"] == "bytes")["configuration"].pop("endian")
        edit_json(source, "s0/zarr.json", remove_endian)
        raw(name, source, status=2, diagnostic="require bytes endian")

    def normalize_integer(value, minimum, maximum):
        width = maximum - minimum
        return ((value - minimum) * 255 + width // 2) // width

    def omero_display_checks():
        def overlap(doc):
            doc["channels"][0]["window"].update(start=0, end=300)
            doc["channels"][1]["window"].update(start=16000, end=16100)
        samples = crop(arrays, 0, 0, 0, 0, 5, 3, 1, 1, 2)
        name = "saturating-channel-composite"
        source = display_variant(name, overlap)
        expected = (255, 128, 255, 255)
        display_check(name, source, expected)
        raw("raw-after-producer-render", source, x=5, y=3, channels=2, expected=samples)

        def change_channel(doc, **fields):
            overlap(doc)
            doc["channels"][1].update(**fields)

        name = "inactive-channel"
        source = display_variant(name, lambda doc: change_channel(doc, active=False))
        display_check(name, source, (255, 0, 128, 255))
        name = "inverted-channel"
        source = display_variant(name, lambda doc: change_channel(doc, inverted=True))
        display_check(name, source, (255, 0, 128, 255))

        def no_active(doc):
            for channel in doc["channels"]:
                channel["active"] = False
        name = "no-active-channels-clipped"
        source = display_variant(name, no_active)
        display_check(name, source, (0, 0, 0, 0, 0, 0, 0, 255), x=-1, y=0, width=2)

        def label_only(doc):
            doc.pop("rdefs")
            doc["channels"] = [{"label": channel["label"]} for channel in doc["channels"]]
        name = "legacy-label-only-display"
        source = display_variant(name, label_only)
        display_check(name, source,
                      tuple((value * 255 + 32767) // 65535 for value in samples) + (0, 255))
        for name, edit in (
            ("invalid-channel-color", lambda doc: doc["channels"][0].update(color="nothex")),
            ("inverted-window-bounds", lambda doc: doc["channels"][0]["window"].update(start=65535)),
            ("unsupported-display-family", lambda doc: doc["channels"][0].update(family="logarithmic")),
            ("unsupported-display-model", lambda doc: doc["rdefs"].update(model="greyscale")),
        ):
            source = display_variant(name, edit)
            display_check(name, source, status=1)
            raw(f"{name}-raw", source, x=5, y=3, channels=2, expected=samples)

        name = "invalid-channel-label-nul"
        source = display_variant(name, lambda doc: doc["channels"][0].update(label="DAPI\x00Other"))
        raw(name, source, status=2)

    def float32_storage():
        key = "s0/c/0/0/0/0/0"
        for name, token, bits in (
            ("signed-zero-fill", "-0", 0x80000000),
            ("negative-underflow-fill", "-1e-100", 0x80000000),
            ("float32-decimal-midpoint", "1.0000000596046447753906250000000001", 0x3f800001),
            ("nan-fill", '"NaN"', 0x7fc00000),
            ("inf-fill", '"-Infinity"', 0xff800000),
            ("ieee-payload-fill", '"0x7f800001"', 0x7f800001),
        ):
            source = variant(name)
            path = source / "s0/zarr.json"
            edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value="@typed-fill@"))
            path.write_text(path.read_text(encoding="utf-8").replace('"@typed-fill@"', token), encoding="utf-8")
            if not args.sharded or name != "ieee-payload-fill":
                (source / key).unlink()
            expected = (bits, bits) if args.sharded and name != "ieee-payload-fill" else (bits, arrays[0][192])
            raw(name, source, channels=2, expected=expected)
            if name == "ieee-payload-fill":
                pam = output / "display-nonfinite-fill.pam"
                run("display-nonfinite-fill", [*prefix, *reader, source, 0, 0, 0, 1, 1, pam])
                assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == bytes(4)
        for name, fill in (("bad-float32-bits", "0x1"), ("null-float32-fill", None)):
            changed = variant(name)
            edit_json(changed, "s0/zarr.json", lambda doc: doc.update(fill_value=fill))
            raw(name, changed, status=2)
        changed = variant("mixed-pyramid-dtype")
        edit_json(changed, "s1/zarr.json", lambda doc: doc.update(data_type="uint16", fill_value=0))
        raw("mixed-pyramid-dtype", changed, status=2)
        raw("float32-clipped-region", x=-1, width=2, status=2)
        pam = output / "float32-clipped-display.pam"
        run("float32-clipped-display", [*prefix, *reader, store, -1, 0, 0, 2, 1, pam])
        assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == bytes((0, 0, 0, 0, 0, 0, 0, 255))

    def float64_storage():
        key = "s0/c/0/0/0/0/0"
        patterns = (0x8000000000000000, 1, 0x3FF0000000000001, 0x7FF0000000000042,
                    0xFFF8000000000084, 0x7FF0000000000000, 0xFFF0000000000000)
        raw("float64-special-samples", x=3, width=len(patterns), expected=patterns)
        display_check("float64-display-specials", store, (0, 0, 0, 0) * 4, x=6, y=0, width=4)
        for name, token, bits in (
            ("signed-zero", "-0", 0x8000000000000000),
            ("signed-decimal-zero", "-0.0", 0x8000000000000000),
            ("negative-underflow", "-1e-400", 0x8000000000000000),
            ("subnormal", "4.9406564584124654e-324", 1),
            ("adjacent-binary64", "1.0000000000000002", 0x3FF0000000000001),
            ("rounded-integer", "9007199254740995", 0x4340000000000002),
            ("max-finite", "1.7976931348623157e308", 0x7FEFFFFFFFFFFFFF),
            ("nan", '"NaN"', 0x7FF8000000000000),
            ("infinity", '"Infinity"', 0x7FF0000000000000),
            ("negative-infinity", '"-Infinity"', 0xFFF0000000000000),
            ("signaling-nan", '"0x7ff0000000000042"', 0x7FF0000000000042),
            ("negative-nan", '"0xFFF8000000000084"', 0xFFF8000000000084),
            ("oversized-number", "1e309", None),
            ("malformed-hex", '"0x7ff000000000004g"', None),
            ("float32-hex-width", '"0x7fc00000"', None),
            ("null", "null", None),
        ):
            case = f"float64-fill-{name}"
            source = variant(case)
            (source / key).unlink()
            metadata = source / "s0/zarr.json"
            spec = json.loads(metadata.read_text(encoding="utf8"))
            spec["fill_value"] = "FLOAT64_TOKEN"
            metadata.write_text(json.dumps(spec).replace('"FLOAT64_TOKEN"', token), encoding="utf8")
            raw(case, source, status=2 if bits is None else 0, expected=(bits,))
        if args.sharded:
            case = "float64-inner-nan-fill"
            source = variant(case)
            edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value="0x7ff0000000000042"))
            sibling = crop(arrays, 0, 0, 1, 0, 0, 0, 1, 1, 1)
            raw(case, source, channels=2, expected=(0x7FF0000000000042, *sibling))

    def alternate_codec_storage():
        key = "s0/c/0/0/0/0/0"
        member = (store / key).read_bytes()
        if args.sharded:
            index_bytes = 8 * 16 + 4
            data, index = member[:-index_bytes], member[-index_bytes:-4]
            assert struct.unpack("<I", member[-4:])[0] == crc32c(index)
            entries = list(struct.iter_unpack("<QQ", index))
            sentinel = (1 << 64) - 1
            assert entries[0] == (sentinel, sentinel)
            start, length = entries[1]
            assert 0 <= start < len(data) and 0 < length <= len(data) - start
            chunk = data[start:start + length]
            x, width, height = 3, 3, 2
        else:
            chunk = member
            x, width, height = 0, 6, 4
        samples = crop(arrays, 0, 0, 0, 0, x, 0, width, height, 1)
        expected_payload = struct.pack((">" if args.big_endian else "<") + sample_code * len(samples), *samples)
        decoded = gzip.decompress(chunk) if args.codec == "gzip" else chunk
        assert decoded == expected_payload

        def replace_chunk(source, replacement):
            if args.sharded:
                modified = entries.copy()
                modified[1] = (len(data), len(replacement))
                index = b"".join(struct.pack("<QQ", *entry) for entry in modified)
                replacement = data + replacement + index + struct.pack("<I", crc32c(index))
            (source / key).write_bytes(replacement)

        name = "absent-coded-member-fill"
        source = variant(name)
        (source / key).unlink()
        edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=fill_value))
        raw(name, source, width=3, height=2, expected=(fill_word,) * 6)
        if args.sharded:
            name = "absent-inner-coded-chunk-fill"
            source = variant(name)
            edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=fill_value))
            sibling = crop(arrays, 0, 0, 1, 0, 0, 0, 3, 2, 1)
            expected = tuple(word for sample in sibling for word in (fill_word, sample))
            raw(name, source, width=3, height=2, channels=2, expected=expected)

        for name, bad in (("short-coded-payload", chunk[:-1]), ("long-coded-payload", chunk + b"\x00"),
                          ("empty-coded-payload", b"")):
            source = variant(name)
            replace_chunk(source, bad)
            raw(name, source, x=x, status=2)
        if args.codec == "gzip":
            name = "gzip-checksum-mismatch"
            source = variant(name)
            broken = bytearray(chunk)
            broken[-8] ^= 1
            replace_chunk(source, broken)
            raw(name, source, x=x, status=2)
            for name, level in (("negative-gzip-level", -1), ("oversized-gzip-level", 10),
                                ("fractional-gzip-level", 1.25)):
                source = variant(name)
                def set_level(doc):
                    codecs = sharding_config(doc)["codecs"] if args.sharded else doc["codecs"]
                    codecs[1]["configuration"]["level"] = level
                edit_json(source, "s0/zarr.json", set_level)
                raw(name, source, x=x, status=2)

    def single_byte_metadata():
        def serializer(doc):
            _, codecs = storage_codec_lists(doc)
            return next(codec for codec in codecs if codec["name"] == "bytes")

        for name, configuration, status in (
            ("empty-single-byte-configuration", {}, 0),
            ("redundant-single-byte-endian", {"endian": "big"}, 0),
            ("invalid-single-byte-endian", {"endian": "middle"}, 2),
            ("invalid-single-byte-configuration", [], 2),
        ):
            changed = variant(name)
            edit_json(changed, "s0/zarr.json", lambda doc: serializer(doc).update(configuration=configuration))
            raw(name, changed, width=4, channels=2, status=status)
        range_case = ("below-int8-fill", -129) if args.int8 else ("negative-uint8-fill", -1)
        for name, fill in (range_case, (f"fractional-{dtype}-fill", 1.5),
                           (f"boolean-{dtype}-fill", True), (f"null-{dtype}-fill", None)):
            changed = variant(name)
            edit_json(changed, "s0/zarr.json", lambda doc: doc.update(fill_value=fill))
            raw(name, changed, status=2)
        name = f"mixed-{dtype}-pyramid"
        changed = variant(name)
        def mixed_uint16(doc):
            doc["data_type"] = "uint16"
            serializer(doc)["configuration"] = {"endian": "little"}
        edit_json(changed, "s1/zarr.json", mixed_uint16)
        raw(name, changed, status=2)

    def int8_samples():
        raw("int8-signed-endpoints", x=6, width=2, expected=(127, -128))
        green = crop(arrays, 0, 0, 1, 0, 6, 0, 2, 1, 1)
        expected = [component for red, g in zip((255, 0), green) for component in (red, g + 128, 0, 255)]
        display_check("int8-signed-display", store, expected, x=6, y=0, width=2)
        if args.sharded:
            name = "int8-signed-inner-fill"
            changed = variant(name)
            edit_json(changed, "s0/zarr.json", lambda doc: doc.update(fill_value=-128))
            sibling = crop(arrays, 0, 0, 1, 0, 0, 0, 1, 1, 1)
            raw(name, changed, channels=2, expected=(-128, *sibling))

    def int16_metadata():
        raw("signed-endpoints", x=6, width=2, expected=(32767, -32768))
        name = "maximum-int16-fill"
        changed = variant(name)
        (changed / "s0/c/0/0/0/0/0").unlink()
        edit_json(changed, "s0/zarr.json", lambda doc: doc.update(fill_value=32767))
        raw(name, changed, width=2, expected=(32767, 32767))
        for name, fill in (("below-int16-fill-minimum", -32769), ("fractional-int16-fill", -1.5),
                           ("huge-unsigned-int16-fill", (1 << 64) - 1)):
            changed = variant(name)
            edit_json(changed, "s0/zarr.json", lambda doc: doc.update(fill_value=fill))
            raw(name, changed, status=2)
        name = "missing-int16-endian"
        changed = variant(name)
        def remove_endian(doc):
            _, _, codecs = storage_profile(doc)
            del codecs[0]["configuration"]["endian"]
        edit_json(changed, "s0/zarr.json", remove_endian)
        raw(name, changed, status=2)
        name = "mixed-int16-pyramid"
        changed = variant(name)
        edit_json(changed, "s1/zarr.json", lambda doc: doc.update(data_type="uint16"))
        raw(name, changed, status=2)

    def transpose_checks():
        def pipeline(doc):
            outer, inner = storage_codec_lists(doc)
            return outer if args.transpose == "shard" else inner

        def split_order(doc):
            codecs = pipeline(doc)
            original = codecs[0]["configuration"]["order"]
            first = [1, 0, 2, 3, 4]
            second = [first.index(axis) for axis in original]
            codecs[0]["configuration"]["order"] = first
            codecs.insert(1, {"name": "transpose", "configuration": {"order": second}})

        name = "equivalent-composed-transposes"
        source = variant(name)
        edit_json(source, "s0/zarr.json", split_order)
        raw(name, source, x=2, y=1, width=7, height=6, channels=2)

        name = "transposed-absent-chunk-fill"
        source = variant(name)
        chunk_key = "s0/c/0/0/0/0/0"
        (source / chunk_key).unlink()
        edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=fill_value))
        raw(name, source, width=3, height=2, expected=(fill_word,) * 6)

        for name, order in (("transpose-duplicate-axis", [0, 0, 1, 2, 3]),
                            ("transpose-negative-axis", [-1, 0, 1, 2, 3]),
                            ("transpose-axis-outside-rank", [5, 0, 1, 2, 3]),
                            ("transpose-fractional-axis", [0.5, 0, 1, 2, 3]),
                            ("transpose-short-order", [0, 1, 2, 3])):
            source = variant(name)
            edit_json(source, "s0/zarr.json",
                      lambda doc: pipeline(doc)[0]["configuration"].update(order=order))
            raw(name, source, status=2)

        def move_after_serialization(doc):
            codecs = pipeline(doc)
            codecs.insert(1, codecs.pop(0))
        name = "transpose-after-serialization"
        source = variant(name)
        edit_json(source, "s0/zarr.json", move_after_serialization)
        raw(name, source, status=2)

        name = "truncated-transposed-member"
        source = variant(name)
        member = source / chunk_key
        member.write_bytes(member.read_bytes()[:-1])
        raw(name, source, x=3, status=2)

    def crc32c_checks():
        key = "s0/c/0/0/0/0/0"
        member = (store / key).read_bytes()
        x = 0
        if args.sharded:
            footer_size = 8 * 16 + 4
            payload, index = member[:-footer_size], member[-footer_size:-4]
            assert crc32c(index) == struct.unpack("<I", member[-4:])[0]
            entries = list(struct.iter_unpack("<QQ", index))
            offset, length = entries[1]
            assert length > 0 and offset + length <= len(payload)
            stored = payload[offset:offset + length]
            x = 3
        else:
            stored = member

        def check_trailer(chunk):
            assert len(chunk) > 4
            assert crc32c(chunk[:-4]) == struct.unpack("<I", chunk[-4:])[0]

        def rewrite_chunk(source, content):
            if args.sharded:
                updated = entries.copy()
                updated[1] = (len(payload), len(content))
                index = b"".join(struct.pack("<QQ", *entry) for entry in updated)
                content = payload + content + index + struct.pack("<I", crc32c(index))
            (source / key).write_bytes(content)

        if args.crc32c == "before":
            assert args.codec == "gzip"
            plain = gzip.decompress(stored)
            check_trailer(plain)
            bad = bytearray(plain)
            bad[0] ^= 1
            corrupt = gzip.compress(bytes(bad), mtime=0)
            diagnostic = "decoded chunk CRC32C mismatch"
        else:
            check_trailer(stored)
            bad = bytearray(stored)
            bad[-1] ^= 1
            corrupt = bytes(bad)
            diagnostic = "encoded chunk CRC32C mismatch" if args.crc32c == "both" else "decoded chunk CRC32C mismatch"

        name = "bad-chunk-crc32c"
        source = variant(name)
        rewrite_chunk(source, corrupt)
        raw(name, source, x=x, status=2, diagnostic=diagnostic)
        name = "truncated-chunk-crc32c"
        source = variant(name)
        rewrite_chunk(source, stored[:3])
        raw(name, source, x=x, status=2)

        name = "absent-crc32c-member-uses-fill"
        source = variant(name)
        (source / key).unlink()
        edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=fill_value))
        raw(name, source, width=3, height=2, expected=(fill_word,) * 6)
        if args.sharded:
            name = "absent-crc32c-inner-uses-fill"
            source = variant(name)
            edit_json(source, "s0/zarr.json", lambda doc: doc.update(fill_value=fill_value))
            sibling = crop(arrays, 0, 0, 1, 0, 0, 0, 3, 2, 1)
            expected = tuple(word for sample in sibling for word in (fill_word, sample))
            raw(name, source, width=3, height=2, channels=2, expected=expected)

        def invalid_parameters(doc):
            _, inner = storage_codec_lists(doc)
            crc = next(codec for codec in inner if codec["name"] == "crc32c")
            crc["configuration"] = {"unexpected": 1}
        def duplicate_crc(doc):
            _, inner = storage_codec_lists(doc)
            index = next(i for i, codec in enumerate(inner) if codec["name"] == "crc32c")
            inner.insert(index, {"name": "crc32c"})
        for name, edit in (("crc32c-parameters-refused", invalid_parameters),
                           ("duplicate-crc32c-refused", duplicate_crc)):
            source = variant(name)
            edit_json(source, "s0/zarr.json", edit)
            raw(name, source, status=2)
        if args.sharded:
            name = "whole-shard-crc32c-refused"
            source = variant(name)
            edit_json(source, "s0/zarr.json", lambda doc: doc["codecs"].append({"name": "crc32c"}))
            raw(name, source, status=2)

    passed = False
    try:
        plain = json.loads(run("catalog-without-plugin", [region, "--readers"]))
        assert all(item["provider_id"] != PROVIDER for item in plain["readers"])
        catalog = json.loads(run("catalog", [*prefix, "--readers"]))
        provider, = (item for item in catalog["readers"] if item["provider_id"] == PROVIDER)
        assert provider["available"] and provider["display_rgba"] and provider["raw_samples"]
        assert provider["contract"] == contract and provider["module_sha256"] == entry["module"]["sha256"]

        for level, geometry in enumerate(LEVELS):
            for t in range(2):
                for z in range(2):
                    raw(f"full-l{level}-t{t}-z{z}", level=level, t=t, z=z, width=geometry["width"],
                        height=geometry["height"], channels=2)
        raw("chunk-seam-xy", t=1, c=1, z=1, x=5, y=3, width=3, height=3)
        raw("pyramid-odd-crop", level=1, t=1, c=1, x=3, y=3, width=3, height=2)
        raw("pyramid-odd-final-pixel", level=1, t=1, c=1, z=1, x=11, y=7)
        raw("pyramid-even-final-pixel", level=1, t=1, c=1, z=1, x=10, y=6)
        for level, x, y in ((0, 5, 3), (1, 3, 3)):
            pam = output / f"display-level-{level}.pam"
            run(f"display-level-{level}", [*prefix, *reader, store, x, y, level, 3, 2, pam])
            header, encoded = pam.read_bytes().split(b"ENDHDR\n", 1)
            assert b"WIDTH 3\nHEIGHT 2\nDEPTH 4\n" in header
            # Integer-ratio equivalent of nearest-byte normalization over UInt16.
            source_values = crop(arrays, level, 0, 0, 0, x, y, 3, 2, 2)
            if args.omero_display:
                expected = bytes(component for pixel in zip(source_values[::2], source_values[1::2])
                                 for component in omero_colors(producer_display["channels"], pixel))
                assert encoded == expected, pam
                continue
            if args.float32 or args.float64:
                word, value = ("<I", "<f") if args.float32 else ("<Q", "<d")
                values = [struct.unpack(value, struct.pack(word, bits))[0] for bits in source_values]
                projected = [math.floor(min(1, max(0, value)) * 255 + .5) for value in values]
            elif args.uint8:
                projected = source_values
            elif args.int8:
                projected = [value + 128 for value in source_values]
            elif wide_integer:
                projected = [normalize_integer(value, minimum, maximum) for value in source_values]
            elif args.int16:
                projected = [(((value + 32768) * 255 + 32767) // 65535) for value in source_values]
            else:
                projected = [((value * 255 + 32767) // 65535) for value in source_values]
            expected = bytes(component for r, g in zip(projected[::2], projected[1::2])
                             for component in (r, g, 0, 255))
            assert encoded == expected, pam

        if args.crc32c:
            crc32c_checks()
            if args.transpose:
                transpose_checks()
            if args.float32:
                float32_storage()
        elif args.transpose:
            transpose_checks()
            if args.float32:
                float32_storage()
        elif args.codec in ("bytes", "gzip"):
            alternate_codec_storage()
            if args.float32:
                float32_storage()
        elif args.float32:
            float32_storage()
        elif args.codec == "blosc":
            # The compressed-frame harness covers storage failures and ensures
            # the fixtures reach the real decoder, beyond small memcpy frames.
            pass
        elif args.sharded:
            check_shards(store, receipt, arrays, raw, variant, edit_json)
        else:
            chunk_key = "s0/c/0/0/0/0/0"
            encoded = (store / chunk_key).read_bytes()
            missing = variant("missing-member")
            (missing / chunk_key).unlink()
            edit_json(missing, "s0/zarr.json", lambda doc: doc.update(fill_value=integer_fill))
            # The other channel occupies a different chunk and must stay exact.
            c1 = crop(arrays, 0, 0, 1, 0, 1, 1, 2, 2, 1)
            fill = tuple(value for sample in c1 for value in (integer_fill, sample))
            raw("declared-fill", missing, x=1, y=1, width=2, height=2, channels=2, expected=fill)
            for name, bad in (("truncated-frame", encoded[:-1]), ("trailing-frame", encoded + encoded),
                              ("empty-chunk", b"")):
                broken = variant(name)
                (broken / chunk_key).write_bytes(bad)
                raw(name, broken, status=2)

        if single_byte:
            single_byte_metadata()
        if args.int8:
            int8_samples()
        if args.int16:
            int16_metadata()
        if wide_integer:
            wide_integer_checks()
        if args.float64:
            float64_storage()
        if args.omero_display:
            omero_display_checks()

        def pyramid_dataset(doc, index):
            return doc["attributes"]["ome"]["multiscales"][0]["datasets"][index]

        for name, key, edit in (
            ("future-version", "zarr.json", lambda doc: doc["attributes"]["ome"].update(version="0.6")),
            ("wrong-array-axis", "s1/zarr.json", lambda doc: doc.update(dimension_names=list("ctzyx"))),
            ("shifted-pyramid", "zarr.json", lambda doc: pyramid_dataset(doc, 1)["coordinateTransformations"][1]
             ["translation"].__setitem__(4, 1.25)),
            ("path-escape", "zarr.json", lambda doc: pyramid_dataset(doc, 0).update(path="../s0")),
            ("unsupported-codec", "s0/zarr.json", lambda doc: doc["codecs"][0].update(name="gzip")),
            ("oversized-chunk", "s0/zarr.json", lambda doc: doc["chunk_grid"]["configuration"]
             ["chunk_shape"].__setitem__(4, 1 << 30)),
            ("oversized-fill", "s0/zarr.json",
             lambda doc: doc.update(fill_value=maximum + 1 if wide_integer else None if args.float64 else
                                    1e50 if args.float32 else 256 if args.uint8 else 128 if args.int8 else
                                    32768 if args.int16 else 65536)),
        ):
            changed = variant(name)
            edit_json(changed, key, edit)
            raw(name, changed, status=2)
        raw("invalid-plane", z=2, status=2)
        raw("invalid-channel", c=1, channels=2, status=2)

        # Output must never become a new scientific input member of the store.
        protected = variant("protected-store")
        inside = protected / "output.npy"
        run("output-inside-source", [*prefix, *reader, "--raw", protected, inside], 2)
        assert not inside.exists()
        verify_inventory(protected, members)
        verify_inventory(store, members)
        passed = True
    finally:
        (output / "results.json").write_text(json.dumps({"passed": passed, "sharded": args.sharded, "float32": args.float32,
                                                         "data_type": dtype,
                                                         "omero_display": args.omero_display,
                                                         "transpose": args.transpose,
                                                         "crc32c": args.crc32c,
                                                         "codec": args.codec, "endian": endian,
                                                         "fixture_sha256": sha(fixture / "producer.json"),
                                                         "region_sha256": sha(region), "manifest_sha256": sha(manifest),
                                                         "module_sha256": sha(module), "commands": commands},
                                                        indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(commands)} native NGFF 0.5 commands")


if __name__ == "__main__":
    main()
