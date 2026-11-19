#!/usr/bin/env python3
"""Author FITS image fixtures and check the native ABI, project and CLI."""

import argparse
import gzip
import hashlib
from io import BytesIO
import json
import math
from pathlib import Path
import struct
import subprocess

import astropy
from astropy.io import fits
from astropy.io.fits.hdu.compressed._codecs import Rice1
import numpy as np


def integer_values(dtype):
    limits = np.iinfo(dtype)
    if limits.min < 0:
        return (limits.min, limits.min + 1, -257, -129, -1, 0, 1, 127, 256, limits.max - 1, limits.max)
    return (0, 1, 2, 127, 129, 251, limits.max // 2, limits.max // 2 + 1, limits.max - 1, limits.max)


# Each entry has BITPIX, exported sample ID, literal storage dtype, exported dtype.
TYPES = {
    "u8": (8, 1, "u1", "u1"),
    "i16": (16, 4, "i2", "i2"),
    "u16": (16, 2, "u2", "u2"),
    "i32": (32, 6, "i4", "i4"),
    "u32": (32, 5, "u4", "u4"),
    "i64": (64, 8, "i8", "i8"),
    "u64": (64, 7, "u8", "u8"),
    "f32": (-32, 9, "u4", "f4"),
    "f64": (-64, 10, "u8", "f8"),
}
FLOAT_BITS = {
    "f32": (0, 0x80000000, 0x3f800000, 0xbf800000, 0x3e800000, 0xbe800000,
            0x7f800000, 0xff800000, 0x7fc01234, 0xffc05678, 0x7fa12345, 0xffa54321,
            1, 0x80000001, 0x40000000, 0xc0000000, 0x7f7fffff, 0xff7fffff),
    "f64": (0, 0x8000000000000000, 0x3ff0000000000000, 0xbff0000000000000,
            0x3fd0000000000000, 0xbfd0000000000000, 0x7ff0000000000000, 0xfff0000000000000,
            0x7ff8000000001234, 0xfff8000000005678, 0x7ff0000000a12345, 0xfff0000000a54321,
            1, 0x8000000000000001, 0x4000000000000000, 0xc000000000000000, 0x7fefffffffffffff,
            0xffefffffffffffff),
}
BLOCK = 2880


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def card(keyword, value=None, comment=""):
    assert len(keyword) <= 8
    if value is None:
        body = keyword.ljust(8) + comment
    else:
        body = keyword.ljust(8) + "= " + str(value).rjust(20)
        if comment:
            body += " / " + comment
    assert len(body) <= 80
    return body.encode("ascii").ljust(80, b" ")


def pad(data, value):
    return data + value * ((-len(data)) % BLOCK)


def write_bytes(cards, payload):
    return pad(b"".join(cards) + card("END"), b" ") + pad(payload, b"\0")


def raw_codes(kind, shape):
    _, _, storage, _ = TYPES[kind]
    values = FLOAT_BITS[kind] if kind in FLOAT_BITS else integer_values(storage)
    if kind == "u64":
        values += (2**53 + 1, 2**63 + 1)
    if kind == "i64":
        values += (-2**53 - 1, 2**53 + 1)
    length = math.prod(shape)
    return np.array([values[(i + i // 7) % len(values)] for i in range(length)],
                    dtype=storage).reshape(shape)


def display_bytes(array, kind, valid, window):
    result = bytearray()
    _, _, storage, dtype = TYPES[kind]
    if window is None:
        stats = np.iinfo(storage) if kind not in FLOAT_BITS else None
        window = (stats.min, stats.max) if stats else (-1, 1)
    low, high = map(float, window)
    for bits, accepted in zip(array[0].ravel(), valid[0].ravel()):
        if not accepted:
            result.extend(bytes(4))
            continue
        number = float(bits) if kind not in FLOAT_BITS else bits.view(np.dtype(dtype)).item()
        if not math.isfinite(number):
            result.extend(bytes(4))
            continue
        normalized = min(1., max(0., (number - low) / (high - low)))
        gray = math.floor(normalized * 255 + .5)
        result.extend((gray, gray, gray, 255))
    return bytes(result)


def stored_codes(array, kind):
    bitpix, _, storage, _ = TYPES[kind]
    if bitpix > 8 and kind.startswith("u"):
        sign = np.array(1 << (bitpix - 1), dtype=storage)
        return (array ^ sign).view(np.dtype(f"i{bitpix // 8}"))
    return array


def valid_codes(array, kind, blank):
    if kind in FLOAT_BITS:
        # Determine NaN from its bits to avoid evaluating signaling payloads.
        if kind == "f32":
            return np.logical_or((array & 0x7f800000) != 0x7f800000, (array & 0x7fffff) == 0)
        return np.logical_or((array & 0x7ff0000000000000) != 0x7ff0000000000000,
                             (array & 0xfffffffffffff) == 0)
    return np.ones(array.shape, dtype=bool) if blank is None else stored_codes(array, kind) != blank


def authored(array, kind, dimensions, blank, extras, scaling=None):
    bitpix, _, _, _ = TYPES[kind]
    cards = [card("SIMPLE", "T"), card("BITPIX", bitpix), card("NAXIS", dimensions)]
    cards += [card(f"NAXIS{i+1}", size) for i, size in enumerate(reversed(array.shape[-dimensions:]))]
    if scaling is not None:
        cards += [card("BSCALE", scaling[0]), card("BZERO", scaling[1])]
    elif bitpix > 8 and kind.startswith("u"):
        cards += [card("BSCALE", 1), card("BZERO", 1 << (bitpix - 1))]
    if blank is not None:
        cards.append(card("BLANK", blank))
    stored = stored_codes(array, kind)
    wire = stored.astype(stored.dtype.newbyteorder(">")).tobytes()
    return cards + list(extras), wire


def check_reference(path, array, kind, dimensions, blank, authored_scaling, hdu):
    bitpix, _, storage, dtype = TYPES[kind]
    with fits.open(path, do_not_scale_image_data=True, memmap=False) as hdus:
        hdus.verify("exception")
        assert (len(hdus) == 1 if hdu is None else hdu < len(hdus))
        selected = hdus[0 if hdu is None else hdu]
        assert selected.header["BITPIX"] == bitpix
        expected = stored_codes(array, kind)
        data = selected.data.reshape(array.shape)
        order = data.dtype.byteorder if isinstance(selected, fits.CompImageHDU) else ">"
        wire_bits = data.view(expected.dtype.newbyteorder(order))
        assert wire_bits.astype(expected.dtype).tobytes() == expected.tobytes(), path.name
        assert selected.header["NAXIS"] == dimensions and selected.header.get("BLANK") == blank, path.name
    if blank is None and kind not in FLOAT_BITS and authored_scaling is None:
        with fits.open(path, uint=True, memmap=False) as hdus:
            interpreted = hdus[0 if hdu is None else hdu].data.reshape(array.shape)
            assert interpreted.dtype.kind == np.dtype(dtype).kind
            assert interpreted.dtype.itemsize == np.dtype(dtype).itemsize
            assert interpreted.astype(storage).tobytes() == array.tobytes(), path.name


def write_fixtures(directory, tile_baseline=None, rice_baseline=None, gzip2_baseline=None, volume_baseline=None):
    assert astropy.__version__ == "7.2.0", "Use the admitted Astropy development oracle"
    directory.mkdir(parents=True, exist_ok=True)
    cases, negative = {}, {}

    def add(name, kind, *, shape=(2, 3, 5), dimensions=3, producer=False, blank=None,
            extras=(), scaling=None, window=None, contents=None, hdu=None, compressed=False, array=None):
        array = raw_codes(kind, shape) if array is None else array.reshape(shape)
        path = directory / (name + (".fits.gz" if compressed else ".fits"))
        cards, wire = authored(array, kind, dimensions, blank, extras, scaling)
        if contents is not None:
            path.write_bytes(gzip.compress(contents, mtime=0) if compressed else contents)
        elif producer:
            fits.PrimaryHDU(array if dimensions == 3 else array[0]).writeto(path)
        else:
            path.write_bytes(write_bytes(cards, wire))
        check_reference(path, array, kind, dimensions, blank, scaling, hdu)
        expected_valid = valid_codes(array, kind, blank)
        # Raw ABI represents undefined BLANK/NaN as zero with validity=0.
        # The separately checked stored code oracle still sees their exact bits.
        exported = array.copy()
        exported[~expected_valid] = 0
        raw = directory / (name + ".raw")
        raw.write_bytes(exported.tobytes())
        valid = directory / (name + ".valid")
        valid.write_bytes(expected_valid.astype(np.uint8).tobytes())
        rgba = display_bytes(array, kind, expected_valid, window)
        (directory / (name + ".rgba")).write_bytes(rgba)
        if all(value == 255 for value in rgba[3::4]):
            (directory / (name + ".rgb")).write_bytes(bytes(value for i, value in enumerate(rgba) if i%4!=3))
        _, type_id, storage, dtype = TYPES[kind]
        options = {}
        if hdu is not None:
            options["hdu"] = hdu
        if window is not None:
            options["window"] = window
        cases[name] = {
            "name":name, "filename":path.name, "width":shape[2], "height":shape[1], "z_planes":shape[0],
            "sample_type":type_id, "sample_bytes":np.dtype(storage).itemsize, "dtype":dtype,
            "dimension_order":"XYZ" if dimensions == 3 else "XY",
            "calibration_flags":0, "pixel_width_um":0, "pixel_height_um":0, "z_spacing_um":0,
            "option":json.dumps(options,separators=(",",":")) if options else "-",
            "source_sha256":sha256(path), "raw_sha256":sha256(raw), "valid_sha256":sha256(valid),
            "astropy_verified":True,
        }
        return cards, wire

    for kind in TYPES:
        add(kind+"-volume",kind)
        add(kind+"-image",kind,shape=(1,3,5),dimensions=2)

    main, wire = add("i16-main","i16",producer=True)
    add("i16-window","i16",window=(-128,127))
    add("u16-producer","u16",producer=True)
    add("u64-producer","u64",producer=True)
    add("i16-blank","i16",blank=-257)
    add("u16-blank","u16",blank=-32768)
    add("u64-blank","u64",blank=-9223372036854775808)
    add("u64-equivalent-offset","u64",scaling=("1.000D0","92233720368547758080D-1"))
    add("comment-history","u8",extras=(card("COMMENT",comment="Fixture commentary"),card("HISTORY",comment="Origin"),card("")))
    add("multiblock-header","u16",extras=tuple(card("COMMENT",comment=f"Card {i}") for i in range(75)))
    add("budget-input","u8",shape=(2,255,137))

    def bundle_bytes(*hdus):
        stream = BytesIO()
        fits.HDUList(list(hdus)).writeto(stream)
        return stream.getvalue()

    def checksummed(*hdus):
        for hdu in hdus:
            hdu.add_checksum(when="authored fixture")
        data = bundle_bytes(*hdus)
        with fits.open(BytesIO(data)) as checked:
            assert all(hdu.verify_checksum() == hdu.verify_datasum() == 1 for hdu in checked)
        return data

    checked_primary = checksummed(fits.PrimaryHDU(raw_codes("i16", (2, 3, 5))))
    add("checked-primary", "i16", contents=checked_primary)
    add("gzip-primary", "i16", contents=checked_primary, compressed=True)
    image_shape = (1, 3, 5)
    datum = raw_codes("u8", image_shape)[0]
    data_hdu = fits.PrimaryHDU(datum)
    data_hdu.add_datasum(when="authored fixture")
    data_hdu.header["DATASUM"] = "  000" + str(data_hdu.header["DATASUM"])
    datasum_only = bundle_bytes(data_hdu)
    with fits.open(BytesIO(datasum_only)) as checked:
        assert checked[0].verify_datasum() == 1 and "CHECKSUM" not in checked[0].header
    add("datasum-only", "u8", shape=image_shape, dimensions=2, contents=datasum_only)
    checksum_hdu = fits.PrimaryHDU(datum)
    checksum_hdu.add_checksum(when="authored fixture", override_datasum=True)
    checksum_only = bundle_bytes(checksum_hdu)
    assert b"DATASUM" not in checksum_only[:BLOCK]
    # Astropy's verification assumes a missing DATASUM means no data. Sum
    # the physical words independently for this CHECKSUM-only fixture.
    total = sum(int.from_bytes(checksum_only[i:i+4], "big") for i in range(0, len(checksum_only), 4))
    while total >> 32:
        total = (total & 0xffffffff) + (total >> 32)
    assert total == 0xffffffff
    add("checksum-only", "u8", shape=image_shape, dimensions=2, contents=checksum_only)
    add("blank-sums", "u8", shape=image_shape, dimensions=2,
        extras=(card("CHECKSUM", "'        '"), card("DATASUM", "'        '")))
    large_shape = (2, 255, 137)
    large = fits.PrimaryHDU(raw_codes("u8", large_shape))
    for index in range(75):
        large.header.add_history(f"Checked history {index}")
    checked_large = checksummed(large)
    add("checked-large", "u8", shape=large_shape, contents=checked_large)
    add("gzip-large", "u8", shape=large_shape, contents=checked_large, compressed=True)

    def image_extension(kind, name):
        array = raw_codes(kind, (2, 3, 5)).view(TYPES[kind][3])
        hdu = fits.ImageHDU(array, name=name)
        hdu.header["EXTVER"] = 2
        return hdu

    bundle = bundle_bytes(fits.PrimaryHDU(), image_extension("i16", "SCI"),
                          image_extension("f32", "AUX/SCI"))
    assert len(bundle) == 5 * BLOCK
    checked_bundle = checksummed(fits.PrimaryHDU(), image_extension("i16", "SCI"),
                                 image_extension("f32", "AUX/SCI"))
    add("bundle-hdu1", "i16", contents=checked_bundle, hdu=1)
    add("bundle-hdu2", "f32", contents=checked_bundle, hdu=2)
    add("bundle-window", "i16", contents=checked_bundle, hdu=1, window=(-128, 127))

    def quality_table():
        return fits.BinTableHDU.from_columns([fits.Column(name="QUALITY", format="J", array=[7, 11])])

    def heap_table():
        values = np.empty(2, dtype=object)
        values[:] = [np.array([3, 4], dtype="i2"), np.array([20], dtype="i2")]
        hdu = fits.BinTableHDU.from_columns([fits.Column(name="EVENTS", format="PI()", array=values)])
        hdu.header["THEAP"] = hdu.header["NAXIS1"] * hdu.header["NAXIS2"] + 8
        # Astropy finalizes PCOUNT when it first serializes the variable heap.
        bundle_bytes(fits.PrimaryHDU(), hdu)
        return hdu

    mixed_bundle = checksummed(fits.PrimaryHDU(), quality_table(),
                              image_extension("i16", "SCI"), heap_table(),
                              image_extension("f32", "AUX"))
    with fits.open(BytesIO(mixed_bundle)) as mixed:
        heap_block = mixed[3].fileinfo()["hdrLoc"] // BLOCK
        heap_data = mixed[3].fileinfo()["datLoc"]
        heap_start = mixed[3].header["THEAP"]
        assert mixed[3].header["PCOUNT"] > 8 and mixed[3].data["EVENTS"][0].tolist() == [3, 4]
    add("table-before-hdu2", "i16", contents=mixed_bundle, hdu=2)
    add("table-after-heap-hdu4", "f32", contents=mixed_bundle, hdu=4)

    def ascii_table(rows):
        hdu = fits.TableHDU.from_columns([
            fits.Column(name="QUALITY", format="I3", array=np.arange(rows) % 100)
        ])
        # Materialize Astropy's ASCII record bytes before adding fixed comments to the sums.
        bundle_bytes(fits.PrimaryHDU(), hdu)
        return hdu

    ascii_bundle = checksummed(fits.PrimaryHDU(), ascii_table(1), image_extension("i16", "SCI"),
                               ascii_table(1001), image_extension("f32", "AUX"))
    with fits.open(BytesIO(ascii_bundle)) as mixed:
        ascii_data = mixed[1].fileinfo()["datLoc"]
        assert mixed[1].header["NAXIS1"] == 3 and mixed[1].data["QUALITY"].tolist() == [0]
        assert mixed[3].data["QUALITY"][-1] == 0
        assert ascii_bundle[ascii_data + 3:ascii_data + BLOCK] == b" " * (BLOCK - 3)
    add("ascii-before-hdu2", "i16", contents=ascii_bundle, hdu=2)
    add("ascii-after-hdu4", "f32", contents=ascii_bundle, hdu=4, compressed=True)
    gzip_with_table = checksummed(fits.PrimaryHDU(), image_extension("i16", "SCI"),
                                 image_extension("f32", "AUX/SCI"), quality_table(), ascii_table(3))
    add("gzip-bundle-hdu1", "i16", contents=gzip_with_table, hdu=1, compressed=True)
    add("gzip-bundle-hdu2", "f32", contents=gzip_with_table, hdu=2, compressed=True)
    add("bundle-hdu0", "u16",
        contents=bundle_bytes(fits.PrimaryHDU(raw_codes("u16", (2, 3, 5))), image_extension("i16", "SCI")), hdu=0)
    add("bundle-hdu63", "f32", contents=bundle[:BLOCK] + bundle[BLOCK:3*BLOCK] * 62 + bundle[3*BLOCK:], hdu=63)

    shape = (1, 7, 9)
    tile_codes = (np.arange(63, dtype=np.int16).reshape(7, 9) * 211 - 7000)
    tiled = bundle_bytes(fits.PrimaryHDU(), fits.CompImageHDU(tile_codes, compression_type="GZIP_1",
                                                             tile_shape=(3, 4), name="SCI"))
    def retained_gzip_tiles(generated, baseline, digest, tile_count=9):
        if baseline is None:
            return generated
        original = baseline.read_bytes()
        assert hashlib.sha256(original).hexdigest() == digest

        def ignore_member_times(contents):
            result = bytearray(contents)
            for index in range(tile_count):
                _, offset = struct.unpack_from(">ii", contents, 2 * BLOCK + 8 * index)
                start = 2 * BLOCK + 8 * tile_count + offset
                assert result[start:start + 2] == b"\x1f\x8b"
                result[start + 4:start + 8] = bytes(4)
            return result
        assert ignore_member_times(generated) == ignore_member_times(original)
        return original

    tiled = retained_gzip_tiles(tiled, tile_baseline, "75d37c84be684720bb9117dcbd4685bb7f8ac08d4892221c8e4360bed4086b49")
    add("gzip-int16-tiles", "i16", shape=shape, dimensions=2, contents=tiled, hdu=1, array=tile_codes)
    tiled_unsigned = raw_codes("u16", shape)
    tiled_blank = fits.CompImageHDU(tiled_unsigned[0], compression_type="GZIP_1", tile_shape=(3, 4),
                                  header=fits.Header({"BLANK": -32768}))
    add("gzip-tile-unsigned-blank", "u16", shape=shape, dimensions=2, hdu=1, blank=-32768,
        contents=bundle_bytes(fits.PrimaryHDU(), tiled_blank), array=tiled_unsigned, compressed=True)

    # Use a wide descriptor with a gap before the heap and a full-width row tile.
    q_values = raw_codes("i16", (1, 1, 5))
    q_member = gzip.compress(q_values.astype(">i2").tobytes(), mtime=0)
    q_gap = b"\0" * 19
    q_cards = [card("XTENSION", "'BINTABLE'"), card("BITPIX", 8), card("NAXIS", 2),
               card("NAXIS1", 16), card("NAXIS2", 1), card("PCOUNT", len(q_gap) + len(q_member)),
               card("GCOUNT", 1), card("TFIELDS", 1), card("THEAP", 16 + len(q_gap)),
               card("ZIMAGE", "T"), card("ZBITPIX", 16), card("ZNAXIS", 2), card("ZNAXIS1", 5),
               card("ZNAXIS2", 1), card("ZTILE1", 5), card("ZTILE2", 1),
               card("ZCMPTYPE", "'GZIP_1'"), card("TTYPE1", "'COMPRESSED_DATA'"),
               card("TFORM1", f"'1QB({len(q_member)})'")]
    q_payload = struct.pack(">qq", len(q_member), 0) + q_gap + q_member
    wide = bundle_bytes(fits.PrimaryHDU()) + write_bytes(q_cards, q_payload)
    add("gzip-tile-wide-descriptor", "i16", shape=(1, 1, 5), dimensions=2, hdu=1, contents=wide, array=q_values)

    def rice_file(array, tile_shape, *, name="SCI", header=None):
        return bundle_bytes(fits.PrimaryHDU(), fits.CompImageHDU(array, tile_shape=tile_shape, name=name, header=header))

    rice = rice_file(tile_codes, (3, 4))
    assert hashlib.sha256(rice).hexdigest() == "dfab6e2f35660a2f5322272f5170a13cc656167654d71b9060eeb0bfa3c0d629"
    if rice_baseline is not None:
        assert rice == rice_baseline.read_bytes()
    add("rice-int16-tiles", "i16", shape=shape, dimensions=2, contents=rice, hdu=1, array=tile_codes)
    add("rice-unsigned-blank", "u16", shape=shape, dimensions=2, hdu=1, blank=-32768,
        contents=rice_file(tiled_unsigned[0], (3, 4), header=fits.Header({"BLANK": -32768})), array=tiled_unsigned)
    long_shape = (1, 3, 97)
    fixed_codes = np.full(long_shape, -768, dtype=np.int16)
    add("rice-constant", "i16", shape=long_shape, dimensions=2, hdu=1,
        contents=rice_file(fixed_codes[0], (2, 65)), array=fixed_codes)
    u8_noise = np.random.default_rng(77).integers(0, 256, size=long_shape, dtype=np.uint8)
    add("rice-direct-u8", "u8", shape=long_shape, dimensions=2, hdu=1,
        contents=rice_file(u8_noise[0], (2, 65)), array=u8_noise)
    i32_noise = np.random.default_rng(78).integers(-(2**31), 2**31, size=long_shape, dtype=np.int32)
    rice_i32 = rice_file(i32_noise[0], (2, 65))
    for key in ("ZNAME1", "ZNAME2", "ZVAL1", "ZVAL2"):
        start = rice_i32.index(key.encode().ljust(8), BLOCK, 2 * BLOCK)
        rice_i32 = rice_i32[:start] + card("COMMENT", comment="Default Rice parameter") + rice_i32[start+80:]
    add("rice-defaults-i32", "i32", shape=long_shape, dimensions=2, hdu=1, contents=rice_i32, array=i32_noise)
    custom_values = raw_codes("i16", (1, 1, 19))
    custom = Rice1(blocksize=7, bytepix=2, tilesize=19).encode(custom_values)
    replacements = {"PCOUNT": len(q_gap) + len(custom), "ZNAXIS1": 19, "ZTILE1": 19,
                    "ZCMPTYPE": "'RICE_1'", "TFORM1": f"'1QB({len(custom)})'"}
    custom_cards = []
    for original_card in q_cards:
        key = original_card[:8].decode().strip()
        custom_cards.append(card(key, replacements[key]) if key in replacements else original_card)
    custom_cards.extend((card("ZNAME1", "'BYTEPIX'"), card("ZVAL1", 2),
                         card("ZNAME2", "'BLOCKSIZE'"), card("ZVAL2", 7)))
    custom_payload = struct.pack(">qq", len(custom), 0) + q_gap + custom
    add("rice-custom-block", "i16", shape=(1, 1, 19), dimensions=2, hdu=1, array=custom_values,
        contents=bundle_bytes(fits.PrimaryHDU()) + write_bytes(custom_cards, custom_payload))

    def gzip_tiles(values, codec="GZIP_2", quantize_level=0, header=None, tile_shape=(3, 4)):
        return bundle_bytes(fits.PrimaryHDU(), fits.CompImageHDU(values, compression_type=codec,
            quantize_level=quantize_level, tile_shape=tile_shape, name="SCI", header=header))

    float_values = (np.arange(63, dtype=np.float32).reshape(7, 9) - np.float32(31)) / np.float32(1024)
    float_values[1, 2] = np.float32(-0.0)
    float_values[6, 8] = np.float32(2 ** -149)
    gzip2 = retained_gzip_tiles(gzip_tiles(float_values), gzip2_baseline,
                                "77b6ab20846a96bd1d80067a54d2efee0e1496ac8821f1222f511152610aadbe")
    add("gzip2-float32-tiles", "f32", shape=shape, dimensions=2, hdu=1, contents=gzip2,
        array=float_values.view("u4"))
    doubles = raw_codes("f64", shape)
    for idx, codec in enumerate(("GZIP_1", "GZIP_2"), 1):
        content = gzip_tiles(doubles[0].view("f8"), codec)
        if idx == 1:
            start = content.index(b"ZQUANTIZ", BLOCK, 2 * BLOCK)
            content = content[:start] + card("ZQUANTIZ", "'NONE'") + content[start+80:]
        add(f"gzip{idx}-float64-tiles", "f64", shape=shape, dimensions=2, hdu=1, contents=content, array=doubles)
    add("gzip2-unsigned-blank", "u16", shape=shape, dimensions=2, hdu=1, blank=-32768,
        contents=gzip_tiles(tiled_unsigned[0], header=fits.Header({"BLANK": -32768})), array=tiled_unsigned)
    byte_codes = raw_codes("u8", shape)
    add("gzip2-byte-tiles", "u8", shape=shape, dimensions=2, hdu=1,
        contents=gzip_tiles(byte_codes[0]), array=byte_codes)

    volume_shape, tile_shape = (5, 7, 9), (2, 3, 4)
    z, y, x = np.indices(volume_shape, dtype=np.float32)
    floats = z * 101 + y * 3.25 - x * 0.125
    floats[0, 0, 0] = np.float32(-0.0)
    floats[4, 6, 8] = np.float32(2 ** -149)
    volume = retained_gzip_tiles(gzip_tiles(floats, tile_shape=tile_shape), volume_baseline,
                                 "7d6aaff7eb092566b72ae539f57e921b2441e64541e005181b802322fb6aef61", tile_count=27)
    add("gzip2-float32-volume", "f32", shape=volume_shape, hdu=1, contents=volume, array=floats.view("u4"))
    words = raw_codes("i16", volume_shape)
    add("gzip1-int16-volume", "i16", shape=volume_shape, hdu=1,
        contents=gzip_tiles(words, "GZIP_1", tile_shape=tile_shape), array=words)
    wide_words = raw_codes("i32", volume_shape)
    add("rice-int32-volume", "i32", shape=volume_shape, hdu=1,
        contents=rice_file(wide_words, tile_shape), array=wide_words)
    add("gzip2-singleton-z", "f32", shape=(1, 7, 9), hdu=1,
        contents=gzip_tiles(floats[:1], tile_shape=(1, 3, 4)), array=floats[:1].view("u4"))

    unicode_name = "\u03bc-\u56fe.fits"
    (directory / unicode_name).write_bytes((directory / "i16-main.fits").read_bytes())
    (directory / "i16-source-changed.fits").write_bytes(write_bytes(main+[card("COMMENT",comment="Changed source")],wire))

    def bad(name, content, status, hdu=None, *, compressed=False):
        path = directory / (name + (".fits.gz" if compressed else ".fits"))
        path.write_bytes(content)
        negative[path.name] = {"status":status,"sha256":sha256(path),
                               "option":f'{{"hdu":{hdu}}}' if hdu is not None else "-"}

    binary_data = 2 * BLOCK
    first_length, first_offset = struct.unpack_from(">ii", tiled, binary_data)
    assert first_offset == 0 and tiled[binary_data + 72:binary_data + 74] == b"\x1f\x8b"
    for label, off, word in (("negative-offset", 4, -1), ("heap-overrun", 4, 314),
                             ("empty", 0, 0), ("length-overrun", 0, 315)):
        altered = bytearray(tiled)
        struct.pack_into(">i", altered, binary_data + off, word)
        bad("tile-" + label, altered, 4, hdu=1)
    damaged = bytearray(tiled)
    damaged[binary_data + 72 + first_length - 8] ^= 1
    bad("tile-bad-crc", damaged, 4, hdu=1)
    for label, key, value, status in (("bad-grid", "ZTILE1", 9, 4), ("bad-tile", "ZTILE2", 0, 4),
                                       ("other-codec", "ZCMPTYPE", "'HCOMPRESS_1'", 2)):
        start = tiled.index(key.encode().ljust(8), BLOCK, binary_data)
        bad("tile-" + label, tiled[:start] + card(key, value) + tiled[start+80:], status, hdu=1)
    for label, key, value in (("bytepix", "ZVAL2", 4), ("zero-block", "ZVAL1", 0), ("duplicate", "ZNAME2", "'BLOCKSIZE'")):
        start = rice.index(key.encode().ljust(8), BLOCK, binary_data)
        bad("rice-" + label, rice[:start] + card(key, value) + rice[start+80:], 4, hdu=1)
    rice_size, rice_offset = struct.unpack_from(">ii", rice, binary_data)
    assert rice_offset == 0
    rice_start = binary_data + 72
    damaged = bytearray(rice)
    struct.pack_into(">i", damaged, binary_data, rice_size - 1)
    bad("rice-truncated", damaged, 4, hdu=1)
    bad("rice-nonzero-pad", rice[:rice_start + rice_size - 1] + bytes([rice[rice_start + rice_size - 1] | 1]) +
        rice[rice_start + rice_size:], 4, hdu=1)
    invalid_seed = bytearray(rice)
    invalid_seed[rice_start + 2] = 0xF8
    bad("rice-seed-mismatch", invalid_seed, 4, hdu=1)
    i32_start = binary_data + 8 * 4
    reserved = bytearray(rice_i32)
    reserved[i32_start + 4] |= 0xF8
    bad("rice-reserved-block", reserved, 4, hdu=1)
    for label, key, value, status in (("dither", "ZQUANTIZ", "'SUBTRACTIVE_DITHER_1'", 2),
                                      ("noisebit", "ZVAL1", "4.0", 2),
                                      ("extent", "ZTILE1", 3, 4)):
        start = gzip2.index(key.encode().ljust(8), BLOCK, binary_data)
        bad("gzip2-" + label, gzip2[:start] + card(key, value) + gzip2[start+80:], status, hdu=1)
    unlabeled = gzip2
    for key in ("ZNAME1", "ZVAL1"):
        start = unlabeled.index(key.encode().ljust(8), BLOCK, binary_data)
        unlabeled = unlabeled[:start] + card("COMMENT", comment="Noise level absent") + unlabeled[start+80:]
    bad("gzip2-bare-no-dither", unlabeled, 2, hdu=1)
    bad("gzip2-quantized-table", gzip_tiles(float_values, quantize_level=4), 2, hdu=1)
    start = gzip2.index(b"EXTNAME ", BLOCK, binary_data)
    bad("gzip2-float-blank", gzip2[:start] + card("ZBLANK", 0) + gzip2[start+80:], 2, hdu=1)
    damaged = bytearray(gzip2)
    member_length, member_offset = struct.unpack_from(">ii", gzip2, binary_data)
    damaged[binary_data + 72 + member_offset + member_length - 8] ^= 1
    bad("gzip2-crc", damaged, 4, hdu=1)
    for label, key, value, status in (("rank-four", "ZNAXIS", 4, 2), ("stray-z", "ZNAXIS", 2, 4),
                                      ("zero-tile-z", "ZTILE3", 0, 4), ("wrong-grid", "ZTILE3", 4, 4),
                                      ("wrong-edge-depth", "ZNAXIS3", 6, 4),
                                      ("budget", "ZNAXIS3", 2**31 - 1, 3)):
        start = volume.index(key.encode().ljust(8), BLOCK, binary_data)
        bad("volume-" + label, volume[:start] + card(key, value) + volume[start+80:], status, hdu=1)
    start = volume.index(b"ZNAXIS3 ", BLOCK, binary_data)
    bad("volume-missing-z", volume[:start] + card("COMMENT", comment="Z dimension absent") + volume[start+80:], 4, hdu=1)

    def replace(name, index, keyword, value, status):
        cards = main.copy()
        cards[index] = card(keyword,value)
        bad(name,write_bytes(cards,wire),status)

    complete = write_bytes(main,wire)
    def flipped(data, offset):
        result = bytearray(data)
        result[offset] ^= 1
        return bytes(result)

    bad("checked-data-changed", flipped(checked_primary, BLOCK + 5), 4)
    bad("checked-header-changed", flipped(checked_primary, checked_primary.index(b"authored fixture")), 4)
    bad("datasum-only-changed", flipped(datasum_only, BLOCK + 14), 4)
    bad("checksum-only-changed", flipped(checksum_only, BLOCK + 14), 4)
    bad("checked-unselected-changed", flipped(checked_bundle, 4 * BLOCK + 7), 4, hdu=1)
    bad("datasum-overflow", write_bytes(main + [card("DATASUM", "'4294967296'")], wire), 4)
    bad("checksum-duplicate", write_bytes(main + [card("CHECKSUM", "'        '")] * 2, wire), 4)
    gzip_image = gzip.compress(checked_primary, mtime=0)
    for name, content, status in [
        ("gzip-short", gzip_image[:12], 4),
        ("gzip-bad-crc", flipped(gzip_image, len(gzip_image) - 8), 4),
        ("gzip-concatenated", gzip_image * 2, 4),
        ("gzip-trailing", gzip_image + b"tail" + gzip_image[-4:], 4),
        ("gzip-wrong-length", gzip_image[:-4] + struct.pack("<I", 3 * BLOCK), 4),
        ("gzip-oversized-length", gzip_image[:-4] + struct.pack("<I", BLOCK * 100000), 3),
        ("gzip-nonaligned", gzip.compress(checked_primary[:-1], mtime=0), 4),
        ("gzip-inner-checksum", gzip.compress(flipped(checked_primary, BLOCK + 5), mtime=0), 4),
    ]:
        bad(name, content, status, compressed=True)
    bad("bad-signature",b"NOTFITS"+complete[7:],2)
    bad("short-header",complete[:BLOCK-1],4)
    bad("short-data",complete[:-1],4)
    bad("trailing-bytes",complete+b"\0",4)
    bad("unexplained-block",complete+bytes(BLOCK),4)
    extension = fits.hdu.image.ImageHDU(np.ones((2,2),dtype=np.uint8))
    extended = directory / "extra-hdu.fits"
    fits.HDUList([fits.PrimaryHDU(np.ones((3,5),dtype=np.int16)), extension]).writeto(extended)
    bad("extra-hdu",extended.read_bytes(),2)
    bad("bad-header-pad",complete[:BLOCK-1]+b"\0"+complete[BLOCK:],4)
    bad("bad-data-pad",complete[:-1]+b"\1",4)
    bad("nonascii-card",complete[:20]+b"\x80"+complete[21:],4)
    replace("simple-false",0,"SIMPLE","F",2)
    replace("simple-malformed",0,"SIMPLE","Q",4)
    replace("unknown-bitpix",1,"BITPIX",12,2)
    replace("dimension-four",2,"NAXIS",4,2)
    replace("dimension-zero",3,"NAXIS1",0,4)
    replace("dimension-negative",3,"NAXIS1",-1,4)
    replace("wrong-mandatory-order",3,"NAXIS2",5,4)
    replace("bad-layout-integer",3,"NAXIS1","1.5",4)
    replace("extent-budget",3,"NAXIS1",2147483647,3)
    for name, keywords, status in (
        ("duplicate-bitpix",[card("BITPIX",16)],4),
        ("duplicate-scale",[card("BSCALE",1),card("BSCALE",1)],4),
        ("bad-scale",[card("BSCALE","1D")],4),
        ("rounded-scale",[card("BSCALE","1.00000000000000000001")],2),
        ("other-scale",[card("BSCALE",2)],2),
        ("other-offset",[card("BZERO",32767)],2),
        ("bad-blank",[card("BLANK",32768)],4),
        ("wcs",[card("CTYPE1","'RA---TAN'")],2),
        ("physical-units",[card("BUNIT","'K'")],2),
        ("checksum",[card("CHECKSUM","'abcd'")],4),
        ("bad-extend",[card("EXTEND","?")],4),
    ):
        bad(name,write_bytes(main+keywords,wire),status)
    f32_cards, f32_wire = authored(raw_codes("f32",(2,3,5)),"f32",3,None,())
    bad("float-blank",write_bytes(f32_cards+[card("BLANK",0)],f32_wire),2)
    u64_cards, u64_wire = authored(raw_codes("u64",(2,3,5)),"u64",3,None,(),("1","9223372036854775809"))
    bad("rounded-u64-offset",write_bytes(u64_cards,u64_wire),2)
    bad("header-budget",write_bytes(main+[card("COMMENT",comment="Bounded") for _ in range(64*36)],wire),3)

    def extension_card_patch(content, block, keyword, replacement):
        start = block * BLOCK
        for offset in range(start, start + BLOCK, 80):
            if content[offset:offset+8] == keyword.ljust(8).encode("ascii"):
                return content[:offset] + replacement + content[offset+80:]
        raise AssertionError(f"{keyword} missing in block {block}")

    bad("bundle-default-refused", bundle, 2)
    bad("empty-default-refused", bundle[:BLOCK], 2)
    bad("bundle-selected-empty", bundle, 3, hdu=0)
    bad("bundle-missing-selection", bundle, 3, hdu=3)
    bad("standalone-missing-selection", complete, 3, hdu=1)
    bad("bundle-truncated-tail-data", bundle[:-1], 4, hdu=1)
    bad("bundle-truncated-tail-header", bundle[:4*BLOCK-1], 4, hdu=1)
    bad("bundle-tail-padding", bundle[:-1] + b"\1", 4, hdu=1)
    bad("bundle-skipped-padding", bundle[:3*BLOCK-1] + b"\1" + bundle[3*BLOCK:], 4, hdu=2)
    bad("bundle-extension-order", extension_card_patch(bundle, 3, "PCOUNT", card("GCOUNT", 1)), 4, hdu=1)
    bad("bundle-duplicate-group", extension_card_patch(bundle, 3, "EXTVER", card("GCOUNT", 1)), 4, hdu=1)
    bad("bundle-parameter-count", extension_card_patch(bundle, 1, "PCOUNT", card("PCOUNT", 1)), 2, hdu=2)
    bad("bundle-group-count", extension_card_patch(bundle, 3, "GCOUNT", card("GCOUNT", 2)), 2, hdu=1)
    bad("bundle-extname-malformed", extension_card_patch(bundle, 3, "EXTNAME", card("EXTNAME", "bare")), 4, hdu=1)
    bad("bundle-extver-malformed", extension_card_patch(bundle, 3, "EXTVER", card("EXTVER", 0)), 4, hdu=1)
    bad("bundle-tail-wcs", extension_card_patch(bundle, 3, "EXTVER", card("CTYPE1", "'RA---TAN'")), 2, hdu=1)
    bad("bundle-table-refused", mixed_bundle, 2, hdu=1)
    bad("table-bitpix", extension_card_patch(mixed_bundle, 1, "BITPIX", card("BITPIX", 16)), 4, hdu=2)
    bad("table-group-count", extension_card_patch(mixed_bundle, 1, "GCOUNT", card("GCOUNT", 2)), 4, hdu=2)
    bad("table-missing-format", extension_card_patch(mixed_bundle, 1, "TFORM1", card("COMMENT")), 4, hdu=2)
    bad("table-unknown-format-index", extension_card_patch(mixed_bundle, 1, "TFORM1", card("TFORM2", "'J'")), 4, hdu=2)
    bad("table-size-bound", extension_card_patch(mixed_bundle, 1, "NAXIS2", card("NAXIS2", 100000000)), 3, hdu=2)
    bad("table-tile-compressed", extension_card_patch(mixed_bundle, 1, "TTYPE1", card("ZIMAGE", "T")), 2, hdu=2)
    bad("table-heap-overlap", extension_card_patch(mixed_bundle, heap_block, "THEAP", card("THEAP", 1)), 4, hdu=2)
    bad("table-heap-corrupt", flipped(mixed_bundle, heap_data + heap_start), 4, hdu=2)
    bad("ascii-selection", ascii_bundle, 2, hdu=1)
    bad("ascii-missing-column", extension_card_patch(ascii_bundle, 1, "TBCOL1", card("COMMENT")), 4, hdu=2)
    bad("ascii-column-outside-row", extension_card_patch(ascii_bundle, 1, "TBCOL1", card("TBCOL1", 4)), 4, hdu=2)
    bad("ascii-parameter-count", extension_card_patch(ascii_bundle, 1, "PCOUNT", card("PCOUNT", 1)), 4, hdu=2)
    bad("ascii-padding", flipped(ascii_bundle, ascii_data + 3), 4, hdu=2)
    bad("ascii-checksum", flipped(ascii_bundle, ascii_data + 2), 4, hdu=2)
    bad("bundle-hdu-count-limit", bundle[:BLOCK] + bundle[BLOCK:3*BLOCK] * 64, 3, hdu=1)

    columns = ("name", "filename", "width", "height", "z_planes", "sample_type", "sample_bytes", "dimension_order",
               "calibration_flags", "pixel_width_um", "pixel_height_um", "z_spacing_um", "option")
    (directory / "fixtures.tsv").write_text(
        "".join("\t".join(str(case[column]) for column in columns) + "\n" for case in cases.values()),
        encoding="utf8",
    )
    (directory / "negative.tsv").write_text(
        "".join(f"{name}\t{item['status']}\t{item['option']}\n" for name, item in negative.items()), encoding="utf8",
    )
    manifest = {
        "schema": "qpx.fits-test-fixtures/1", "cases": cases, "negative": negative,
        "oracle": {"astropy": astropy.__version__, "numpy": np.__version__,
                   "compare":"Authored stored bits checked by Astropy. Exact native bit patterns and independent null masks checked across the C ABI."},
        "unicode_filename": unicode_name,
    }
    (directory / "fixtures.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    return manifest


def run(command, directory, name, checks, *, expected_exit=0, diagnostic=None):
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True, encoding="utf8")
    log = directory / (name + ".log")
    log.write_text(result.stdout + result.stderr, encoding="utf8")
    checks.append({"name": name, "command": command, "exit": result.returncode,
                   "expected_exit": expected_exit, "log": str(log)})
    if result.returncode != expected_exit or (diagnostic and diagnostic not in result.stderr):
        raise RuntimeError(f"{name} exited {result.returncode}, expected {expected_exit}\n{result.stdout}{result.stderr}")


def bundle_manifests(manifest, output):
    catalogue = output / "bundle-catalogue"
    catalogue.mkdir()
    document = json.loads(manifest.read_text(encoding="utf8"))
    plugin, = document["plugins"]
    assert not plugin["dependencies"] and "options" not in plugin
    module_name = plugin["module"]["path"]
    assert "/" not in module_name and "\\" not in module_name and ":" not in module_name
    target = catalogue / module_name
    target.write_bytes((manifest.parent / module_name).read_bytes())
    assert sha256(target) == plugin["module"]["sha256"]
    manifests = {}
    for index in (0, 1, 2, 4):
        plugin["options"] = {"hdu": index}
        path = catalogue / f"hdu{index}.json"
        path.write_text(json.dumps(document), encoding="utf8")
        manifests[index] = path
    return manifests


def validate(args):
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError("Validation requires a new or empty output directory")
    output.mkdir(parents=True, exist_ok=True)
    fixture_path = output / "fixtures"
    fixtures = write_fixtures(fixture_path, args.tile_baseline, args.rice_baseline, args.gzip2_baseline, args.volume_baseline)
    artifact_names = ("module", "abi_test", "consumer", "manifest", "region")
    artifacts = {name: path.resolve() for name in artifact_names if (path := getattr(args, name))}
    before = {name: sha256(path) for name, path in artifacts.items()}
    checks = []
    report = {"schema": "qpx.fits-plugin-validation/1", "checks": checks, "artifacts_before": before,
              "artifact_paths": {name: str(path) for name, path in artifacts.items()},
              "fixture_cases": len(fixtures["cases"]), "negative_cases": len(fixtures["negative"]),
              **{name + "_sha256": digest for name, digest in before.items()}}
    try:
        run([str(artifacts["abi_test"]), str(artifacts["module"]), str(fixture_path)],
            output, "direct-abi", checks)
        manifests = bundle_manifests(artifacts["manifest"], output) if args.manifest and (args.consumer or args.region) else {}
        if args.consumer and args.manifest:
            work = output / "project"
            for mode in ("create", "reopen", "missing", "changed-source"):
                run([str(artifacts["consumer"]), mode, str(artifacts["manifest"]), str(fixture_path), str(work)],
                    output, "project-" + mode, checks)
            for mode, selected in (("bundle-create", 1), ("bundle-reopen", 2)):
                run([str(artifacts["consumer"]), mode, str(manifests[selected]), str(fixture_path),
                     str(output / "bundle-project")], output, "project-" + mode, checks)
            for mode, selected in (("tile-create", 1), ("tile-reopen", 0)):
                run([str(artifacts["consumer"]), mode, str(manifests[selected]), str(fixture_path),
                     str(output / "tile-project")], output, "project-" + mode, checks)
            for mode, selected in (("rice-create", 1), ("rice-reopen", 0)):
                run([str(artifacts["consumer"]), mode, str(manifests[selected]), str(fixture_path),
                     str(output / "rice-project")], output, "project-" + mode, checks)
            for mode, selected in (("float-tile-create", 1), ("float-tile-reopen", 0)):
                run([str(artifacts["consumer"]), mode, str(manifests[selected]), str(fixture_path),
                     str(output / "float-tile-project")], output, "project-" + mode, checks)
            for mode, selected in (("volume-tile-create", 1), ("volume-tile-reopen", 0)):
                run([str(artifacts["consumer"]), mode, str(manifests[selected]), str(fixture_path),
                     str(output / "volume-tile-project")], output, "project-" + mode, checks)
        if args.region and args.manifest:
            cli = [str(artifacts["region"]), "--reader-plugins", str(artifacts["manifest"])]
            run(cli + ["--readers"], output, "host-catalog", checks)
            assert "org.qupathxx.fits" in (output / "host-catalog.log").read_text(encoding="utf8")
            command = cli + ["--reader", "org.qupathxx.fits"]
            tile_args = command + ["--reader-options", '{"hdu":1}']
            for codec, dtype, suffix in (("gzip", "<i2", "int16"), ("rice", "<i2", "int16"),
                                         ("gzip2", "<f4", "float32")):
                source = fixture_path / (codec + "-" + suffix + "-tiles.fits")
                crop = output / (codec + "-tile-crop.npy")
                run(tile_args + ["--raw", str(source), str(crop), "--x", "3", "--y", "2", "--width", "6", "--height", "5"],
                    output, "host-raw-" + codec + "-tile-crop", checks)
                expected = np.fromfile(source.with_suffix(".raw"), dtype=dtype).reshape(7, 9)
                actual = np.load(crop, allow_pickle=False)
                assert actual.dtype == expected.dtype and actual.shape == (5, 6, 1)
                assert actual[..., 0].tobytes() == expected[2:, 3:].tobytes()
            source = fixture_path / "gzip2-float32-volume.fits"
            expected_volume = np.fromfile(source.with_suffix(".raw"), dtype="<f4").reshape(5, 7, 9)
            for z in (1, 3, 4):
                crop = output / f"gzip2-volume-z{z}-crop.npy"
                run(tile_args + ["--raw", str(source), str(crop), "--x", "3", "--y", "2", "--z", str(z),
                                 "--width", "6", "--height", "5"], output, f"host-raw-tile-volume-z{z}", checks)
                actual = np.load(crop, allow_pickle=False)
                assert actual.dtype == expected_volume.dtype and actual.shape == (5, 6, 1)
                assert actual[..., 0].tobytes() == expected_volume[z, 2:, 3:].tobytes()
            for name in ("i16-main", "u16-producer", "i64-volume", "u64-producer", "f32-volume", "f64-volume", "u16-blank",
                         "checked-primary", "gzip-primary"):
                case = fixtures["cases"][name]
                assert (case["width"], case["height"]) == (5, 3)
                width, height = (3, 2) if name in ("f32-volume", "f64-volume", "u16-blank") else (5, 3)
                mask = np.fromfile(fixture_path / (name + ".valid"), dtype=np.uint8).reshape(2, 3, 5)
                assert mask[1, :height, :width].all(), name
                source = fixture_path / case["filename"]
                npy = output / (name + "-z1.npy")
                run(command + ["--raw", str(source), str(npy), "--width", str(width), "--height", str(height), "--z", "1"],
                    output, "host-raw-" + name, checks)
                data = np.load(npy, allow_pickle=False)
                expected = np.fromfile(fixture_path / (name + ".raw"), dtype=case["dtype"]).reshape(2, 3, 5)
                assert data.dtype == expected.dtype and data.shape == (height, width, 1), name
                assert data.tobytes() == expected[1, :height, :width].tobytes(), name
            invalid_npy = output / "invalid-f32-z1.npy"
            run(command + ["--raw", str(fixture_path / "f32-volume.fits"), str(invalid_npy),
                           "--width", "5", "--height", "3", "--z", "1"],
                output, "host-raw-refuses-invalid", checks, expected_exit=2,
                diagnostic="Plugin NumPy array export requires complete valid samples")
            assert not invalid_npy.exists(), "Failed export published a NumPy array"
            for name in ("i16-main", "f32-volume", "u16-blank"):
                source = fixture_path / (name + ".fits")
                pam = output / (name + ".pam")
                run(command + [str(source), "0", "0", "0", "5", "3", str(pam)], output, "host-display-" + name, checks)
                assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == (fixture_path / (name + ".rgba")).read_bytes(), name
            shared_bundle = fixture_path / "bundle-hdu1.fits"
            for index, width, height in ((0, 5, 3), (1, 5, 3), (2, 3, 2)):
                name = f"bundle-hdu{index}"
                case = fixtures["cases"][name]
                selected = [str(artifacts["region"]), "--reader-plugins", str(manifests[index]), "--reader", "org.qupathxx.fits"]
                source = fixture_path / "bundle-hdu0.fits" if index == 0 else shared_bundle
                npy = output / (name + "-z1.npy")
                mask = np.fromfile(fixture_path / (name + ".valid"), dtype=np.uint8).reshape(2, 3, 5)
                assert mask[1, :height, :width].all(), name
                run(selected + ["--raw", str(source), str(npy), "--width", str(width), "--height", str(height), "--z", "1"],
                    output, "host-raw-" + name, checks)
                data = np.load(npy, allow_pickle=False)
                expected = np.fromfile(fixture_path / (name + ".raw"), dtype=case["dtype"]).reshape(2, 3, 5)
                assert data.dtype == expected.dtype and data.shape == (height, width, 1) and data.tobytes() == expected[1, :height, :width].tobytes(), name
            pam = output / "bundle-hdu2.pam"
            run(selected + [str(shared_bundle), "0", "0", "0", "5", "3", str(pam)],
                output, "host-display-bundle-hdu2", checks)
            assert pam.read_bytes().split(b"ENDHDR\n", 1)[1] == (fixture_path / "bundle-hdu2.rgba").read_bytes()
            gzip_pam = output / "gzip-bundle-hdu2.pam"
            run(selected + [str(fixture_path / "gzip-bundle-hdu2.fits.gz"), "0", "0", "0", "5", "3",
                            str(gzip_pam)], output, "host-display-gzip-bundle", checks)
            assert gzip_pam.read_bytes() == pam.read_bytes()
            for name, index in (("table-before-hdu2", 2), ("table-after-heap-hdu4", 4),
                                ("ascii-before-hdu2", 2), ("ascii-after-hdu4", 4)):
                case = fixtures["cases"][name]
                selected = [str(artifacts["region"]), "--reader-plugins", str(manifests[index]),
                            "--reader", "org.qupathxx.fits"]
                dest = output / (name + ".npy")
                run(selected + ["--raw", str(fixture_path / case["filename"]), str(dest),
                                "--width", "3", "--height", "2", "--z", "1"],
                    output, "host-raw-" + name, checks)
                expected = np.fromfile(fixture_path / (name + ".raw"), dtype=case["dtype"]).reshape(2, 3, 5)
                assert np.load(dest, allow_pickle=False).tobytes() == expected[1, :2, :3].tobytes()
            for name, index in (("checked-header-changed", None), ("checked-unselected-changed", 1)):
                chosen = command if index is None else [
                    str(artifacts["region"]), "--reader-plugins", str(manifests[index]), "--reader", "org.qupathxx.fits"]
                absent = output / (name + ".npy")
                diagnostic = "FITS CHECKSUM" if index is None else "FITS DATASUM"
                run(chosen + ["--raw", str(fixture_path / (name + ".fits")), str(absent),
                              "--width", "3", "--height", "2"],
                    output, "host-refuses-" + name, checks, expected_exit=2, diagnostic=diagnostic)
                assert not absent.exists(), name
            absent = output / "bundle-without-selection.npy"
            run(command + ["--raw", str(shared_bundle), str(absent), "--width", "3", "--height", "2"],
                output, "host-refuses-bundle-without-selection", checks, expected_exit=2)
            assert not absent.exists(), "Unselected bundle published an output"
            absent = output / "gzip-inner-checksum.npy"
            run(command + ["--raw", str(fixture_path / "gzip-inner-checksum.fits.gz"), str(absent),
                           "--width", "3", "--height", "2"],
                output, "host-refuses-gzip-inner-checksum", checks, expected_exit=2, diagnostic="FITS DATASUM")
            assert not absent.exists(), "Gzip with invalid FITS checksum published an output"
            for name, index, diagnostic in (
                ("bundle-table-refused", 1, "selected HDU is a binary table"),
                ("table-heap-corrupt", 2, "FITS DATASUM"),
                ("ascii-selection", 1, "selected HDU is an ASCII table"),
                ("ascii-padding", 2, "FITS data block padding"),
                ("ascii-checksum", 2, "FITS DATASUM"),
            ):
                selected = [str(artifacts["region"]), "--reader-plugins", str(manifests[index]),
                            "--reader", "org.qupathxx.fits"]
                absent = output / (name + ".npy")
                run(selected + ["--raw", str(fixture_path / (name + ".fits")), str(absent),
                                "--width", "3", "--height", "2"],
                    output, "host-refuses-" + name, checks, expected_exit=2, diagnostic=diagnostic)
                assert not absent.exists(), name
        report["checks_passed"] = True
    finally:
        report["artifacts_after"] = {name: sha256(path) for name, path in artifacts.items()}
        report["artifacts_unchanged"] = before == report["artifacts_after"]
        report["passed"] = bool(report.get("checks_passed") and report["artifacts_unchanged"])
        (output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf8")
        if not report["artifacts_unchanged"]:
            raise RuntimeError("Validation module, manifest or consumer changed during execution")
    print(json.dumps({"passed": True, "fixture_cases": len(fixtures["cases"]),
                      "native_checks": len(checks), "report": str(output / "validation.json")}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="mode", required=True)
    output = commands.add_parser("fixtures")
    output.add_argument("output", type=Path)
    validate_parser = commands.add_parser("validate")
    for name in ("module", "abi-test", "output"):
        validate_parser.add_argument("--" + name, type=Path, required=True)
    for name in ("consumer", "manifest", "region", "tile-baseline", "rice-baseline", "gzip2-baseline", "volume-baseline"):
        validate_parser.add_argument("--" + name, type=Path)
    args = parser.parse_args()
    if args.mode == "fixtures":
        result = write_fixtures(args.output.resolve())
        print(json.dumps({"cases": len(result["cases"]), "negatives": len(result["negative"])}))
    else:
        validate(args)


if __name__ == "__main__":
    main()
