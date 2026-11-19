# SPDX-License-Identifier: AGPL-3.0-only
"""Stage a hash-bound Windows NGFF 0.5 module and native region CLI."""
import argparse
import json
from pathlib import Path
import tarfile
import zipfile

from check_ngff05 import FLOAT64_CONTRACT, INT8_CONTRACT, INT32_CONTRACT, INT64_CONTRACT, data_pipeline, storage_profile
from check_material_staging import check_archive, inventory
from native_build_inputs import msvc_inputs
from stage_ngff_labels import NgffPackage, ROOT, checked, encoded, file_sha, load, sha


PACKET = ROOT / "build/gui-aggregation/catalogue-closure/ngff05"
LABELS = PACKET.parent / "ngff-labels"
BASE = LABELS / "package-final"
INVENTORY = ROOT / "tests/fixtures/manifest.json"
PROVIDER = "org.qupathxx.ngff05"
CONTRACT = "ngff05-zarr3-uint16/2"
TYPED_CONTRACT = "ngff05-zarr3-image/3"
CODEC_CONTRACT = "ngff05-zarr3-image/4"
UINT8_CONTRACT = "ngff05-zarr3-image/5"
INT16_CONTRACT = "ngff05-zarr3-image/6"
DISPLAY_CONTRACT = "ngff05-zarr3-image/7"
TRANSPOSE_CONTRACT = "ngff05-zarr3-image/8"
CRC32C_CONTRACT = "ngff05-zarr3-image/9"
SHARD_CRC32C_CONTRACT = "ngff05-zarr3-image/10"
JSON_PREFIX = "build/ci/gridfrontends167-package/_deps/json-src/include/"
ZLIB_PREFIX = "build/format-family-plugins/native/_deps/qpx_zlib-build/qpx-include/"
KIT_PREFIX = "build/gui-aggregation/catalogue-closure/ngff-labels/reader-kit/"
ZSTD_SHA = "eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3"
ZSTD_NOTICE_SHA = "7055266497633c9025b777c78eb7235af13922117480ed5c674677adc381c9d8"


class ImagePackage(NgffPackage):
    material_prefix = "ngff05"


def compiled_inputs(package, admission, *, module_only=False, blosc_sources=False, snappy_archive=None):
    paths, targets, commands = {}, {}, {}
    generated = {}
    builds = [("cache", {"qpx_ngff05_plugin", "ngff05_zstd"})]
    if blosc_sources:
        builds[0][1].add("ngff05_blosc")
    snappy_members = {}
    if snappy_archive:
        builds[0][1].add("snappy")
        with tarfile.open(snappy_archive) as archive:
            snappy_members = {item.name: sha(archive.extractfile(item).read()) for item in archive.getmembers()
                              if item.isfile() and item.name.endswith((".h", ".cc"))}
    if not module_only:
        builds.append(("host-cache", {"qupathxx_region"}))
    for cache, expected in builds:
        sources, groups, _, invocations = msvc_inputs(ROOT, PACKET / cache, expected.__contains__)
        assert set(groups) == expected and all(groups.values())
        paths.update(sources)
        targets.update(groups)
        commands.update({target: data["sha256"] for target, data in invocations.items()})
    extras = (
        "plugins/ngff05/CMakeLists.txt", "plugins/ngff05/Zstd.cmake",
        "plugins/image-common/plugin_build.cmake",
        "plugins/image-common/reader_plugin_profile.hpp.in",
        "plugins/image-common/stage_sdk_runtime.cmake", "plugins/image-common/write_manifest.cmake",
        "plugins/ngff05/tests/shard_index_tests.cpp",
        "tools/ci/check_ngff05.py",
        "tools/ci/check_ngff05_shard_crc.py",
        "tools/ci/make_ngff05_producer_fixture.py", "tools/ci/stage_ngff05.py",
    )
    if not module_only:
        extras += ("tools/ci/ngff05_host/CMakeLists.txt",)
    if blosc_sources:
        extras += ("plugins/ngff05/Blosc.cmake", "plugins/ngff05/BloscDecoder.cmake",
                   "plugins/blosc-bytes/source-lock.json", "tools/ci/check_ngff05_blosc_frames.py",
                   "tools/ci/check_ngff05_index_profiles.py", "tools/ci/stage_ngff05_blosc.py")
    if snappy_archive:
        extras += ("plugins/ngff05/Snappy.cmake", "plugins/parquet_matrix/decoder-sources.json")
    paths.update({rel: file_sha(ROOT / rel) for rel in extras})
    parents = []
    for rel, digest in sorted(paths.items()):
        if rel.startswith(("cpp/", "plugins/", "tools/")):
            ident = package.add("source/" + rel, checked(ROOT / rel, digest), rel)
        elif rel.startswith(JSON_PREFIX):
            name = "source/third_party/native-overlay/json/include/" + rel.removeprefix(JSON_PREFIX)
            assert admission["members"][name]["sha256"] == digest
            package.borrow(name, BASE, admission)
            ident = package.members[name]
        elif rel.startswith(KIT_PREFIX):
            name = "source/reader-plugin-kit/" + rel.removeprefix(KIT_PREFIX)
            assert admission["members"][name]["sha256"] == digest
            package.borrow(name, BASE, admission)
            ident = package.members[name]
        elif rel.startswith(ZLIB_PREFIX):
            name = "source/third_party/zlib/include/" + rel.removeprefix(ZLIB_PREFIX)
            ident = package.licensed(name, checked(ROOT / rel, digest), rel, "Zlib",
                                       (package.members["licenses/tiff-zlib/LICENSE"],),
                                       attribution="Jean-loup Gailly and Mark Adler",
                                       evidence="licenses/tiff-zlib/LICENSE; exact SDK prefixed interface")
        elif rel == "build/format-family-plugins/sdk/include/qupathxx/sdk_abi.hpp":
            assert digest == load(BASE / "FROZEN-SOURCE-AVAILABILITY.json")["required_external_abi_header"]["sha256"]
            generated[rel] = digest
            continue
        elif blosc_sources and "/blosc-inputs/c-blosc-1.21.6/" in rel:
            member = rel.split("/blosc-inputs/c-blosc-1.21.6/", 1)[1]
            lock = load(ROOT / "plugins/blosc-bytes/source-lock.json")
            assert lock["members"][member] == digest, member
            generated[rel] = digest
            continue
        elif blosc_sources and rel.endswith("/ngff05_blosc.c"):
            generated[rel] = digest
            continue
        elif snappy_archive and "/snappy-inputs/" in rel:
            assert snappy_members[rel.split("/snappy-inputs/", 1)[1]] == digest, rel
            generated[rel] = digest
            continue
        elif snappy_archive and rel.endswith(("/snappy-build/config.h", "/snappy-build/snappy-stubs-public.h")):
            generated[rel] = digest
            continue
        elif "/zstd-inputs/zstd-1.5.7/lib/" in rel or rel.endswith("/reader_plugin_profile.hpp"):
            generated[rel] = digest
            continue
        else:
            raise ValueError("Unexpected compile input outside admitted source groups: " + rel)
        parents.append(ident)
    assert any(p.endswith("ngff05.cpp") for p in targets["qpx_ngff05_plugin"])
    assert any(p.endswith("zstd_decompress.c") for p in targets["ngff05_zstd"])
    if not module_only:
        assert "cpp/region_export.cpp" in targets["qupathxx_region"]
    return parents, {"observed_inputs": paths, "targets": targets, "command_sha256": commands,
                     "generated_or_archive_inputs": generated}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--zip", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--regular-validation", required=True, type=Path)
    parser.add_argument("--sharded-validation", required=True, type=Path)
    parser.add_argument("--float32-regular-validation", type=Path)
    parser.add_argument("--float32-sharded-validation", type=Path)
    parser.add_argument("--alternate-codec-validation-dir", type=Path)
    parser.add_argument("--uint8-validation-dir", type=Path)
    parser.add_argument("--int16-validation-dir", type=Path)
    parser.add_argument("--omero-display-validation", type=Path)
    parser.add_argument("--transpose-validation-dir", type=Path)
    parser.add_argument("--crc32c-validation-dir", type=Path)
    parser.add_argument("--shard-crc32c-validation-dir", type=Path)
    parser.add_argument("--integer32-validation-dir", type=Path)
    parser.add_argument("--integer32-fixture-dir", type=Path)
    parser.add_argument("--integer64-validation-dir", type=Path)
    parser.add_argument("--integer64-fixture-dir", type=Path)
    parser.add_argument("--float64-validation-dir", type=Path)
    parser.add_argument("--float64-fixture-dir", type=Path)
    parser.add_argument("--int8-validation-dir", type=Path)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    typed = args.float32_regular_validation is not None
    assert typed == (args.float32_sharded_validation is not None), "Supply both Float32 validation receipts"
    alternate = args.alternate_codec_validation_dir
    assert not alternate or typed, "The alternate codec profile requires the Float32 regressions"
    assert not args.uint8_validation_dir or alternate, "The UInt8 profile requires all codec regressions"
    assert not args.int16_validation_dir or args.uint8_validation_dir, "The Int16 profile requires UInt8 regressions"
    assert not args.omero_display_validation or args.int16_validation_dir, "The display profile requires Int16 regressions"
    assert not args.transpose_validation_dir or args.omero_display_validation, "The transpose profile requires the display regression"
    assert not args.crc32c_validation_dir or args.transpose_validation_dir, "The CRC32C profile requires transpose regressions"
    assert not args.shard_crc32c_validation_dir or args.crc32c_validation_dir, "Whole-shard CRC32C requires chunk CRC32C regressions"
    assert not args.integer32_validation_dir or args.shard_crc32c_validation_dir, "32-bit integers require whole-shard regressions"
    assert not args.integer32_fixture_dir or args.integer32_validation_dir, "Supply the 32-bit regression receipts"
    assert not args.integer64_validation_dir or args.integer32_validation_dir, "64-bit integers require the 32-bit regressions"
    assert not args.integer64_fixture_dir or args.integer64_validation_dir, "Supply the 64-bit regression receipts"
    assert not args.float64_validation_dir or args.integer64_validation_dir, "Float64 requires the 64-bit integer regressions"
    assert not args.float64_fixture_dir or args.float64_validation_dir, "Supply the Float64 regression receipts"
    assert not args.int8_validation_dir or args.float64_validation_dir, "Int8 requires the Float64 regressions"
    assert not args.output.exists() and not args.zip.exists(), "Use new stage and ZIP paths"
    runtime = args.runtime
    manifest = runtime / "ngff05-reader-plugins.json"
    module = runtime / "qpx_ngff05_plugin.dll"
    host = runtime / "qupathxx_region.exe"
    entry, = load(manifest)["plugins"]
    contract, version = (TYPED_CONTRACT, "1.2.0") if typed else (CONTRACT, "1.1.0")
    if alternate:
        contract, version = CODEC_CONTRACT, "1.3.0"
    if args.uint8_validation_dir:
        contract, version = UINT8_CONTRACT, "1.4.0"
    if args.int16_validation_dir:
        contract, version = INT16_CONTRACT, "1.5.0"
    if args.omero_display_validation:
        contract, version = DISPLAY_CONTRACT, "1.6.0"
    if args.transpose_validation_dir:
        contract, version = TRANSPOSE_CONTRACT, "1.7.0"
    if args.crc32c_validation_dir:
        contract, version = CRC32C_CONTRACT, "1.8.0"
    if args.shard_crc32c_validation_dir:
        contract, version = SHARD_CRC32C_CONTRACT, "1.9.0"
    if args.integer32_validation_dir:
        contract, version = INT32_CONTRACT, "1.10.0"
    if args.integer64_validation_dir:
        contract, version = INT64_CONTRACT, "1.11.0"
    if args.float64_validation_dir:
        contract, version = FLOAT64_CONTRACT, "1.12.0"
    if args.int8_validation_dir:
        contract, version = INT8_CONTRACT, "1.13.0"
    assert entry["provider_id"] == PROVIDER and entry["contract"] == contract and entry["version"] == version
    assert entry["capabilities"] == {"display_rgba": True, "raw_samples": True, "source_store": True}
    module_sha = file_sha(module)
    host_sha = file_sha(host)
    manifest_sha = file_sha(manifest)
    assert entry["module"] == {"path": module.name, "sha256": module_sha}
    assert module_sha == file_sha(PACKET / "cache/Release" / module.name)
    assert host_sha == file_sha(PACKET / "package-01/bin" / host.name)
    validations = {}
    checks = [
        ("regular", args.regular_validation, "producer-01", 30, 13),
        ("sharded", args.sharded_validation, "sharded-producer-01", 46, 23),
    ]
    if typed:
        checks += [
            ("float32-regular", args.float32_regular_validation, "float32-regular-producer-01", 38, 14),
            ("float32-sharded", args.float32_sharded_validation, "float32-sharded-producer-01", 38, 14),
        ]
    if alternate:
        for label, count, refusals in (
            ("bytes-regular", 30, 13), ("bytes-sharded", 43, 17),
            ("gzip-regular", 46, 21), ("gzip-sharded", 35, 17),
        ):
            checks.append((label, alternate / f"{label}-integration-01/results.json",
                           f"{label}-producer-01", count, refusals))
    if args.uint8_validation_dir:
        for label, count, refusals in (("uint8-regular", 39, 20), ("uint8-sharded", 44, 24), ("uint8-bytes", 40, 20)):
            checks.append((label, args.uint8_validation_dir / f"{label}-integration-01/results.json",
                           f"{label}-producer-01", count, refusals))
    if args.int16_validation_dir:
        for label, count, refusals in (("int16-regular", 37, 18), ("int16-sharded", 42, 22), ("int16-bytes", 38, 18)):
            checks.append((label, args.int16_validation_dir / f"{label}-integration-01/results.json",
                           f"{label}-producer-01", count, refusals))
    if args.omero_display_validation:
        checks.append(("omero-display", args.omero_display_validation, "omero-display-producer-01", 45, 14))
    if args.transpose_validation_dir:
        for label, fixture, count, refusals in (
            ("transpose-regular", "transpose-regular-producer-01", 42, 22),
            ("transpose-inner", "transpose-inner-producer-01", 47, 21),
            ("transpose-outer", "transpose-outer-producer-02", 42, 22),
        ):
            checks.append((label, args.transpose_validation_dir / f"{label}-integration-01/results.json",
                           fixture, count, refusals))
    if args.crc32c_validation_dir:
        for label, count, refusals in (("crc32c-regular", 31, 14), ("crc32c-inner", 40, 20),
                                       ("crc32c-double", 54, 26)):
            checks.append((label, args.crc32c_validation_dir / f"{label}-integration-01/results.json",
                           f"{label}-producer-01", count, refusals))
    for label, path, fixture, count, refusals in checks:
        check = load(path)
        producer_path = PACKET / fixture / "producer.json"
        array = load(producer_path)["levels"][0]["array"]
        sharded, transpose, codecs = storage_profile(array)
        compressor, checksum = data_pipeline(codecs)
        assert check["passed"] is True and check["sharded"] == sharded
        old_dtype = "float32" if check.get("float32", False) else "uint16"
        assert check.get("data_type", old_dtype) == array["data_type"]
        assert check.get("codec", "zstd") == compressor
        assert check.get("endian", "little") == codecs[0].get("configuration", {}).get("endian")
        assert check.get("transpose") == transpose
        assert check.get("crc32c") == checksum
        assert check.get("omero_display", False) == (label == "omero-display")
        assert sum(command["exit_code"] == 1 for command in check["commands"]) == (4 if label == "omero-display" else 0)
        assert check["fixture_sha256"] == file_sha(producer_path)
        assert (check["module_sha256"], check["region_sha256"], check["manifest_sha256"]) == (module_sha, host_sha, manifest_sha)
        assert len(check["commands"]) == count and check["commands"][-1]["name"] == "output-inside-source"
        assert sum(command["exit_code"] == 2 for command in check["commands"]) == refusals
        validations[label] = file_sha(path)
    if args.shard_crc32c_validation_dir:
        for location, dtype, receipt_id, count, refusals in (("start", "int16", "01", 12, 7), ("end", "float32", "02", 13, 8)):
            label = f"whole-shard-{location}"
            path = args.shard_crc32c_validation_dir / f"{label}-integration-{receipt_id}/results.json"
            check = load(path)
            producer_path = PACKET / f"{label}-producer-01/producer.json"
            assert check["passed"] is True and check["index_location"] == location and check["dtype"] == dtype
            assert check["fixture_sha256"] == file_sha(producer_path)
            assert (check["module_sha256"], check["region_sha256"], check["manifest_sha256"]) == (module_sha, host_sha, manifest_sha)
            assert len(check["commands"]) == count and sum(command["exit_code"] == 2 for command in check["commands"]) == refusals
            validations[label] = file_sha(path)
    def wide_integer_checks(receipts, fixtures, width, cases):
        for label, dtype, count in cases:
            path = receipts / f"packaging-checks/checked-{label}/results.json"
            check = load(path)
            assert check["passed"] is True and check["data_type"] == dtype
            assert check["fixture_sha256"] == file_sha(fixtures / label / "producer.json")
            assert (check["module_sha256"], check["region_sha256"], check["manifest_sha256"]) == (module_sha, host_sha, manifest_sha)
            commands = {command["name"]: command["exit_code"] for command in check["commands"]}
            prefix = f"integer{width}"
            assert len(commands) == count and commands[f"{prefix}-extreme-samples"] == 0
            assert commands[f"{prefix}-extreme-display"] == 0 and commands[f"{prefix}-missing-endian"] == 2
            assert all(commands[f"{prefix}-bad-fill-{index}"] == 2 for index in range(7))
            if width == 64:
                assert commands["integer64-adjacent-samples"] == 0
                assert commands["integer64-missing-fill-beyond-binary64"] == 0
            validations[f"{label}-{prefix}"] = file_sha(path)
    if args.integer32_validation_dir:
        wide_integer_checks(args.integer32_validation_dir, args.integer32_fixture_dir or args.integer32_validation_dir,
                            32, (("signed", "int32", 46), ("unsigned", "uint32", 48)))
    if args.integer64_validation_dir:
        wide_integer_checks(args.integer64_validation_dir, args.integer64_fixture_dir or args.integer64_validation_dir,
                            64, (("signed64", "int64", 48), ("unsigned64", "uint64", 50)))
    if args.float64_validation_dir:
        fixtures = args.float64_fixture_dir or args.float64_validation_dir
        for label, count in (("float64-regular", 52), ("float64-sharded", 54)):
            path = args.float64_validation_dir / f"packaging-checks/checked-{label}/results.json"
            check = load(path)
            assert check["passed"] is True and check["data_type"] == "float64"
            assert check["fixture_sha256"] == file_sha(fixtures / label / "producer.json")
            assert (check["module_sha256"], check["region_sha256"], check["manifest_sha256"]) == (module_sha, host_sha, manifest_sha)
            commands = {command["name"]: command["exit_code"] for command in check["commands"]}
            assert len(commands) == count
            assert commands["float64-special-samples"] == commands["float64-display-specials"] == 0
            assert commands["float64-fill-signaling-nan"] == commands["float64-fill-subnormal"] == 0
            assert commands["float64-fill-oversized-number"] == commands["float64-fill-malformed-hex"] == 2
            if label.endswith("sharded"):
                assert commands["float64-inner-nan-fill"] == 0
            validations[label] = file_sha(path)
    if args.int8_validation_dir:
        fixtures = args.int8_validation_dir
        for label, count, sparse_case in (
            ("int8-regular", 42, "absent-crc32c-member-uses-fill"),
            ("int8-sharded", 47, "int8-signed-inner-fill"),
        ):
            path = fixtures / f"packaging-checks/checked-{label}/results.json"
            check = load(path)
            assert check["passed"] is True and check["data_type"] == "int8"
            assert check["fixture_sha256"] == file_sha(fixtures / label / "producer.json")
            assert (check["module_sha256"], check["region_sha256"], check["manifest_sha256"]) == (module_sha, host_sha, manifest_sha)
            commands = {command["name"]: command["exit_code"] for command in check["commands"]}
            assert len(commands) == count and commands[sparse_case] == 0
            assert commands["int8-signed-endpoints"] == commands["int8-signed-display"] == 0
            assert commands["empty-single-byte-configuration"] == commands["redundant-single-byte-endian"] == 0
            assert commands["below-int8-fill"] == commands["oversized-fill"] == 2
            validations[label] = file_sha(path)

    admission = load(LABELS / "package-final-admission.json")
    assert admission["admission_exit"] == 0
    module_only = args.integer32_validation_dir is not None
    label = "Windows x64 local source-matched NGFF0.5 optional reader"
    if not module_only:
        label += " and region CLI"
    package = ImagePackage(load(INVENTORY), label,
                           "ngff05_image_plugin")
    availability = load(BASE / "FROZEN-SOURCE-AVAILABILITY.json")
    external = [availability["core"], availability["first_party_source"]]
    external.extend({"path": row["local_path"], "sha256": row["sha256"]} for row in availability["upstream_sources"])
    for record in external:
        assert file_sha(Path(record["path"])) == record["sha256"], record["path"]
    assert file_sha(BASE / "native/ngff_native.lib") == file_sha(PACKET.parent / "ome-roi/cache/Release/ngff_native.lib")
    package.borrow("FROZEN-SOURCE-AVAILABILITY.json", BASE, admission)
    for name in admission["members"]:
        if name.startswith(("licenses/openslide-sdk/", "licenses/nlohmann-json/", "licenses/tiff-zlib/",
                            "source/frozen-sdk-interface/")) or name in (
            "licenses/qupathxx/LICENSE", "licenses/LICENSE", "source/LICENSE",
            "source/reader-plugin-kit/share/qupathxx/reader-plugin/profile.cmake",
            "source/reader-plugin-kit/share/qupathxx/reader-plugin/LICENSE",
        ):
            package.borrow(name, BASE, admission)
    dependency, = entry["dependencies"]
    name = "bin/" + dependency["path"]
    assert admission["members"][name]["sha256"] == dependency["sha256"] == file_sha(runtime / dependency["path"])
    package.borrow(name, BASE, admission)
    parents, inputs = compiled_inputs(package, admission, module_only=module_only)
    archive = PACKET / "inputs/zstd-1.5.7.tar.gz"
    origin = "https://github.com/meta/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz"
    notice = package.licensed("licenses/zstd/LICENSE", checked(runtime / "licenses/zstd/LICENSE", ZSTD_NOTICE_SHA),
                              origin + " (zstd-1.5.7/LICENSE)", "BSD-3-Clause", kind="notice",
                              attribution="Meta Platforms, Inc. and affiliates",
                              evidence="Official Zstandard 1.5.7 BSD-3-Clause LICENSE")
    codec = package.licensed("source/third_party/zstd-1.5.7.tar.gz", checked(archive, ZSTD_SHA),
                             origin, "BSD-3-Clause", (notice,), "source_archive",
                             "Meta Platforms, Inc. and affiliates", "licenses/zstd/LICENSE")
    prior_overlay = admission["members"]["native/ngff_native.lib"]
    lineage = parents + [codec, package.members[name], package.members["FROZEN-SOURCE-AVAILABILITY.json"]]
    binary = package.add("bin/" + module.name, module.read_bytes(), "Validated NGFF0.5 native DLL",
                         lineage, "derived_asset")
    if not module_only:
        package.add("bin/" + host.name, host.read_bytes(), "Validated store-aware native region CLI",
                    lineage + [prior_overlay["material_id"]], "derived_asset")
    package.add("bin/" + manifest.name, manifest.read_bytes(), "Qualified reader manifest", (binary,), "asset")
    package.add("README.md", (ROOT / "plugins/ngff05/README.md").read_bytes(), "Native install and rebuild guide", kind="asset")
    package.add("NOTICE.txt", b"qupathxx source and new binaries: AGPL-3.0-only. Zstandard 1.5.7 decoder: BSD-3-Clause. "
                b"The corresponding codec source and notice are included. SDK and OpenSlide retain their own terms. "
                b"See FROZEN-SOURCE-AVAILABILITY.json for matching retained inputs and upstream sources. "
                b"Preserve OpenSlide replacement and applicable LGPL relinking rights.\n",
                "Component distribution notices", kind="notice")
    metadata = {"schema": "qpx.ngff05-delivery/1", "providers": {PROVIDER: entry},
                "region_sha256": host_sha, "module_only": module_only, "validation_sha256": validations,
                "compiled": inputs, "upstream_zstd_sha256": ZSTD_SHA,
                "external_open_inputs": {"base": "FROZEN-SOURCE-AVAILABILITY.json",
                                         "store_host_overlay": prior_overlay}}
    result = package.finish(args.output, file_sha(module), metadata, INVENTORY, args.write_inventory)
    with zipfile.ZipFile(args.zip, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as destination:
        for member in sorted(package.members):
            destination.write(args.output / member, member)
    members = check_archive(args.zip, inventory([INVENTORY]), expected=package.members)
    assert members == result["files"]
    receipt = {"archive": args.zip.name, "sha256": file_sha(args.zip), "members": members}
    args.zip.with_suffix(".admission.json").write_bytes(encoded(receipt))
    print(json.dumps(receipt))


if __name__ == "__main__":
    main()
