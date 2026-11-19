# SPDX-License-Identifier: AGPL-3.0-only
"""Stage a tested N5 or BigDataViewer reader with exact source and dependencies."""
import argparse
from pathlib import Path
import tarfile
import zipfile

from check_material_staging import check_archive, inventory
from native_build_inputs import msvc_inputs
from stage_ngff05 import BASE, INVENTORY, JSON_PREFIX, KIT_PREFIX, LABELS, ZLIB_PREFIX
from stage_ngff_labels import NgffPackage, ROOT, checked, encoded, file_sha, load


PACKET = ROOT / "build/gui-aggregation/catalogue-closure/n5"
REGION_PACKAGE = PACKET.parent / "ngff05/package-1.1"
XML_HEADER = "build/ci/gridfrontends167-package/_deps/qpx_xml-src/tinyxml2.h"
XML_ARCHIVE = ROOT / "build/gui-aggregation/molecular-delivery/dependency-sources/sources/tinyxml2-11.0.0.tar.gz"
LOOM_PACKET = PACKET.parent / "loom"
HDF5_PREFIXES = (
    ("build/ci/gridfrontends167-package/_deps/qpx_hdf5-src/src/", "src"),
    ("build/format-family-plugins/native/_deps/qpx_hdf5-build/src/", "generated"),
)


class N5Package(NgffPackage):
    material_prefix = "n5-image"


def compiled_source(package, admitted, cache, plugin, target, include_multiset_check):
    sources, groups, _, commands = msvc_inputs(ROOT, cache, lambda found: found == target)
    assert set(groups) == {target}
    reader_source = "plugins/bdv_hdf5/bdv_hdf5_source.cpp" if plugin == "bdv_hdf5" else "plugins/n5/n5_source.cpp"
    assert {reader_source, "cpp/directory_store.cpp",
            "plugins/image-common/uint16_store_reader.hpp"} <= sources.keys()
    extras = ["plugins/image-common/" + item for item in
              ("plugin_build.cmake", "stage_sdk_runtime.cmake", "write_manifest.cmake", "reader_plugin_profile.hpp.in")]
    extras += [f"plugins/{plugin}/CMakeLists.txt", f"tools/ci/check_{plugin}.py", "tools/ci/check_ngff05.py",
               f"tools/ci/make_{'n5' if plugin == 'n5' else 'bdv'}_producer_fixture.py"]
    if include_multiset_check:
        extras.append("tools/ci/check_bdv_multiset.py")
    generated = (cache / "reader_plugin_profile.hpp").relative_to(ROOT).as_posix()
    loom = None
    if plugin == "bdv_hdf5":
        loom = load(LOOM_PACKET / "package-final-admission.json")
        assert loom["admission_exit"] == 0
        extras.append("tools/ci/check_bdv_n5.py")
    sources.update({key: file_sha(ROOT / key) for key in extras})
    for path, digest in sorted(sources.items()):
        if path.startswith(("cpp/", "plugins/", "tools/")):
            package.add("source/" + path, checked(ROOT / path, digest), path)
        elif path.startswith((JSON_PREFIX, KIT_PREFIX)):
            if path.startswith(JSON_PREFIX):
                member = "source/third_party/native-overlay/json/include/" + path.removeprefix(JSON_PREFIX)
            else:
                member = "source/reader-plugin-kit/" + path.removeprefix(KIT_PREFIX)
            assert admitted["members"][member]["sha256"] == digest
            package.borrow(member, BASE, admitted)
        elif path.startswith(ZLIB_PREFIX):
            package.licensed("source/third_party/zlib/include/" + path.removeprefix(ZLIB_PREFIX),
                checked(ROOT / path, digest), path, "Zlib",
                (package.members["licenses/tiff-zlib/LICENSE"],), attribution="Jean-loup Gailly and Mark Adler",
                evidence="licenses/tiff-zlib/LICENSE; exact SDK prefixed interface")
        elif path == "build/format-family-plugins/sdk/include/qupathxx/sdk_abi.hpp":
            assert digest == load(BASE / "FROZEN-SOURCE-AVAILABILITY.json")["required_external_abi_header"]["sha256"]
        elif path == XML_HEADER and plugin != "n5":
            xml_source = package.known_records["release-src-tinyxml2"]
            data = checked(XML_ARCHIVE, xml_source["sha256"])
            package._member("source/third_party/" + XML_ARCHIVE.name, data, "release-src-tinyxml2")
            header = checked(ROOT / path, digest)
            with tarfile.open(XML_ARCHIVE) as archive:
                with archive.extractfile("tinyxml2-11.0.0/tinyxml2.h") as source:
                    assert source.read() == header
            package.licensed("source/third_party/tinyxml2/tinyxml2.h", header, path, "Zlib",
                ("release-src-tinyxml2", package.members["licenses/tinyxml2/LICENSE.txt"]),
                attribution="TinyXML2 upstream authors", evidence="licenses/tinyxml2/LICENSE.txt")
        elif loom and any(path.startswith(prefix) for prefix, _ in HDF5_PREFIXES):
            prefix, location = next(item for item in HDF5_PREFIXES if path.startswith(item[0]))
            member = f"source/third_party/loom/hdf5/{location}/" + path.removeprefix(prefix)
            assert loom["members"][member]["sha256"] == digest
            package.borrow(member, LOOM_PACKET / "package-final", loom)
        elif path != generated:
            raise ValueError("Unclassified compiler input: " + path)
    return {"source_inputs": sources, "msvc_command_sha256": commands[target]["sha256"]}


def verify_run(path, fixture, binaries, expected_commands, expected_refusals):
    result = load(path)
    assert result["passed"] is True and len(result["commands"]) == expected_commands
    assert sum(command["exit_code"] != 0 for command in result["commands"]) == expected_refusals
    assert result["fixture_sha256"] == file_sha(fixture / "producer.json")
    assert binaries == (result["manifest_sha256"], result["module_sha256"], result["region_sha256"])


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--output", required=True, type=Path)
    args.add_argument("--zip", required=True, type=Path)
    variants = args.add_mutually_exclusive_group()
    variants.add_argument("--bdv", action="store_true", help="Stage the qualified aligned BDV N5 profile")
    variants.add_argument("--bdv-hdf5", action="store_true", help="Stage the qualified aligned BDV HDF5 profile")
    profiles = args.add_mutually_exclusive_group()
    profiles.add_argument("--multiset", action="store_true", help="Stage the BDV 1.1.0 aligned channel profile")
    profiles.add_argument("--partitioned", action="store_true", help="Stage the BDV HDF5 1.2.0 partition profile")
    profiles.add_argument("--shared-view-roles", action="store_true", help="Stage the shared BDV acquisition roles profile")
    profiles.add_argument("--missing-views", action="store_true", help="Stage the BDV missing views profile")
    args.add_argument("--write-inventory", action="store_true")
    args = args.parse_args()
    roles_profile = args.shared_view_roles or args.missing_views
    partition_profile = args.partitioned or (roles_profile and args.bdv_hdf5)
    assert not (args.multiset or roles_profile) or args.bdv or args.bdv_hdf5
    assert not args.partitioned or args.bdv_hdf5
    assert not args.output.exists() and not args.zip.exists()
    packet = PACKET.parent / "bdv" if args.bdv or args.bdv_hdf5 else PACKET
    plugin = "bdv_hdf5" if args.bdv_hdf5 else "bdv_n5" if args.bdv else "n5"
    manifest_id = plugin.replace("_", "-")
    target = f"qpx_{plugin}_plugin"
    cache = packet / ("h5-cache" if args.bdv_hdf5 else "cache" if args.bdv else "cache/provider")
    backend = "hdf5" if args.bdv_hdf5 else "n5"
    profile, run, revision = None, "01", 1
    if args.multiset:
        profile, revision = f"multiset-{backend}", 2
    if args.partitioned:
        profile, run, revision = "partitioned-hdf5", "03", 3
    if args.shared_view_roles:
        profile, revision = f"roles-{backend}", (4 if args.bdv_hdf5 else 3)
    if args.missing_views:
        profile, revision = f"missing-{backend}", (5 if args.bdv_hdf5 else 4)
    runtime = packet / (f"{profile}-runtime" if profile else "h5-runtime" if args.bdv_hdf5 else "runtime")
    manifest = runtime / f"{manifest_id}-reader-plugins.json"
    module = runtime / f"{target}.dll"
    host = runtime / "qupathxx_region.exe"
    result_path = packet / ("h5-integration-01" if args.bdv_hdf5 else "integration-01") / "results.json"
    entry, = load(manifest)["plugins"]
    expected_commands, expected_refusals = (40, 21) if args.bdv_hdf5 else (35, 16) if args.bdv else (60, 27)
    fixture = "h5-producer-01" if args.bdv_hdf5 else "n5-producer-01" if args.bdv else "producer-01"
    binaries = (file_sha(manifest), file_sha(module), file_sha(host))
    validation = {}
    if profile:
        regression_path = packet / f"{profile}-regression-{run}" / "results.json"
        verify_run(regression_path, packet / fixture, binaries, expected_commands, expected_refusals)
        validation["single_setup_regression_sha256"] = file_sha(regression_path)
        result_path = packet / f"{profile}-integration-{run}" / "results.json"
        fixture = "h5-multiset-02" if args.bdv_hdf5 else "n5-multiset-02"
        expected_commands, expected_refusals = 29, 9
    if args.partitioned or roles_profile:
        regression_path = packet / f"{profile}-unpartitioned-{run}" / "results.json"
        verify_run(regression_path, packet / fixture, binaries, expected_commands, expected_refusals)
        validation["unpartitioned_channels_regression_sha256"] = file_sha(regression_path)
    if partition_profile:
        fixture, expected_commands, expected_refusals = "h5-partitioned-03", 44, 21
        if roles_profile:
            regression_path = packet / f"{profile}-partitioned-{run}" / "results.json"
            verify_run(regression_path, packet / fixture, binaries, expected_commands, expected_refusals)
            validation["partition_regression_sha256"] = file_sha(regression_path)
    if roles_profile:
        fixture = f"roles-{backend}-producer-01"
        expected_commands += 9
        expected_refusals += 8
    if args.missing_views:
        regression_path = packet / f"{profile}-roles-{run}" / "results.json"
        verify_run(regression_path, packet / fixture, binaries, expected_commands, expected_refusals)
        validation["shared_view_roles_regression_sha256"] = file_sha(regression_path)
        fixture = f"{profile}-producer-01"
        expected_commands, expected_refusals = (58, 31) if args.bdv_hdf5 else (42, 19)
        result_path = packet / f"{profile}-integration-02" / "results.json"
    verify_run(result_path, packet / fixture, binaries, expected_commands, expected_refusals)
    assert file_sha(module) == file_sha(cache / "Release" / module.name)
    version = f"1.{revision - 1}.0"
    contract = "n5-tensorstore-uint16/1" if plugin == "n5" else f"{manifest_id}-uint16/{revision}"
    assert (entry["provider_id"], entry["contract"], entry["version"], entry["abi"]) == (
        "org.qupathxx." + manifest_id, contract, version, 1)
    assert entry["module"] == {"path": module.name, "sha256": file_sha(module)}
    assert entry["capabilities"] == {"display_rgba": True, "raw_samples": True, "source_store": True}
    admitted = load(LABELS / "package-final-admission.json")
    region_admitted = load(REGION_PACKAGE.parent / "package-1.1-admission.json")
    assert admitted["admission_exit"] == region_admitted["admission_exit"] == 0
    package = N5Package(load(INVENTORY), "Local Windows x64 optional image reader and exact region CLI",
                        f"{plugin}_image_plugin")
    if plugin != "n5":
        package.material_prefix = f"{manifest_id}-image"
    availability = load(BASE / "FROZEN-SOURCE-AVAILABILITY.json")
    for record in (availability["core"], availability["first_party_source"]):
        assert file_sha(Path(record["path"])) == record["sha256"]
    for record in availability["upstream_sources"]:
        assert file_sha(Path(record["local_path"])) == record["sha256"]
    for name in admitted["members"]:
        if name.startswith(("licenses/openslide-sdk/", "licenses/nlohmann-json/", "licenses/tiff-zlib/",
                            "source/frozen-sdk-interface/")) or name in (
            "FROZEN-SOURCE-AVAILABILITY.json", "source/LICENSE", "licenses/qupathxx/LICENSE",
            "source/reader-plugin-kit/share/qupathxx/reader-plugin/profile.cmake",
            "source/reader-plugin-kit/share/qupathxx/reader-plugin/LICENSE",
        ):
            package.borrow(name, BASE, admitted)
    if plugin != "n5":
        package.borrow("licenses/tinyxml2/LICENSE.txt", BASE, admitted)
    if args.bdv_hdf5:
        package.borrow("licenses/hdf5/LICENSE", BASE, admitted)
    for item in entry["dependencies"]:
        name = "bin/" + item["path"]
        assert file_sha(runtime / item["path"]) == item["sha256"] == admitted["members"][name]["sha256"]
        package.borrow(name, BASE, admitted)
    assert file_sha(host) == region_admitted["members"]["bin/" + host.name]["sha256"]
    package.borrow("bin/" + host.name, REGION_PACKAGE, region_admitted)
    inputs = compiled_source(package, admitted, cache, plugin, target, profile is not None)
    binary = package.add("bin/" + module.name, module.read_bytes(), "Validated native N5 provider",
                         package.members.values(), "derived_asset")
    package.add("bin/" + manifest.name, manifest.read_bytes(), "Exact standalone N5 plugin manifest",
                (binary,), "asset")
    package.add("README.md", (ROOT / f"plugins/{plugin}/README.md").read_bytes(), "Windows setup and rebuild guide", kind="asset")
    xml_notice = b"TinyXML2 11.0.0: Zlib, exact upstream source and notice included. " if plugin != "n5" else b""
    hdf5_notice = b"HDF5 2.2.0: original BSD-3-Clause terms and retained component notices. " if args.bdv_hdf5 else b""
    package.add("NOTICE.txt", b"qupathxx provider and source: AGPL-3.0-only. SDK zlib: Zlib. " + xml_notice +
                hdf5_notice +
                b"JSON headers: MIT. OpenSlide and its bundled components keep their individual notices and terms. "
                b"FROZEN-SOURCE-AVAILABILITY.json records the separately retained corresponding sources. "
                b"The unchanged region CLI remains covered by the matching NGFF 1.1.0 source handoff.\n",
                "Component source and license terms", kind="notice")
    metadata = {"schema": f"qpx.{manifest_id}-delivery/1", "providers": {entry["provider_id"]: entry}, "compiled": inputs,
                "validation_sha256": file_sha(result_path), "region_sha256": file_sha(host),
                "region_source_receipt_sha256": file_sha(REGION_PACKAGE / "SOURCE.json"), **validation}
    receipt = package.finish(args.output, file_sha(module), metadata, INVENTORY, args.write_inventory)
    with zipfile.ZipFile(args.zip, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for member in sorted(package.members):
            archive.write(args.output / member, member)
    members = check_archive(args.zip, inventory([INVENTORY]), expected=package.members)
    assert members == receipt["files"]
    args.zip.with_suffix(".admission.json").write_bytes(encoded({"sha256": file_sha(args.zip), "members": members}))
    print(f"Verified {members} admitted N5 Windows ZIP members")


if __name__ == "__main__":
    main()
