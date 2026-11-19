"""Stage the exact qualified M05 Loom/native matrix packet; no builds/downloads.

This development writer preserves component terms and matched source evidence.
The delivered A08 addon and frozen FP04 SDK/core/static dependencies remain
separate required companions. Deployment and ZIP qualification follow staging.
"""
import argparse
import json
from pathlib import Path
import tarfile
import zipfile

from check_material_staging import admitted, inventory as read_inventory
from stage_annotation_sources import Package, ROOT
from stage_ngff_labels import (BASE_CORE, BASE_SOURCE, BASE_SOURCE_SHA, SDK, SDK_ABI_SHA,
                               NATIVE_SHA as A08_NATIVE_SHA, RUNTIMES, checked, context,
                               encoded, file_sha, load, project_path, relative, sha)


PROVIDER = "org.qupathxx.loom"
CONTRACT = "loom3-dense-typed/1"
PROFILE = "windows-x64-msvc1944-release-md"
NATIVE_SHA = "42f41eb92220038324424435a0963b93c4e16ad3420596d603658d6623176fb0"
HDF5_SHA = "ef5a6cfebd2fd9d5bf2b5f449980fcf8676cb64fc42048e952f3c9f3b272f43c"
ZLIB_SHA = "4ec525ee88a54ccf5d33ef1a5c8105533ad19f0ac946b71b52c8a824cbefb63d"
HDF5_CONFIG_SHA = "e980d2826a23809a3640549fecd587868644c494b1ea6e0ddc4640eed7562f77"
ZLIB_CONFIG_SHA = "ae13d72e1dbe3d06a5a1b87a902d1c17bfe9a66ee728ba19a591eb21d16663da"
HDF5_ROOT = ROOT / "build/ci/gridfrontends167-package/_deps/qpx_hdf5-src/src"
HDF5_GENERATED = ROOT / "build/format-family-plugins/native/_deps/qpx_hdf5-build/src"
ZLIB_HEADERS = ROOT / "build/format-family-plugins/native/_deps/qpx_zlib-build/qpx-include"
JSON_ROOT = ROOT / "build/ci/gridfrontends167-package/_deps/json-src/include"
SQLITE_ROOT = ROOT / "build/deps/sqlite-amalgamation-3540100"
HOST_UNITS = {"cpp/format_plugin_loader.cpp", "cpp/format_plugin_runtime.cpp", "cpp/format_import.cpp",
              "cpp/annotation_geometry_value.cpp", "cpp/expression_matrix_metadata.cpp",
              "cpp/expression_matrix_writer.cpp"}
REUSED_CHECKS = {"c-header", "catalog", "actual-workflow", "provider-free-metadata", "provider-free-import",
    "loader-off-metadata", "loader-off-import", "typed-controls", "duplicate-default-options",
    "legacy-schema4-exact", "legacy-abi", "legacy-host-catalog", "legacy-host-matrix-control",
    "legacy-host-receipt", "legacy-host-source-change", "legacy-host-cancel", "legacy-host-ranges",
    "legacy-host-annotation-control", "legacy-import-matrix"}
FINAL_CHECKS = {"legacy-import-annotation", "legacy-import-annotation-stale", "legacy-import-annotation-cancel",
    "legacy-import-positions", "legacy-import-limits", "legacy-import-legacy", "legacy-import-native-id-limits",
    "cli-counts", "cli-info-counts", "cli-existing-counts", "sdk-counts", "cli-continuous",
    "cli-info-continuous", "cli-existing-continuous", "sdk-continuous"}
EXTRA = {"cmake/qpx_loom_matrixConfig.cmake.in", "cmake/H5adReader.cmake", "cmake/PinnedZlib.cmake",
    "docs/benchmarks/loom/README.md", "docs/format-family-plugins.md", "docs/reader-license-policy.md",
    "docs/format-family-catalogs.md", "plugins/format-common/hdf5_input.hpp",
    "plugins/format-common/reader_input.hpp", "plugins/format-common/json_tree.hpp",
    "plugins/format-common/plugin_build.cmake", "plugins/format-common/format_plugin_profile.hpp.in",
    "plugins/format-common/write_manifest.cmake", "tools/ci/check_loom.py", "tools/ci/check_loom_delivery.py",
    "tools/ci/collect_loom_inputs.py", "tools/ci/native_build_inputs.py", "tools/ci/stage_loom.py",
    "tools/ci/stage_ngff_labels.py", "tools/ci/stage_annotation_sources.py",
    "tools/ci/stage_annotation_geometry.py", "tools/ci/check_material_staging.py"}


class LoomPackage(Package):
    def licensed(self, name, data, origin, license_id, parents=(), kind="code",
                 attribution="qupathxx contributors", evidence="LICENSE; repository author declaration"):
        parents = list(dict.fromkeys(parents))
        suffix = "-" + context(parents)[:12] if parents and parents != ["qupathxx-root-license"] else ""
        ident = "loom-" + license_id + "-" + kind + "-" + sha(data)[:24] + suffix
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
                  "material": {"id": ident, "category": "admitted_open", "kind": kind,
                      "license_id": license_id, "license_evidence": evidence, "attribution": attribution,
                      "source_ids": parents}}
        existing = self.known_records.get(ident, self.records.get(ident))
        if existing:
            assert existing["sha256"] == record["sha256"] and existing["bytes"] == len(data)
            assert existing["material"] == record["material"], "Changed material identity: " + ident
        else:
            self.records[ident] = record
        self._member(name, data, ident)
        return ident

    def add(self, name, data, origin, parents=("qupathxx-root-license",), kind="code"):
        return self.licensed(name, data, origin, "AGPL-3.0-only", parents, kind)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packet", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    for name in ("output", "validation", "base-package", "base-admission"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    packet, cache = args.packet.resolve(), args.cache.resolve()
    output = (args.output or packet / "package-final").resolve()
    validation_path = (args.validation or packet / "integration-04/results.json").resolve()
    base_package = (args.base_package or ROOT / "build/gui-aggregation/catalogue-closure/ngff-labels/package-final").resolve()
    base_admission_path = (args.base_admission or base_package.parent / (base_package.name + "-admission.json")).resolve()
    assert not output.exists(), "Use a fresh stage; preserve prior evidence"
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory, before = load(inventory_path), load(packet / "before-material-inventory.json")
    assert len(before) == 45 and "loom_matrix_plugin" not in before
    assert all(inventory.get(name) == value for name, value in before.items()), "Prior inventory group changed"
    assert set(inventory) <= {*before, "loom_matrix_plugin"}, "Unexpected concurrent inventory group"
    entries = read_inventory([inventory_path])
    pkg = LoomPackage(inventory,
        "Selected native Loom3 typed UInt64/Float64 matrix ABI2/schema5 workflow and six-unit SDK addon; "
        "separate exact A08/FP04 companions, unchanged schema4/ABI1 controls; no Python/Java/data/model/UI/general-Loom/full-goal/publication claim",
        "loom_matrix_plugin")

    by_hash = {}
    for ident, record in entries.items():
        by_hash.setdefault(record["sha256"], []).append(ident)

    def known(name, data, preferred=None):
        ids = [i for i in by_hash.get(sha(data), []) if entries[i].get("bytes") == len(data)]
        if preferred:
            assert preferred in ids, "Required exact admitted material absent: " + preferred
            ids = [preferred]
        failures = []
        for ident in sorted(ids):
            try:
                admitted(ident, entries)
            except ValueError as error:
                failures.append(str(error))
                continue
            if name in pkg.files:
                assert pkg.files[name] == data, "Conflicting staged material: " + name
                return pkg.members[name]
            pkg._member(name, data, ident)
            return ident
        raise AssertionError("No eligible exact material for " + name + ": " + str(failures or [sha(data)]))

    compiled_path = packet / "compiled-inputs.json"
    compiled = load(compiled_path)
    assert compiled["schema"] == "qpx.loom-native-inputs/1"
    assert compiled["base_core_sha256"] == BASE_CORE and compiled["retained_application_addon_sha256"] == A08_NATIVE_SHA
    assert compiled["profile"] == {"msvc": 1944, "compiler": "19.44.35225.0", "toolset": "14.44.35207",
                                    "configuration": "Release", "runtime": "MD", "cpp": 17}
    sources = {}
    for name, digest in compiled["sources"].items():
        path = project_path(name)
        key = relative(path)
        assert key not in sources or sources[key] == digest, "Conflicting source case aliases: " + key
        checked(path, digest)
        sources[key] = digest
    groups = {name: [relative(project_path(n)) for n in names] for name, names in compiled["groups"].items()}
    assert all(n in sources for names in groups.values() for n in names)
    assert {n for n in groups["qupathxx_loom_matrix_native"] if n.endswith((".cpp", ".c"))} == HOST_UNITS
    assert {"plugins/loom/loom.cpp", "plugins/format-common/hdf5_input.hpp",
            "plugins/format-common/json_tree.hpp", "plugins/format-common/reader_input.hpp"} <= set(groups["qpx_loom_plugin"])
    for command in compiled["commands"].values():
        assert file_sha(ROOT / command["path"]) == command["sha256"], "Compiler command changed"

    def product(path):
        digest = compiled["products"][relative(path)]
        return checked(path, digest)

    binaries = cache / "loom/Release"
    module, manifest = binaries / "qpx_loom_plugin.dll", binaries / "loom-format-plugins.json"
    cli, consumer = binaries / "qupathxx_format_import.exe", binaries / "loom_matrix_consumer.exe"
    native = cache / "loom-host/Release/qupathxx_loom_matrix_native.lib"
    products = {p.name: product(p) for p in (module, manifest, cli, consumer, native)}
    assert sha(products[native.name]) == NATIVE_SHA
    validation = load(validation_path)
    assert validation["passed"] and validation["native_only_environment"]
    assert FINAL_CHECKS == set(validation["checks"])
    for name, row in validation["checks"].items():
        expected = 2 if name in {"cli-existing-counts", "cli-existing-continuous"} else 0
        assert row["expected_returncode"] == expected and row["returncode"] == expected, name
    reused = validation["reused"]
    primary_path = Path(reused["receipt"])
    assert file_sha(primary_path) == reused["sha256"]
    primary = load(primary_path)
    assert primary["native_only_environment"] and set(reused["passed_checks"]) == REUSED_CHECKS
    assert all(primary["checks"][name]["returncode"] == 0 for name in REUSED_CHECKS)
    assert not primary["passed"], "The reused receipt must retain its documented later setup failure"
    for name, digest in reused["verified_binaries"].items():
        path = project_path(name)
        assert checked(path, digest) and primary["binaries"][str(path)] == digest
        assert compiled["products"][relative(path)] == digest
    for path in (module, cli):
        assert reused["verified_binaries"][str(path)] == sha(products[path.name])
    assert primary["provider_manifest"] == {"path": str(manifest), "sha256": sha(products[manifest.name])}
    for name in ("cli-counts", "cli-continuous"):
        assert Path(validation["checks"][name]["argv"][0]).resolve() == cli
    for name in ("sdk-counts", "sdk-continuous"):
        assert Path(validation["checks"][name]["argv"][0]).resolve() == consumer
        assert Path(validation["checks"][name]["argv"][1]).resolve() == manifest
    reference_path = primary_path.parent / "fixtures/native-reference.json"
    assert file_sha(reference_path) == validation["native_reference_sha256"]
    definition = json.loads(products[manifest.name])
    assert definition["schema"] == "qpx.format-plugins/1" and len(definition["plugins"]) == 1
    entry = definition["plugins"][0]
    assert (entry["provider_id"], entry["format_id"], entry["domain"], entry["version"], entry["contract"],
            entry["abi"], entry["profile"]) == (PROVIDER, "loom", "matrix", "1.0.0", CONTRACT, 2, PROFILE)
    assert entry["module"] == {"path": module.name, "sha256": sha(products[module.name])} and entry["dependencies"] == []

    base_admission, base_source = load(base_admission_path), load(base_package / "SOURCE.json")
    a08_result_path = base_package.parent / "results.json"
    a08_result = load(a08_result_path)
    assert base_admission["admission_exit"] == 0 and a08_result["passed"]
    assert base_source["native_library_sha256"] == a08_result["native_library_sha256"] == A08_NATIVE_SHA
    assert file_sha(base_package / "native/ngff_native.lib") == A08_NATIVE_SHA
    assert base_source["base_sdk"]["core_sha256"] == file_sha(SDK / "lib/qupathxx/qupathxx_core.lib") == BASE_CORE
    assert base_source["base_sdk"]["source_archive_sha256"] == file_sha(BASE_SOURCE) == BASE_SOURCE_SHA
    a08_zip = a08_result["package_zip"]
    assert a08_zip["passed"] and a08_zip["all_member_paths_and_material_identities_match"]
    assert Path(a08_zip["path"]).stat().st_size == a08_zip["bytes"] and file_sha(Path(a08_zip["path"])) == a08_zip["sha256"]
    for name in base_admission["members"]:
        if name.startswith(("source/third_party/native-overlay/json/", "source/third_party/native-overlay/sqlite/", "licenses/")) and not name.startswith("licenses/ngff/"):
            pkg.borrow(name, base_package, base_admission)
    for name in ("source/LICENSE", "source/frozen-sdk-interface/cmake/sdk_abi.hpp.in",
                 "source/frozen-sdk-interface/cmake/NativeSdk.cmake", "source/frozen-sdk-interface/DERIVATION.json"):
        pkg.borrow(name, base_package, base_admission)
    sdk_abi = SDK / "include/qupathxx/sdk_abi.hpp"
    sdk_abi_bytes = checked(sdk_abi, SDK_ABI_SHA)
    abi_derivation = load(base_package / "source/frozen-sdk-interface/DERIVATION.json")
    assert abi_derivation["header"]["sha256"] == SDK_ABI_SHA and abi_derivation["header"]["bytes"] == len(sdk_abi_bytes) == 500
    abi_template = pkg.files["source/frozen-sdk-interface/cmake/sdk_abi.hpp.in"]
    abi_recipe = pkg.files["source/frozen-sdk-interface/cmake/NativeSdk.cmake"]
    with zipfile.ZipFile(BASE_SOURCE) as archive:
        assert archive.read("cmake/sdk_abi.hpp.in") == abi_template and archive.read("cmake/NativeSdk.cmake") == abi_recipe
    assert abi_template.count(b"@MSVC_VERSION@") == 1 and abi_template.replace(b"@MSVC_VERSION@", b"1944") == sdk_abi_bytes
    runtime_ids = {}
    for name, digest in RUNTIMES.items():
        source = binaries / name
        data = product(source)
        assert sha(data) == digest
        pkg.borrow("bin/" + name, base_package, base_admission)
        assert pkg.files["bin/" + name] == data
        runtime_ids[name] = pkg.members["bin/" + name]

    def source_archive(ident):
        record = entries[ident]
        admitted(ident, entries)
        candidates = [ROOT / record["filename"], ROOT / "build/deps" / Path(record["filename"]).name,
                      ROOT / "build/dependency-downloads" / Path(record["filename"]).name]
        found = next((p for p in candidates if p.is_file() and p.stat().st_size == record["bytes"] and file_sha(p) == record["sha256"]), None)
        assert found, "Required matching source archive unavailable; no download: " + ident
        return found, record

    hdf_archive, hdf_record = source_archive("release-src-hdf5")
    zlib_archive, zlib_record = source_archive("release-src-zlib")
    lock = load(ROOT / "plugins/loom/source-lock.json")
    assert lock["hdf5"]["source_archive_sha256"] == hdf_record["sha256"]
    assert lock["zlib"]["source_archive_sha256"] == zlib_record["sha256"]
    hdf_library, zlib_library = SDK / "lib/qupathxx/deps/qpx-hdf5.lib", SDK / "lib/qupathxx/deps/qpx-zlib.lib"
    assert file_sha(hdf_library) == lock["hdf5"]["static_library_sha256"] == HDF5_SHA
    assert file_sha(zlib_library) == lock["zlib"]["static_library_sha256"] == ZLIB_SHA
    hdf_notice, zlib_notice = pkg.members["licenses/hdf5/LICENSE"], pkg.members["licenses/tiff-zlib/LICENSE"]
    known("licenses/zlib/LICENSE", pkg.files["licenses/tiff-zlib/LICENSE"], zlib_notice)
    hdf_tar, zlib_tar = tarfile.open(hdf_archive), tarfile.open(zlib_archive)
    try:
        def upstream(archive, name):
            member = archive.getmember(name)
            assert member.isfile(), "Source member must be a regular file: " + name
            return archive.extractfile(member).read()

        assert upstream(hdf_tar, "hdf5-2.2.0/LICENSE") == pkg.files["licenses/hdf5/LICENSE"]
        assert upstream(zlib_tar, "zlib-1.3.2/LICENSE") == pkg.files["licenses/zlib/LICENSE"]
        hdf_template = upstream(hdf_tar, "hdf5-2.2.0/src/H5pubconf.h.in")
        hdf_template_id = pkg.licensed("source/third_party/loom/hdf5/source/H5pubconf.h.in", hdf_template,
            "release-src-hdf5: hdf5-2.2.0/src/H5pubconf.h.in", "BSD-3-Clause", ["release-src-hdf5", hdf_notice],
            attribution="The HDF Group and contributors; full original notices retained", evidence="licenses/hdf5/LICENSE")
        zlib_templates = {}
        for name in ("CMakeLists.txt", "zconf.h"):
            data = upstream(zlib_tar, "zlib-1.3.2/" + name)
            zlib_templates[name] = pkg.licensed("source/third_party/loom/zlib/source/" + name, data,
                "release-src-zlib: zlib-1.3.2/" + name, "Zlib", ["release-src-zlib", zlib_notice],
                attribution="Jean-loup Gailly, Mark Adler and contributors", evidence="licenses/zlib/LICENSE")
        original_zconf = upstream(zlib_tar, "zlib-1.3.2/zconf.h")
        cmake_zconf = (ZLIB_HEADERS.parent / "zconf.h.cmakein").read_bytes()
        normalized = lambda value: value.replace(b"\r\n", b"\n")
        expected_zconf_template = original_zconf[:245] + b"#cmakedefine Z_PREFIX 1\n#cmakedefine HAVE_STDARG_H 1\n#cmakedefine HAVE_UNISTD_H 1\n" + original_zconf[244:]
        assert normalized(cmake_zconf) == normalized(expected_zconf_template)
        configured_zconf = normalized(cmake_zconf).replace(b"#cmakedefine Z_PREFIX 1", b"#define Z_PREFIX 1").replace(
            b"#cmakedefine HAVE_STDARG_H 1", b"#define HAVE_STDARG_H 1").replace(
            b"#cmakedefine HAVE_UNISTD_H 1", b"/* #undef HAVE_UNISTD_H */")
        assert checked(ZLIB_HEADERS / "zconf.h", ZLIB_CONFIG_SHA) == configured_zconf.replace(b"\n", b"\r\n")
        assert (ZLIB_HEADERS / "zconf.h").read_bytes() == (ZLIB_HEADERS.parent / "zconf.h").read_bytes()
        cmake_zconf_id = pkg.licensed("source/third_party/loom/zlib/source/zconf.h.cmakein", cmake_zconf,
            "Exact frozen CMake-generated zlib configuration template; original source recipe/bytes verified", "Zlib",
            ["release-src-zlib", *zlib_templates.values()], "derived_asset",
            "Jean-loup Gailly, Mark Adler and contributors", "licenses/zlib/LICENSE")
        staged_inputs = {}

        def source_member(name, digest):
            path = project_path(name)
            key = relative(path)
            if key in staged_inputs:
                item = staged_inputs[key]
                if isinstance(item, dict):
                    assert item["external"] and item["sha256"] == digest
                    return item["source_evidence_material_id"]
                assert sha(pkg.files[item]) == digest, "Input changed: " + key
                return pkg.members[item]
            data = checked(path, digest)
            if path == sdk_abi:
                evidence = "source/frozen-sdk-interface/DERIVATION.json"
                staged_inputs[key] = {"external": True, "required_separate_product": "frozen-fp04-sdk",
                    "path": str(path), "bytes": 500, "sha256": SDK_ABI_SHA,
                    "source_evidence_stage_path": evidence, "source_evidence_material_id": pkg.members[evidence]}
                return pkg.members[evidence]
            if path.is_relative_to(JSON_ROOT):
                target = "source/third_party/native-overlay/json/include/" + path.relative_to(JSON_ROOT).as_posix()
            elif path.is_relative_to(SQLITE_ROOT):
                assert path.name == "sqlite3.h"
                target = "source/third_party/native-overlay/sqlite/sqlite3.h"
            elif path.is_relative_to(HDF5_ROOT):
                name = path.relative_to(HDF5_ROOT).as_posix()
                assert upstream(hdf_tar, "hdf5-2.2.0/src/" + name) == data
                target = "source/third_party/loom/hdf5/src/" + name
                pkg.licensed(target, data, "Unmodified release-src-hdf5 src/" + name, "BSD-3-Clause",
                    ["release-src-hdf5", hdf_notice], attribution="The HDF Group and contributors; full original notices retained", evidence="licenses/hdf5/LICENSE")
            elif path.is_relative_to(HDF5_GENERATED):
                assert path.name == "H5pubconf.h" and digest == HDF5_CONFIG_SHA
                assert b"This file is part of HDF5" in data and b"HDF Group" in data
                target = "source/third_party/loom/hdf5/generated/H5pubconf.h"
                pkg.licensed(target, data, "Exact frozen HDF5 configuration paired with static library " + HDF5_SHA,
                    "BSD-3-Clause", ["release-src-hdf5", hdf_notice, hdf_template_id], "derived_asset",
                    "The HDF Group and contributors; generated configuration retains original terms", "licenses/hdf5/LICENSE")
            elif path.is_relative_to(ZLIB_HEADERS):
                assert path.name in {"zlib.h", "zconf.h"}
                target = "source/third_party/loom/zlib/include/" + path.name
                if path.name == "zlib.h":
                    assert upstream(zlib_tar, "zlib-1.3.2/zlib.h") == data
                else:
                    assert digest == ZLIB_CONFIG_SHA
                pkg.licensed(target, data, "Exact frozen paired prefixed zlib1.3.2 interface " + path.name,
                    "Zlib", ["release-src-zlib", zlib_notice, cmake_zconf_id], "derived_asset" if path.name == "zconf.h" else "code",
                    "Jean-loup Gailly, Mark Adler and contributors; generated configuration identified", "licenses/zlib/LICENSE")
            elif path.is_relative_to(SDK / "include"):
                target = "source/frozen-sdk-interface/include/" + path.relative_to(SDK / "include").as_posix()
                known(target, data)
            elif path.is_relative_to(packet / "kit"):
                target = "source/format-plugin-kit/" + path.relative_to(packet / "kit").as_posix()
                known(target, data) if path.name == "LICENSE" else pkg.add(target, data, key)
            elif path.is_relative_to(cache):
                assert path.name == "format_plugin_profile.hpp", "Unclassified generated source: " + key
                target = "source/generated/" + path.relative_to(cache).as_posix()
                pkg.add(target, data, key)
            else:
                assert not key.startswith("build/"), "Unclassified dependency; no AGPL fallback: " + key
                target = "source/" + key
                if target not in pkg.files:
                    pkg.add(target, data, key, kind="asset" if path.suffix in {".md", ".json"} else "code")
            assert target in pkg.files and pkg.files[target] == data, "Missing/changed interface: " + target
            staged_inputs[key] = target
            return pkg.members[target]

        for name, digest in sorted(sources.items()):
            source_member(name, digest)
        extra = set(EXTRA)
        for folder in ("plugins/loom", "examples/loom_matrix_host", "examples/loom_matrix_plugin"):
            extra.update(p.relative_to(ROOT).as_posix() for p in (ROOT / folder).rglob("*") if p.is_file())
        for name in sorted(extra):
            path = project_path(name)
            source_member(relative(path), sources.get(relative(path), file_sha(path)))
        kit = load(packet / "kit-inputs.json")
        assert {p.relative_to(packet / "kit").as_posix() for p in (packet / "kit").rglob("*") if p.is_file()} == {r["member"] for r in kit["files"]}
        for row in kit["files"]:
            path = packet / "kit" / row["member"]
            assert path.stat().st_size == row["bytes"]
            source_member(str(path), row["sha256"])
        for name in ("reader_plugin.h", "format_plugin.h"):
            assert (packet / "kit/include/qupathxx" / name).read_bytes() == (ROOT / "cpp/include/qupathxx" / name).read_bytes()

        intake_path = packet / "reference-intake.json"
        intake = load(intake_path)
        assert intake["commit"] == "6177c213ab59110cea1bdaed08ac5a355fc13524" and len(intake["members"]) == 6
        assert not intake["schema_reference"]["retained_bytes"] and not intake["product_python_dependency"]
        assert not intake["upstream_data_model_or_sample_admitted"]
        reference_ids = []
        license_row = intake["members"]["LICENSE"]
        reference_root = packet / "references/loompy"
        # Older intake layouts use references/ directly; require the exact six
        # pinned files, never search/fetch a moving source tree.
        if not (reference_root / "LICENSE").is_file():
            reference_root = packet / "references"
        reference_license = checked(reference_root / "LICENSE", license_row["sha256"])
        reference_notice = pkg.licensed("licenses/loom-reference/LICENSE", reference_license, license_row["url"],
            "BSD-2-Clause", kind="notice", attribution="Linnarsson Lab and contributors", evidence=license_row["url"])
        for name, row in intake["members"].items():
            data = checked(reference_root / name, row["sha256"])
            assert len(data) == row["bytes"] and row["license_id"] == "BSD-2-Clause"
            reference_ids.append(pkg.licensed("source/references/loompy/" + name, data, row["url"], "BSD-2-Clause",
                [reference_notice], "notice" if name == "LICENSE" else "code",
                "Linnarsson Lab and contributors; original notices retained", "licenses/loom-reference/LICENSE"))

        base_availability = load(base_package / "FROZEN-SOURCE-AVAILABILITY.json")
        archives = []
        for path, record in ((hdf_archive, hdf_record), (zlib_archive, zlib_record)):
            archives.append({"path": str(path), "bytes": record["bytes"], "sha256": record["sha256"],
                "material_id": record["material"]["id"], "license_id": record["material"]["license_id"],
                "upstream": record.get("url") or record.get("source")})
        companion = {"schema": "qpx.loom-companion-availability/1",
            "a08": {"package": str(base_package), "native_library_sha256": A08_NATIVE_SHA,
                "source_receipt_sha256": file_sha(base_package / "SOURCE.json"),
                "admission_sha256": file_sha(base_admission_path), "qualification_sha256": file_sha(a08_result_path),
                "full_matching_source": "source/ tree in the exact separately required A08 package/ZIP",
                "package_zip": a08_zip, "scope": "Matched separate delivered product; not copied/rebuilt/requalified by M05"},
            "frozen_sdk": {"path": str(SDK), "core_sha256": BASE_CORE, "full_source": base_availability["first_party_source"],
                "abi_header": staged_inputs[relative(sdk_abi)], "source_availability_receipt_sha256": file_sha(base_package / "FROZEN-SOURCE-AVAILABILITY.json")},
            "private_static_libraries": {"hdf5": {"path": str(hdf_library), "bytes": hdf_library.stat().st_size, "sha256": HDF5_SHA,
                "license": "BSD-3-Clause with retained original component notices", "generated_config_sha256": HDF5_CONFIG_SHA},
                "zlib": {"path": str(zlib_library), "bytes": zlib_library.stat().st_size, "sha256": ZLIB_SHA,
                         "license": "Zlib", "generated_config_sha256": ZLIB_CONFIG_SHA}},
            "corresponding_dependency_sources": archives,
            "openslide_license_context": "Copied runtime binary record is LGPL-2.1-only; older source-archive aggregate is LGPL-2.1-or-later with19 individual component notices retained",
            "toolchain": compiled["profile"]}
        companion_id = pkg.add("COMPANIONS.json", encoded(companion),
            "Verified separate required A08/FP04/static dependency source availability and actual component terms",
            ["qupathxx-root-license", base_admission["members"]["native/ngff_native.lib"]["material_id"], "release-src-hdf5", "release-src-zlib"], "asset")
        native_id = pkg.add("native/" + native.name, products[native.name], "Qualified six-unit typed matrix application addon",
            [companion_id, *(source_member(n, sources[n]) for n in groups["qupathxx_loom_matrix_native"])], "derived_asset")
        module_id = pkg.add("bin/" + module.name, products[module.name], "Qualified optional native Loom3 matrix ABI2 provider",
            [companion_id, *(source_member(n, sources[n]) for n in groups["qpx_loom_plugin"]),
             *(pkg.members[n] for n in pkg.files if n.startswith("source/format-plugin-kit/"))], "derived_asset")
        cli_id = pkg.add("bin/" + cli.name, products[cli.name], "Qualified thin native matrix/annotation CLI",
            [native_id, *(source_member(n, sources[n]) for n in groups["loom_format_cli"])], "derived_asset")
        consumer_id = pkg.add("bin/" + consumer.name, products[consumer.name], "Qualified public native matrix SDK consumer",
            [native_id, *(source_member(n, sources[n]) for n in groups["loom_matrix_consumer"])], "derived_asset")
        pkg.add("bin/" + manifest.name, products[manifest.name], "Exact qualified optional Loom manifest", [module_id], "asset")
        template = (ROOT / "cmake/qpx_loom_matrixConfig.cmake.in").read_bytes()
        placeholder = b"@QPX_LOOM_MATRIX_NATIVE_SHA256@"
        assert template.count(placeholder) == 1
        config = template.replace(placeholder, NATIVE_SHA.encode("ascii"))
        assert str(ROOT).encode().lower() not in config.lower()
        pkg.add("lib/cmake/qpx_loom_matrix/qpx_loom_matrixConfig.cmake", config,
            "Relocatable matched typed matrix SDK config; exact native SHA substitution", [native_id])
        for name, path, parents in [("compiled-inputs.json", compiled_path, [native_id, module_id]),
            ("integration-primary.json", primary_path, [native_id, module_id]),
            ("integration-completion.json", validation_path, [cli_id, consumer_id]),
            ("native-reference.json", reference_path, [native_id]), ("reference-intake.json", intake_path, reference_ids),
            ("kit-inputs.json", packet / "kit-inputs.json", [module_id])]:
            pkg.add("source/receipts/" + name, path.read_bytes(), "Exact source/native/reference qualification receipt " + str(path), parents, "asset")

        notice = """M05 provider, shared HDF5/reader/JSON helpers, six affected application services, examples, tests and development controls: AGPL-3.0-only, qupathxx contributors. Exact matching source is under source/; full application license is licenses/qupathxx/LICENSE.
HDF52.2.0 source interface and frozen generated H5pubconf.h: original BSD-3-Clause terms plus retained notices, The HDF Group and contributors. Full original licenses/hdf5/LICENSE is retained. Generated configuration is identified, not represented as an unmodified upstream file. The exact frozen HDF5 static library and corresponding admitted source archive remain separate prerequisites in COMPANIONS.json.
Prefixed zlib1.3.2: Zlib, Jean-loup Gailly, Mark Adler and contributors. Original zlib.h, source/configuration recipe and the paired generated zconf.h are retained under source/third_party/loom/zlib; full notice is licenses/zlib/LICENSE. Configuration changes are identified. The separate frozen static library and exact source archive retain their actual terms.
Native application JSON3.12.0 interfaces: MIT, Niels Lohmann and per-header contributors. SQLite3.54.1 interface: public-domain author declaration/blessing, SQLite authors. Exact original headers/notices are retained independently of application AGPL terms.
The six pinned loompy3.0.6 source/notice references are BSD-2-Clause, Linnarsson Lab and contributors. They are retained as nonexecuted semantic/source context under source/references/loompy, with full license under licenses/loom-reference; no product Python implementation, fallback or dependency is supplied. Schema prose/third-party introduction is referenced metadata-only. No upstream dataset, model, sample or restricted derivative is admitted.
Exact GEOS runtime DLLs retain LGPL-2.1-or-later; the actual OpenSlide DLL binary record is LGPL-2.1-only. All19 original OpenSlide component notices remain under licenses/openslide-sdk; its older source-archive aggregate label does not relicense components. Remaining copied original SDK notices retain their actual terms.
The matched A08 addon/lib/full source/packageZIP and frozen FP04 SDK/core/full source/dependency sources remain separate required products, not rebuilt or requalified here. Distribution must preserve applicable AGPL corresponding source and each dependency's actual notices/terms, including LGPL replacement/relinking rights where applicable. External Windows/MSVC/compiler/CRT prerequisites follow the recorded Release /MD profile; Microsoft source/redistributables are not packaged.
"""
        pkg.add("NOTICE.txt", notice.encode("utf8"), "Actual M05/reference/dependency/source distribution terms", kind="notice")
        guide = """# Optional native Loom matrix packet

Read source/plugins/loom/README.md and source/docs/benchmarks/loom/README.md for
the selected Loom3 matrix/layer, explicit IDs/domain/frame, bounded HDF5 and
native QMX contracts. The provider is matrix ABI2; native typed receipts use
schema5, with existing ABI1/schema4 identities preserved independently.

Keep bin/qpx_loom_plugin.dll with bin/loom-format-plugins.json and the exact
runtime DLLs. Use an absolute manifest path, for example:
  bin/qupathxx_format_import.exe --format-plugins <absolute-manifest> catalog
The CLI imports through matrix-index; source/docs and the source public consumer
show explicit JSON options and coordinate-TSV choices. No Python/Java runtime is
required. Source/reference Python files are never product implementations.

COMPANIONS.json identifies the separately required matched A08 package/lib/source
ZIP and frozen FP04 SDK/core/dependency source. It also records the unchanged
HDF5/zlib static library and generated interface identities. sdk_abi.hpp remains
external in the frozen SDK; eligible template/recipe/DERIVATION evidence is
included, with its observed exact input hash. Old SDK graphs are not rewritten.

Installed SDK: configure source/examples/loom_matrix_plugin in a fresh cache,
CMAKE_PREFIX_PATH="<M05 packet>;<A08 packet>;<frozen FP04 SDK>", Windows x64
MSVC1944/compiler19.44.35225.0/toolset14.44.35207, Release /MD. The relocatable
qpx_loom_matrix config verifies the new native library and imports the exact A08
addon/core. Native application source rebuild uses source/examples/loom_matrix_host
with CMAKE_PREFIX_PATH="<A08 packet>;<frozen FP04 SDK>"; its six sources link the
retained addon and do not rebuild it. JSON/SQLite headers default to the supplied
source/third_party/native-overlay directories. Rebuilds get separate identities.

Provider source rebuild uses source/plugins/loom, the matching frozen SDK prefix
and QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=<M05 packet>/source/format-plugin-kit.
Set QUPATHXX_LOOM_HDF5_INCLUDE=<M05 packet>/source/third_party/loom/hdf5/src,
QUPATHXX_LOOM_HDF5_GENERATED_INCLUDE=<M05 packet>/source/third_party/loom/hdf5/generated
and QUPATHXX_LOOM_ZLIB_INCLUDE=<M05 packet>/source/third_party/loom/zlib/include.
Use the declared exact compiler/Release profile; no downloads/static-library
rebuild is performed. See source/plugins/format-common for shared authorities.

SOURCE.json records source/binary/qualification identities; PACKAGE.json owns
exact staged files. This writer creates no ZIP and claims no install/removal
acceptance. Separate delivery checks follow staging. Close module users before
removing verified owned paths; preserve research inputs, QMX/projects/results/
notes and all separate companions. This is a selected local workflow, with no
biological, general Loom, GUI, platform or full-goal acceptance claim.
"""
        pkg.add("README.md", guide.encode("utf8"), "Native install/use/source/rebuild/removal and support scope", kind="asset")
        module_inputs = {n: sources[n] for n in groups["qpx_loom_plugin"]}
        metadata = {"schema": "qpx.loom-delivery/1", "profile": PROFILE,
            "providers": {PROVIDER: {"module_sha256": sha(products[module.name]), "version": "1.0.0", "contract": CONTRACT,
                "abi": 2, "domain": "matrix", "source_context_sha256": context(module_inputs), "source_inputs": module_inputs,
                "licenses": ["AGPL-3.0-only", "BSD-3-Clause", "Zlib"], "dependencies": [],
                "validation_sha256": file_sha(validation_path), "reused_primary_sha256": file_sha(primary_path)}},
            "native_library_sha256": NATIVE_SHA, "cli_sha256": sha(products[cli.name]),
            "sdk_consumer_sha256": sha(products[consumer.name]), "compiled_inputs_sha256": file_sha(compiled_path),
            "compiled_context_sha256": context({"sources": sources, "groups": groups, "profile": compiled["profile"], "commands": compiled["commands"]}),
            "host_inputs": {n: sources[n] for n in groups["qupathxx_loom_matrix_native"]},
            "compiled_input_stage_paths": staged_inputs, "companions_sha256": sha(encoded(companion)),
            "retained_application_addon_sha256": A08_NATIVE_SHA, "base_core_sha256": BASE_CORE,
            "component_terms": {"new_code": "AGPL-3.0-only", "hdf5": "BSD-3-Clause with retained original notices",
                "zlib": "Zlib", "json": "MIT", "sqlite": "blessing", "reference_sources": "BSD-2-Clause",
                "openslide_binary": "LGPL-2.1-only"},
            "qualified_checks": {"reused_primary": sorted(REUSED_CHECKS), "completed": sorted(FINAL_CHECKS)},
            "prior_inventory_groups": len(before), "prior_groups_unchanged": True,
            "scope": "Selected native Loom typed UInt64/Float64 import/QMX/project/CLI/SDK; separate A08/FP04 companions, no general-format/UI/biological/platform/full-goal claim",
            "install_or_removal_qualification": "Separate delivery check follows staging", "archive_created": False}
        # All processing remains in memory until the existing package writer
        # owns the files and performs its inventory/member admission check.
        assert all(load(inventory_path).get(name) == value for name, value in before.items())
        result = pkg.finish(output, "loom-" + sha(products[module.name])[:16] + "-" + context(sources)[:8],
                            metadata, inventory_path, args.write_inventory)
        after = load(inventory_path)
        assert all(after.get(name) == value for name, value in before.items())
        assert len(after) == len(before) + 1
        print(json.dumps({"files": result["files"], "admission_exit": result["admission_exit"], "output": str(output)}))
    finally:
        hdf_tar.close()
        zlib_tar.close()


if __name__ == "__main__":
    main()
