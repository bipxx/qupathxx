"""Stage the exact qualified M07 AnnData-Zarr/native packet without building.

The matching M05/A08/FP04 products and their sources remain separate required
companions. This development writer uses the existing material inventory and
admission authority; deployment, removal and ZIP qualification follow staging.
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
from stage_loom import (NATIVE_SHA as M05_NATIVE_SHA, JSON_ROOT, SQLITE_ROOT,
                       ZLIB_HEADERS, ZLIB_CONFIG_SHA, ZLIB_SHA,
                       HDF5_ROOT, HDF5_GENERATED)


PROVIDER = "org.qupathxx.anndata-zarr"
CONTRACT = "anndata-zarr2-typed/1"
PROFILE = "windows-x64-msvc1944-release-md"
GROUP = "anndata_zarr_sparse_matrix_plugin"
VERSION = "1.2.0"
MATRIX_ABI = 4
BLOSC = ROOT / "build/gui-aggregation/gap-closure"
BLOSC_LIBRARY = BLOSC / "omero-raw-build/blosc-bytes/Release/qpx_blosc.lib"
BLOSC_ARCHIVE = BLOSC / "inputs/c-blosc-v1.21.6.tar.gz"
BLOSC_HEADERS = BLOSC / "inputs/c-blosc/blosc"
BLOSC_SHA = "804c02c3b167de146c3c18b7caa3511b237f23fafbbef8fb12936736618c4cf6"
BLOSC_TERMS = "BSD-3-Clause AND BSD-2-Clause AND MIT AND Zlib"
HOST_TARGET = "qupathxx_anndata_zarr_native"
MODULE_TARGET = "qpx_anndata_zarr_plugin"
CLI_TARGET = "anndata_format_cli"
CONSUMER_TARGET = "anndata_zarr_consumer"
HOST_UNITS = {"cpp/directory_store.cpp", "cpp/format_plugin_loader.cpp",
              "cpp/format_plugin_runtime.cpp", "cpp/format_import.cpp",
              "cpp/expression_matrix_attach.cpp",
              "cpp/expression_matrix_metadata.cpp", "cpp/expression_matrix_writer.cpp"}
TARGETS = {HOST_TARGET, MODULE_TARGET, CLI_TARGET, CONSUMER_TARGET,
           "anndata_zarr_tests", "anndata_zarr_offline_tests", "anndata_matrix_control",
           "anndata_c_header_tests", "anndata_legacy_host_tests",
           "anndata_legacy_import_tests", "anndata_loom_tests"}
REQUIRED_CHECKS = {"c-header", "catalog", "actual-workflow", "provider-free-metadata",
    "provider-free-import", "loader-off-metadata", "loader-off-import", "store-controls",
    "legacy-schema4-exact", "legacy-schema5-exact", "legacy-schema5-continuous",
    "upstream-zlib", "upstream-default-blosc", "conflict", "truncated-blosc",
    "compressed-lz4", "compressed-blosclz", "sdk-default-blosc", "sdk-reopen-default-blosc",
    "upstream-wide-csc", "previous-abi3-store-exact", "attach-sparse-without-provider",
    "upstream-moderate-csr", "sdk-reject-project-inside-store",
    "sdk-reject-input-managed-inside-store", "sdk-inspect-counts", "sdk-inspect-continuous",
    *(f"{prefix}-{kind}{suffix}" for kind in ("counts", "continuous")
      for prefix, suffix in (("cli", ""), ("cli", "-info"), ("cli", "-existing"), ("sdk", "")))}
EXPECTED_REFUSALS = {"cli-counts-existing", "cli-continuous-existing",
    "truncated-blosc", "conflict",
    "sdk-reject-project-inside-store", "sdk-reject-input-managed-inside-store"}
EXTRA = {"cmake/qpx_anndata_zarrConfig.cmake.in", "cmake/PinnedZlib.cmake",
    "docs/benchmarks/anndata-zarr/README.md", "docs/format-family-plugins.md",
    "docs/format-family-catalogs.md", "docs/reader-license-policy.md",
    "plugins/format-common/binary_input.hpp", "plugins/format-common/reader_input.hpp",
    "plugins/format-common/json_tree.hpp", "plugins/format-common/zarr_input.hpp",
    "plugins/format-common/plugin_build.cmake", "plugins/format-common/write_manifest.cmake",
    "plugins/format-common/format_plugin_profile.hpp.in", "tools/ci/check_anndata_zarr.py",
    "tools/ci/check_anndata_zarr_delivery.py", "tools/ci/collect_anndata_zarr_inputs.py",
    "tools/ci/check_loom_delivery.py",
    "tools/ci/native_build_inputs.py", "tools/ci/stage_anndata_zarr.py",
    "tools/ci/stage_loom.py", "tools/ci/stage_ngff_labels.py",
    "tools/ci/stage_annotation_sources.py", "tools/ci/stage_annotation_geometry.py",
    "tools/ci/check_material_staging.py", "tools/ci/make_anndata_zarr_producer_fixture.py",
    "plugins/blosc-bytes/CMakeLists.txt", "plugins/blosc-bytes/source-lock.json",
    "plugins/blosc-bytes/reader_plugin_profile.hpp.in", "plugins/blosc-bytes/write_manifest.cmake",
    "plugins/blosc-bytes/blosc_reader.cpp"}
REFERENCE_PINS = {
    ("anndata", "LICENSE"): ("b796d59fea77f5e068d4c0ed8bb9d3f7c5316c19", "BSD-3-Clause",
        "55cad7a0454c86d36ebccbca6063fe23994c4fca99ffa7457f6d9fb0bd7cd2a0"),
    ("anndata", "docs/fileformat-prose.md"): ("b796d59fea77f5e068d4c0ed8bb9d3f7c5316c19", "BSD-3-Clause",
        "10cd151082eec951c32e08ce72f73a17f924714f52d99038a1bb42efbbdd99ad"),
    ("zarr-specs", "LICENSE"): ("6e42439084ca766e80ca0877fed9e0d076905cba", "CC-BY-4.0",
        "fe7b4ce83b8381cc5b216bbb4af73c570688d1b819c73bbaed8ca401f4677cd6"),
    ("zarr-specs", "docs/v2/v2.0.rst"): ("6e42439084ca766e80ca0877fed9e0d076905cba", "CC-BY-4.0",
        "422bd5f5fb27269bcc30da125f0b116c29e5008a5360bb5a5bb77f14b93acea7"),
    ("numcodecs", "LICENSE.txt"): ("3cf8ab1f28e6fbda24fa16c33654cf560a8d4ae2", "MIT",
        "949cac68479e13b6c97924635150b4e1ef5e69766ad9e472ea85a08c1576bb96"),
    ("numcodecs", "numcodecs/vlen.pyx"): ("3cf8ab1f28e6fbda24fa16c33654cf560a8d4ae2", "MIT",
        "ee07175b0109eb6ac563c2f266e43f480341396cafbf8d38c067af16bc3654cb"),
}


class AnnDataPackage(Package):
    def licensed(self, name, data, origin, license_id, parents=(), kind="code",
                 attribution="qupathxx contributors", evidence="LICENSE; repository author declaration"):
        parents = list(dict.fromkeys(parents))
        suffix = "-" + context(parents)[:12] if parents and parents != ["qupathxx-root-license"] else ""
        ident = "anndata-zarr-" + license_id + "-" + kind + "-" + sha(data)[:24] + suffix
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
            "material": {"id": ident, "category": "admitted_open", "kind": kind, "license_id": license_id,
                "license_evidence": evidence, "attribution": attribution, "source_ids": parents}}
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


def qualification(path, compiled, module, manifest, cli, consumer):
    value = load(path)
    assert value["passed"] and value["native_only_environment"], "Native completion is not qualified"
    checks = dict(value["checks"])
    retained_receipts = []
    if "reused" in value:
        reused = value["reused"]
        prior_path = Path(reused["receipt"]).resolve()
        assert file_sha(prior_path) == reused["sha256"]
        prior = load(prior_path)
        assert prior["native_only_environment"]
        for name in reused["passed_checks"]:
            assert name not in checks, "Duplicate qualified command: " + name
            row = prior["checks"][name]
            expected = row["expected_returncode"] if "expected_returncode" in row else row["expected_exit"]
            assert row["returncode"] == expected, name
            checks[name] = row
        retained_receipts.append(prior_path)
        for name, digest in reused.get("verified_binaries", {}).items():
            checked(project_path(name), digest)
    assert REQUIRED_CHECKS <= set(checks), "Missing qualified routes: " + str(sorted(REQUIRED_CHECKS - set(checks)))
    for name, row in checks.items():
        expected = 2 if name in EXPECTED_REFUSALS else 0
        recorded = row["expected_returncode"] if "expected_returncode" in row else row["expected_exit"]
        if "expected_returncode" in row and "expected_exit" in row:
            assert row["expected_returncode"] == row["expected_exit"], name
        assert recorded == expected and row["returncode"] == expected, name
    binaries = value["binaries"]
    for name, digest in binaries.items():
        checked(project_path(name), digest)
        key = relative(project_path(name))
        if key in compiled["products"]:
            assert compiled["products"][key] == digest, "Qualified product context differs: " + name
    for target in (module, cli, consumer):
        assert binaries[str(target)] == compiled["products"][relative(target)], target
    assert value["provider_manifest"] == {"path": str(manifest), "sha256": file_sha(manifest)}
    for kind in ("counts", "continuous"):
        for suffix in ("", "-info", "-existing"):
            assert Path(checks[f"cli-{kind}{suffix}"]["argv"][0]).resolve() == cli
        assert Path(checks[f"sdk-{kind}"]["argv"][0]).resolve() == consumer
        assert Path(checks[f"sdk-{kind}"]["argv"][1]).resolve() == manifest
        assert Path(checks[f"sdk-inspect-{kind}"]["argv"][0]).resolve() == consumer
        inspect_args = checks[f"sdk-inspect-{kind}"]["argv"]
        assert inspect_args[1] == "--inspect" and len(inspect_args) == 9
        assert all(len(value) == 64 and not set(value) - set("0123456789abcdef")
                   for value in inspect_args[-4:]), "SDK inspection lacks exact identity arguments"
    for name in ("upstream-zlib", "upstream-default-blosc", "conflict", "upstream-moderate-csr",
                 "compressed-lz4", "compressed-blosclz", "truncated-blosc", "upstream-wide-csc",
                 "previous-abi3-store-exact", "attach-sparse-without-provider"):
        assert Path(checks[name]["argv"][0]).resolve() == cli
    for name in ("sdk-reject-project-inside-store", "sdk-reject-input-managed-inside-store",
                 "sdk-default-blosc", "sdk-reopen-default-blosc"):
        assert Path(checks[name]["argv"][0]).resolve() == consumer
    reference_candidates = []
    if "native_reference_path" in value:
        reference_candidates.append(Path(value["native_reference_path"]))
    work = Path(checks["actual-workflow"]["argv"][-1]).resolve()
    reference_candidates.extend([work / "native-reference.json", work / "fixtures/native-reference.json",
                                 path.parent / "fixtures/native-reference.json"])
    reference = next((p.resolve() for p in reference_candidates
                      if p.is_file() and file_sha(p) == value["native_reference_sha256"]), None)
    assert reference is not None, "Matching original native reference is unavailable"
    producer = value["producer_reference"]
    producer_path = project_path(producer["path"])
    producer_bytes = checked(producer_path, producer["sha256"])
    assert producer_path.suffix == ".json" and len(producer_bytes) <= 4 * 1024 * 1024
    assert isinstance(json.loads(producer_bytes), dict), "Producer evidence must be metadata JSON"
    return value, checks, reference, retained_receipts, producer_path


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
    base = (args.base_package or ROOT / "build/gui-aggregation/catalogue-closure/loom/package-final").resolve()
    base_admission_path = (args.base_admission or base.parent / (base.name + "-admission.json")).resolve()
    assert not output.exists(), "Use a fresh stage; preserve prior evidence"
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory, before = load(inventory_path), load(packet / "before-sparse-material-inventory.json")
    assert before and GROUP not in before
    assert all(inventory.get(name) == value for name, value in before.items()), "Prior inventory group changed"
    assert set(inventory) <= {*before, GROUP}, "Unexpected concurrent inventory group"
    entries = read_inventory([inventory_path])
    pkg = AnnDataPackage(inventory,
        "Selected native AnnData-Zarr2 directory matrix ABI4/schema6 counts/continuous/QMX/project/CLI/SDK workflow; "
        "exact separate M05/A08/FP04 companions, unchanged ABI1/2 identities, original component/reference terms; "
        "no Python runtime, upstream data/model/fixture, restricted10x, GUI/general-format/full-goal/publication claim", GROUP)
    by_hash = {}
    for ident, record in entries.items():
        by_hash.setdefault(record["sha256"], []).append(ident)

    def known(name, data, preferred=None):
        ids = [i for i in by_hash.get(sha(data), []) if entries[i].get("bytes") == len(data)]
        if preferred:
            assert preferred in ids, "Required exact material absent: " + preferred
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

    compiled_path = packet / "compiled-sparse-inputs.json"
    compiled = load(compiled_path)
    assert compiled["schema"] == "qpx.anndata-zarr-native-inputs/1"
    assert compiled["profile"] == {"msvc": 1944, "compiler": "19.44.35225.0", "toolset": "14.44.35207",
                                    "configuration": "Release", "runtime": "MD", "cpp": 17}
    sources = {}
    for name, digest in compiled["sources"].items():
        key = relative(project_path(name))
        assert key not in sources or sources[key] == digest, "Conflicting source case alias: " + key
        checked(project_path(name), digest)
        sources[key] = digest
    groups = {name: [relative(project_path(n)) for n in names] for name, names in compiled["groups"].items()}
    assert set(groups) == TARGETS and all(n in sources for rows in groups.values() for n in rows)
    assert {n for n in groups[HOST_TARGET] if n.endswith((".cpp", ".c"))} == HOST_UNITS
    assert {"plugins/anndata_zarr/anndata_zarr.cpp", "plugins/format-common/zarr_input.hpp",
            "plugins/format-common/json_tree.hpp", "plugins/format-common/binary_input.hpp"} <= set(groups[MODULE_TARGET])
    assert not any(project_path(n).is_relative_to(HDF5_ROOT) or project_path(n).is_relative_to(HDF5_GENERATED)
                   for n in groups[MODULE_TARGET]), \
        "Provider unexpectedly depends on an HDF interface"
    for command in compiled["commands"].values():
        assert file_sha(ROOT / command["path"]) == command["sha256"], "Compiler command changed"
    for name, digest in compiled["products"].items():
        checked(project_path(name), digest)

    def product(path):
        return checked(path, compiled["products"][relative(path)])

    binaries = cache / "anndata-zarr/Release"
    module, manifest = binaries / "qpx_anndata_zarr_plugin.dll", binaries / "anndata-zarr-format-plugins.json"
    cli, consumer = binaries / "qupathxx_format_import.exe", binaries / "anndata_zarr_consumer.exe"
    native = cache / "anndata-zarr-host/Release/qupathxx_anndata_zarr_native.lib"
    products = {p.name: product(p) for p in (module, manifest, cli, consumer, native)}
    native_sha = sha(products[native.name])
    candidates = sorted(packet.glob("integration-*/results.json"), key=lambda p: p.stat().st_mtime_ns, reverse=True)
    validation_path = args.validation.resolve() if args.validation else next(
        (p for p in candidates if load(p).get("passed")), None)
    assert validation_path is not None, "A completed native qualification receipt is required"
    validation, checks, reference_path, retained_receipts, producer_path = qualification(
        validation_path, compiled, module, manifest, cli, consumer)
    definition = json.loads(products[manifest.name])
    assert definition["schema"] == "qpx.format-plugins/1" and len(definition["plugins"]) == 1
    descriptor = definition["plugins"][0]
    assert tuple(descriptor[n] for n in ("provider_id", "format_id", "domain", "version", "contract", "abi", "profile")) == \
        (PROVIDER, "anndata-zarr", "matrix", VERSION, CONTRACT, MATRIX_ABI, PROFILE)
    assert descriptor["module"] == {"path": module.name, "sha256": sha(products[module.name])}
    assert descriptor["dependencies"] == []

    base_admission, base_source, m05_result = load(base_admission_path), load(base / "SOURCE.json"), load(base.parent / "results.json")
    assert base_admission["admission_exit"] == 0 and m05_result["passed"]
    assert base_source["native_library_sha256"] == m05_result["native_library_sha256"] == M05_NATIVE_SHA
    checked(base / "native/qupathxx_loom_matrix_native.lib", M05_NATIVE_SHA)
    for name, row in base_admission["members"].items():
        data = checked(base / name, row["sha256"])
        assert len(data) == row["bytes"], "Retained M05 member changed: " + name
    m05_zip = m05_result["package_zip"]
    assert m05_zip["passed"] and m05_zip["all_paths_and_material_identities_match"]
    assert checked(Path(m05_zip["path"]), m05_zip["sha256"]) and Path(m05_zip["path"]).stat().st_size == m05_zip["bytes"]
    base_companion = load(base / "COMPANIONS.json")
    a08 = Path(base_companion["a08"]["package"]).resolve()
    a08_admission_path = a08.parent / (a08.name + "-admission.json")
    a08_admission, a08_source = load(a08_admission_path), load(a08 / "SOURCE.json")
    assert a08_admission["admission_exit"] == 0
    assert file_sha(a08_admission_path) == base_companion["a08"]["admission_sha256"]
    assert file_sha(a08 / "SOURCE.json") == base_companion["a08"]["source_receipt_sha256"]
    assert a08_source["native_library_sha256"] == file_sha(a08 / "native/ngff_native.lib") == A08_NATIVE_SHA
    a08_result_path = a08.parent / "results.json"
    assert file_sha(a08_result_path) == base_companion["a08"]["qualification_sha256"]
    assert load(a08_result_path)["passed"]
    a08_zip = base_companion["a08"]["package_zip"]
    assert a08_zip["passed"] and a08_zip["all_member_paths_and_material_identities_match"]
    assert Path(a08_zip["path"]).stat().st_size == a08_zip["bytes"]
    assert file_sha(Path(a08_zip["path"])) == a08_zip["sha256"]
    assert file_sha(SDK / "lib/qupathxx/qupathxx_core.lib") == BASE_CORE
    assert file_sha(BASE_SOURCE) == BASE_SOURCE_SHA
    availability = a08 / "FROZEN-SOURCE-AVAILABILITY.json"
    assert file_sha(availability) == base_companion["frozen_sdk"]["source_availability_receipt_sha256"]
    availability_value = load(availability)
    for row in (availability_value["first_party_source"], *availability_value["upstream_sources"]):
        path = Path(row.get("path", row.get("local_path")))
        assert path.stat().st_size == row["bytes"] and file_sha(path) == row["sha256"], \
            "Required corresponding source is unavailable/changed: " + str(path)
        if "material_id" in row:
            admitted(row["material_id"], entries)
    for name in base_admission["members"]:
        if name.startswith(("source/third_party/native-overlay/json/", "source/third_party/native-overlay/sqlite/", "licenses/")) \
                and not name.startswith("licenses/loom-reference/"):
            pkg.borrow(name, base, base_admission)
    for name in ("source/LICENSE", "source/frozen-sdk-interface/cmake/sdk_abi.hpp.in",
                 "source/frozen-sdk-interface/cmake/NativeSdk.cmake", "source/frozen-sdk-interface/DERIVATION.json"):
        pkg.borrow(name, base, base_admission)
    sdk_abi = SDK / "include/qupathxx/sdk_abi.hpp"
    abi_bytes = checked(sdk_abi, SDK_ABI_SHA)
    derivation = load(base / "source/frozen-sdk-interface/DERIVATION.json")
    assert derivation["header"]["sha256"] == SDK_ABI_SHA and derivation["header"]["bytes"] == len(abi_bytes) == 500
    abi_template, abi_recipe = (pkg.files["source/frozen-sdk-interface/cmake/" + n]
                                for n in ("sdk_abi.hpp.in", "NativeSdk.cmake"))
    with zipfile.ZipFile(BASE_SOURCE) as archive:
        assert archive.read("cmake/sdk_abi.hpp.in") == abi_template and archive.read("cmake/NativeSdk.cmake") == abi_recipe
    assert abi_template.count(b"@MSVC_VERSION@") == 1 and abi_template.replace(b"@MSVC_VERSION@", b"1944") == abi_bytes
    for name, digest in RUNTIMES.items():
        data = product(binaries / name)
        assert sha(data) == digest
        pkg.borrow("bin/" + name, base, base_admission)
        assert pkg.files["bin/" + name] == data

    lock = load(ROOT / "plugins/anndata_zarr/source-lock.json")
    assert lock["schema"] == "qpx.anndata-zarr-native-source-profile/1"
    assert (lock["provider"], lock["contract"], lock["abi"]) == (PROVIDER, CONTRACT, MATRIX_ABI)
    zlib_record = entries["release-src-zlib"]
    admitted("release-src-zlib", entries)
    zlib_paths = [ROOT / zlib_record["filename"], ROOT / "build/deps" / Path(zlib_record["filename"]).name,
                  ROOT / "build/dependency-downloads" / Path(zlib_record["filename"]).name]
    zlib_archive = next((p for p in zlib_paths if p.is_file() and file_sha(p) == zlib_record["sha256"]), None)
    assert zlib_archive is not None, "Matching zlib source is unavailable; no download"
    assert lock["zlib"]["source_archive_sha256"] == zlib_record["sha256"]
    zlib_library = SDK / "lib/qupathxx/deps/qpx-zlib.lib"
    assert file_sha(zlib_library) == lock["zlib"]["static_library_sha256"] == ZLIB_SHA
    blosc_dir = ROOT / "plugins/blosc-bytes"
    blosc_lock = load(blosc_dir / "source-lock.json")
    assert file_sha(BLOSC_LIBRARY) == BLOSC_SHA
    assert file_sha(BLOSC_ARCHIVE) == blosc_lock["archive_sha256"]
    with tarfile.open(BLOSC_ARCHIVE) as archive:
        for name in blosc_lock["members"]:
            member = archive.getmember("c-blosc-1.21.6/" + name)
            assert member.isfile() and sha(archive.extractfile(member).read()) == blosc_lock["members"][name]
    blosc_notice = pkg.licensed("licenses/blosc/THIRD_PARTY_NOTICES.txt",
        (blosc_dir / "THIRD_PARTY_NOTICES.txt").read_bytes(), "Pinned C-Blosc byte provider component notices",
        BLOSC_TERMS, kind="notice", attribution="C-Blosc and bundled component contributors",
        evidence=blosc_lock["archive"])
    staged_inputs = {}
    with tarfile.open(zlib_archive) as zlib_tar:
        def upstream(name):
            member = zlib_tar.getmember("zlib-1.3.2/" + name)
            assert member.isfile()
            return zlib_tar.extractfile(member).read()

        assert upstream("LICENSE") == pkg.files["licenses/zlib/LICENSE"]
        zlib_stage = "source/third_party/anndata-zarr/zlib/"
        for name in ("CMakeLists.txt", "zconf.h"):
            known(zlib_stage + "source/" + name, upstream(name))
        original = upstream("zconf.h")
        template = (ZLIB_HEADERS.parent / "zconf.h.cmakein").read_bytes()
        normalized = lambda b: b.replace(b"\r\n", b"\n")
        expected = original[:245] + b"#cmakedefine Z_PREFIX 1\n#cmakedefine HAVE_STDARG_H 1\n#cmakedefine HAVE_UNISTD_H 1\n" + original[244:]
        assert normalized(template) == normalized(expected)
        configured = normalized(template).replace(b"#cmakedefine Z_PREFIX 1", b"#define Z_PREFIX 1").replace(
            b"#cmakedefine HAVE_STDARG_H 1", b"#define HAVE_STDARG_H 1").replace(
            b"#cmakedefine HAVE_UNISTD_H 1", b"/* #undef HAVE_UNISTD_H */")
        assert checked(ZLIB_HEADERS / "zconf.h", ZLIB_CONFIG_SHA) == configured.replace(b"\n", b"\r\n")
        assert (ZLIB_HEADERS / "zconf.h").read_bytes() == (ZLIB_HEADERS.parent / "zconf.h").read_bytes()
        known(zlib_stage + "source/zconf.h.cmakein", template)

        def source_member(name, digest):
            path, key = project_path(name), relative(project_path(name))
            if key in staged_inputs:
                item = staged_inputs[key]
                if isinstance(item, dict):
                    assert item["external"] and item["sha256"] == digest
                    return item["source_evidence_material_id"]
                assert sha(pkg.files[item]) == digest, "Changed staged input: " + key
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
                known(target, data)
            elif path.is_relative_to(SQLITE_ROOT):
                assert path.name == "sqlite3.h"
                target = "source/third_party/native-overlay/sqlite/sqlite3.h"
                known(target, data)
            elif path.is_relative_to(ZLIB_HEADERS):
                assert path.name in {"zlib.h", "zconf.h"}
                if path.name == "zlib.h":
                    assert upstream("zlib.h") == data
                else:
                    assert digest == ZLIB_CONFIG_SHA
                target = zlib_stage + "include/" + path.name
                known(target, data)
            elif path.is_relative_to(BLOSC_HEADERS):
                assert path.name in {"blosc.h", "blosc-export.h"}
                assert digest == blosc_lock["members"]["blosc/" + path.name]
                target = "source/third_party/anndata-zarr/blosc/include/" + path.name
                pkg.licensed(target, data, blosc_lock["archive"], "BSD-3-Clause", [blosc_notice],
                    attribution="C-Blosc contributors", evidence="C-Blosc LICENSE.txt in the pinned source archive")
            elif path.is_relative_to(HDF5_ROOT) or path.is_relative_to(HDF5_GENERATED):
                # Only the retained Loom regression oracle reads these headers.
                # Reuse exact M05 member IDs/derivations, never relabel them as
                # application code or imply HDF linkage in the new provider.
                if path.is_relative_to(HDF5_ROOT):
                    branch, leaf = "src", path.relative_to(HDF5_ROOT).as_posix()
                else:
                    branch, leaf = "generated", path.relative_to(HDF5_GENERATED).as_posix()
                    assert leaf == "H5pubconf.h"
                original_name = "source/third_party/loom/hdf5/" + branch + "/" + leaf
                record = base_admission["members"][original_name]
                assert record["sha256"] == digest and record["bytes"] == len(data)
                assert checked(base / original_name, digest) == data
                target = "source/third_party/retained-loom/hdf5/" + branch + "/" + leaf
                known(target, data, record["material_id"])
            elif path.is_relative_to(SDK / "include"):
                target = "source/frozen-sdk-interface/include/" + path.relative_to(SDK / "include").as_posix()
                known(target, data)
            elif path.is_relative_to(base) or path.is_relative_to(a08):
                parent = base if path.is_relative_to(base) else a08
                parent_admission = base_admission if parent == base else a08_admission
                original_name = path.relative_to(parent).as_posix()
                assert original_name.startswith("source/"), "Unexpected compiled retained binary"
                target = "source/retained-" + ("m05" if parent == base else "a08") + "-interface/" + original_name
                known(target, data, parent_admission["members"][original_name]["material_id"])
            elif path.is_relative_to(packet / "kit-sparse"):
                target = "source/format-plugin-kit/" + path.relative_to(packet / "kit-sparse").as_posix()
                known(target, data) if path.name == "LICENSE" else pkg.add(target, data, key)
            elif path.is_relative_to(cache):
                assert path.name == "format_plugin_profile.hpp", "Unclassified generated source: " + key
                target = "source/generated/" + path.relative_to(cache).as_posix()
                pkg.add(target, data, key)
            else:
                assert not key.startswith("build/"), "Unclassified dependency; no AGPL fallback: " + key
                target = "source/" + key
                if key.startswith("third_party/"):
                    known(target, data)
                elif target not in pkg.files:
                    pkg.add(target, data, key, kind="asset" if path.suffix in {".md", ".json"} else "code")
            assert target in pkg.files and pkg.files[target] == data, "Missing/changed source: " + key
            staged_inputs[key] = target
            return pkg.members[target]

        for name, digest in sorted(sources.items()):
            source_member(name, digest)
        extra = set(EXTRA)
        for folder in ("plugins/anndata_zarr", "examples/anndata_zarr_host", "examples/anndata_zarr_plugin"):
            extra.update(p.relative_to(ROOT).as_posix() for p in (ROOT / folder).rglob("*") if p.is_file())
        for name in sorted(extra):
            path = project_path(name)
            source_member(relative(path), sources.get(relative(path), file_sha(path)))
        kit_path, kit = packet / "kit-sparse-inputs.json", load(packet / "kit-sparse-inputs.json")
        kit_rows = ([{"member": name, "sha256": digest} for name, digest in kit["files"].items()]
                    if isinstance(kit["files"], dict) else kit["files"])
        assert {p.relative_to(packet / "kit-sparse").as_posix() for p in (packet / "kit-sparse").rglob("*") if p.is_file()} == {r["member"] for r in kit_rows}
        for row in kit_rows:
            path = packet / "kit-sparse" / row["member"]
            if "bytes" in row:
                assert path.stat().st_size == row["bytes"]
            source_member(str(path), row["sha256"])
        for name in ("reader_plugin.h", "format_plugin.h"):
            assert (packet / "kit-sparse/include/qupathxx" / name).read_bytes() == (ROOT / "cpp/include/qupathxx" / name).read_bytes()

    intake_path = packet / "references/intake.json"
    intake = load(intake_path)
    assert intake["schema"] == "qpx.anndata-zarr-reference-intake/1" and intake["references"] == lock["references"]
    rows = {(r["component"], r["source_path"]): r for r in intake["references"]}
    assert len(rows) == len(intake["references"]) == len(REFERENCE_PINS) and set(rows) == set(REFERENCE_PINS)
    notices, reference_ids = {}, []
    attribution = {"anndata": "scverse, P. Angerer, F. Alexander Wolf and Theis Lab contributors",
        "zarr-specs": "Zarr Developers and contributors; unmodified specification; source URI and full CC-BY-4.0 text retained",
        "numcodecs": "Zarr Developers; original MIT notices retained"}
    for (component, name), row in rows.items():
        revision, license_id, digest = REFERENCE_PINS[(component, name)]
        assert (row["revision"], row["license"], row["sha256"]) == (revision, license_id, digest)
        path = project_path(row["local_path"])
        assert path == packet / "references" / component / name
        data = checked(path, digest)
        assert len(data) == row["bytes"]
        if name in {"LICENSE", "LICENSE.txt"}:
            target = "licenses/reference-" + component + "/" + name
            notices[component] = pkg.licensed(target, data, row["url"], license_id, kind="notice",
                attribution=attribution[component], evidence=row["url"])
    assert set(notices) == {"anndata", "zarr-specs", "numcodecs"}
    for (component, name), row in rows.items():
        data = checked(project_path(row["local_path"]), row["sha256"])
        notice_path = "licenses/reference-" + component + "/" + ("LICENSE.txt" if component == "numcodecs" else "LICENSE")
        reference_ids.append(pkg.licensed("source/references/" + component + "/" + name, data,
            row["url"], row["license"], [notices[component]],
            "notice" if name in {"LICENSE", "LICENSE.txt"} else "code" if name.endswith(".pyx") else "asset",
            attribution[component], notice_path))

    companion = {"schema": "qpx.anndata-zarr-companion-availability/1",
        "m05": {"package": str(base), "native_library_sha256": M05_NATIVE_SHA,
            "source_receipt_sha256": file_sha(base / "SOURCE.json"), "admission_sha256": file_sha(base_admission_path),
            "qualification_sha256": file_sha(base.parent / "results.json"), "package_zip": m05_zip,
            "full_matching_source": "Exact separately required M05 source tree/package/ZIP",
            "scope": "Retained application addon, not copied/rebuilt/requalified by M07"},
        "a08": base_companion["a08"], "frozen_sdk": dict(base_companion["frozen_sdk"]),
        "provider_private_static_libraries": {"zlib": {"path": str(zlib_library), "bytes": zlib_library.stat().st_size,
            "sha256": ZLIB_SHA, "license": "Zlib", "generated_config_sha256": ZLIB_CONFIG_SHA},
            "blosc": {"path": str(BLOSC_LIBRARY), "bytes": BLOSC_LIBRARY.stat().st_size,
                "sha256": BLOSC_SHA, "license": BLOSC_TERMS}},
        "corresponding_dependency_sources": [{"path": str(zlib_archive), "bytes": zlib_record["bytes"],
            "sha256": zlib_record["sha256"], "material_id": "release-src-zlib", "license_id": "Zlib"},
            {"path": str(BLOSC_ARCHIVE), "bytes": BLOSC_ARCHIVE.stat().st_size,
             "sha256": blosc_lock["archive_sha256"], "url": blosc_lock["archive"], "license_id": BLOSC_TERMS}],
        "retained_host_dependency_context": {"path": str(base / "COMPANIONS.json"), "sha256": file_sha(base / "COMPANIONS.json"),
            "scope": "Separate M05/A08/FP04 source and all actual host dependency terms remain required"},
        "openslide_license_context": base_companion["openslide_license_context"], "toolchain": compiled["profile"]}
    companion["frozen_sdk"]["abi_header"] = staged_inputs[relative(sdk_abi)]
    companion_id = pkg.add("COMPANIONS.json", encoded(companion),
        "Exact required M05/A08/FP04 and source/component availability",
        ["qupathxx-root-license", base_admission["members"]["native/qupathxx_loom_matrix_native.lib"]["material_id"],
         a08_admission["members"]["native/ngff_native.lib"]["material_id"], "release-src-zlib", blosc_notice], "asset")
    native_id = pkg.add("native/" + native.name, products[native.name], "Qualified native matrix-store and attachment addon",
        [companion_id, *(source_member(n, sources[n]) for n in groups[HOST_TARGET])], "derived_asset")
    module_id = pkg.add("bin/" + module.name, products[module.name], "Qualified optional AnnData-Zarr matrix ABI4 provider",
        [companion_id, *(source_member(n, sources[n]) for n in groups[MODULE_TARGET]),
         *(pkg.members[n] for n in pkg.files if n.startswith("source/format-plugin-kit/"))], "derived_asset")
    cli_id = pkg.add("bin/" + cli.name, products[cli.name], "Qualified shared native matrix/annotation CLI",
        [native_id, *(source_member(n, sources[n]) for n in groups[CLI_TARGET])], "derived_asset")
    consumer_id = pkg.add("bin/" + consumer.name, products[consumer.name], "Qualified public matrix-store SDK consumer",
        [native_id, *(source_member(n, sources[n]) for n in groups[CONSUMER_TARGET])], "derived_asset")
    pkg.add("bin/" + manifest.name, products[manifest.name], "Exact qualified AnnData-Zarr manifest", [module_id], "asset")
    template = (ROOT / "cmake/qpx_anndata_zarrConfig.cmake.in").read_bytes()
    placeholder = b"@QPX_ANNDATA_ZARR_NATIVE_SHA256@"
    assert template.count(placeholder) == 1
    config = template.replace(placeholder, native_sha.encode("ascii"))
    assert str(ROOT).encode().lower() not in config.lower()
    pkg.add("lib/cmake/qpx_anndata_zarr/qpx_anndata_zarrConfig.cmake", config,
        "Relocatable source-matched SDK config; exact native SHA substitution", [native_id])
    receipt_files = [("compiled-inputs.json", compiled_path, [native_id, module_id]),
        ("integration-completion.json", validation_path, [cli_id, consumer_id]),
        ("native-reference.json", reference_path, [native_id]),
        ("reference-intake.json", intake_path, reference_ids), ("kit-inputs.json", kit_path, [module_id]),
        ("producer-reference.json", producer_path, ["qupathxx-root-license", *reference_ids])]
    receipt_files.extend(("integration-reused-" + str(i + 1) + ".json", path, [native_id, module_id])
                         for i, path in enumerate(retained_receipts))
    for name, path, parents in receipt_files:
        pkg.add("source/receipts/" + name, path.read_bytes(), "Exact source/native/reference evidence " + str(path), parents, "asset")
    notice = """M07 provider, private Zarr/reader/JSON helpers, native application units, examples, tests and development controls: AGPL-3.0-only, qupathxx contributors. Matching new source and the full application license are retained.
Provider decoder: unchanged private prefixed zlib1.3.2, Zlib, Jean-loup Gailly, Mark Adler and contributors. Original headers/configuration recipes and identified generated zconf.h retain their own terms under source/third_party/anndata-zarr/zlib and licenses/zlib. Exact static library/source archive remain separate companions. The provider links no HDF5/application core/Python/Cython/Java or external codec fallback.
Provider decoder: reused static C-Blosc 1.21.6 with bundled LZ4 1.9.4. Original compiled headers, source lock, combined component notices and exact corresponding archive are identified under source/third_party/anndata-zarr/blosc, licenses/blosc and COMPANIONS.json.
Native host interfaces retain actual nlohmann-json3.12.0 MIT and SQLite3.54.1 author blessing/public-domain terms. Separate retained M05/A08/FP04 host products retain all other component notices and corresponding sources, including HDF5 where present.
Retained Loom regression-oracle HDF5 original/generated headers keep their exact admitted M05 material IDs under source/third_party/retained-loom/hdf5. Their full BSD/component notices and separate matching source/configuration/library companions remain required; they do not introduce HDF5 linkage into the AnnData-Zarr provider.
Six unmodified nonexecuted references remain distinct: AnnData0.12.7 schema/source/notice BSD-3-Clause; Zarr Storage Specification2 CC-BY-4.0, Zarr Developers, exact source URI/revision and full attribution/license retained; Numcodecs0.15.1 VLenUTF8 definition/notice MIT. They are context, not product Python/Cython implementations, runtime dependencies or scientific fixtures. See source/plugins/anndata_zarr/THIRD_PARTY_NOTICES.txt and source-lock.json.
Exact GEOS runtime DLLs retain LGPL-2.1-or-later; the actual OpenSlide DLL binary is LGPL-2.1-only. Its older aggregate source-archive label does not relicense components; all original component notices remain. sdk_abi.hpp is external in the frozen SDK, with eligible exact template/recipe derivation evidence here.
Preserve corresponding new and retained application/dependency sources, actual notices, attribution and applicable AGPL/LGPL source/replacement/relinking obligations. External MSVC/compiler/CRT prerequisites follow the recorded Release /MD profile; no Microsoft redistributable/source bundle is supplied.
No restricted10x implementation/license artifact, upstream dataset/image/model/fixture or restricted derivative is included. Container/code licensing never licenses unrelated contents.
"""
    pkg.add("NOTICE.txt", notice.encode("utf8"), "Actual component/reference/source obligations", kind="notice")
    guide = """# Optional native AnnData-Zarr matrix packet

Read source/plugins/anndata_zarr/README.md and source/docs/benchmarks/anndata-zarr/README.md for the selected local Zarr2, array/dataframe/string/sparse encodings, explicit matrix/ID/domain/frame choices and refusals. Matrix ABI4 binds all source members through separate native observation and content identities; schema6 preserves those facts and distinguishes dense from stored-entry imports. ABI1/2 schema4/5 bytes retain their own exact compatibility evidence.

Keep bin/qpx_anndata_zarr_plugin.dll with bin/anndata-zarr-format-plugins.json and the exact runtime DLLs. Use an absolute manifest:
  bin/qupathxx_format_import.exe --format-plugins <absolute-manifest> catalog
Use matrix-store-index and explicit primary .zattrs/options/native coordinate TSV, as shown in the workflow guide and source public consumer. Native QMX/project publication remains shared; no inferred spatial registration or Python/Java runtime is required. Dense pages retain their logical-cell limit. CSR/CSC pages and QMX contain only stored entries under the declared work and axis limits.

COMPANIONS.json identifies the separately required matching M05 native/source/package/ZIP, A08 addon/source and frozen FP04 SDK/core/full source/dependency products. Those products are not copied, rebuilt or requalified. External sdk_abi.hpp retains its eligible template/recipe/DERIVATION evidence; no old invalid material ancestry is bypassed by copying that header.

Installed SDK consumer: configure source/examples/anndata_zarr_plugin with CMAKE_PREFIX_PATH="<M07 packet>;<M05 packet>;<A08 packet>;<frozen FP04 SDK>" using Windows x64 MSVC1944/compiler19.44.35225.0/toolset14.44.35207 Release /MD. qpx_anndata_zarr checks its native hash and imports the retained M05/A08/core chain.

Native source rebuild uses source/examples/anndata_zarr_host, the retained A08/FP04 prefixes and QUPATHXX_ANNDATA_ZARR_MATRIX_LIBRARY=<M05 packet>/native/qupathxx_loom_matrix_native.lib. JSON/SQLite interfaces default to supplied source/third_party/native-overlay paths. Provider rebuild uses source/plugins/anndata_zarr, frozen SDK and QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=<M07 packet>/source/format-plugin-kit. Set QUPATHXX_ANNDATA_ZARR_ZLIB_INCLUDE and QUPATHXX_ANNDATA_ZARR_BLOSC_INCLUDE to their recorded include directories under source/third_party/anndata-zarr, and QUPATHXX_ANNDATA_ZARR_BLOSC_LIBRARY to the exact static companion. Rebuilds receive separate identities.

SOURCE.json binds compiled inputs/products/qualification. PACKAGE.json owns exact staged paths. This writer creates no ZIP and claims no installed use/removal acceptance; separate checks follow. Close module users before removing only verified owned paths. Preserve user stores, position tables, QMX/projects/results/notes and all separate companions. No publication or broader catalogue completion is claimed.
"""
    pkg.add("README.md", guide.encode("utf8"), "Native install/source/removal and support scope", kind="asset")
    module_inputs = {n: sources[n] for n in groups[MODULE_TARGET]}
    metadata = {"schema": "qpx.anndata-zarr-delivery/1", "profile": PROFILE,
        "providers": {PROVIDER: {"module_sha256": sha(products[module.name]), "version": VERSION, "contract": CONTRACT,
            "abi": MATRIX_ABI, "domain": "matrix", "source_context_sha256": context(module_inputs), "source_inputs": module_inputs,
            "licenses": ["AGPL-3.0-only", "Zlib", BLOSC_TERMS], "reference_licenses": ["BSD-3-Clause", "CC-BY-4.0", "MIT"],
            "dependencies": [], "validation_sha256": file_sha(validation_path)}},
        "native_library_sha256": native_sha, "cli_sha256": sha(products[cli.name]),
        "sdk_consumer_sha256": sha(products[consumer.name]), "compiled_inputs_sha256": file_sha(compiled_path),
        "compiled_context_sha256": context({"sources": sources, "groups": groups, "profile": compiled["profile"], "commands": compiled["commands"]}),
        "host_inputs": {n: sources[n] for n in groups[HOST_TARGET]}, "compiled_input_stage_paths": staged_inputs,
        "companions_sha256": sha(encoded(companion)), "retained_matrix_addon_sha256": M05_NATIVE_SHA,
        "retained_application_addon_sha256": A08_NATIVE_SHA, "base_core_sha256": BASE_CORE,
        "component_terms": {"new_code": "AGPL-3.0-only", "provider_zlib": "Zlib", "provider_blosc": BLOSC_TERMS, "json": "MIT", "sqlite": "blessing",
            "references": {"anndata": "BSD-3-Clause", "zarr-specs": "CC-BY-4.0", "numcodecs": "MIT"},
            "openslide_binary": "LGPL-2.1-only"},
        "qualified_checks": sorted(checks), "native_reference_sha256": file_sha(reference_path),
        "producer_reference_sha256": file_sha(producer_path),
        "producer_evidence_scope": "Exact metadata-only authored/index/license report and first-party generator; no producer environment/packages copied",
        "representative_sparse_workload": validation["sparse_workload"],
        "prior_inventory_groups": len(before), "prior_groups_unchanged": True,
        "scope": "Selected native AnnData-Zarr2 typed store import/QMX/project/CLI/SDK with exact separate companions; no general-format/UI/biological/platform/full-goal claim",
        "install_or_removal_qualification": "Separate delivery check follows staging", "archive_created": False}
    assert all(load(inventory_path).get(name) == value for name, value in before.items())
    delivery_checks_sha = file_sha(ROOT / "tools/ci/check_anndata_zarr_delivery.py")
    result = pkg.finish(output, "anndata-zarr-" + sha(products[module.name])[:16] + "-" + delivery_checks_sha[:8],
                        metadata, inventory_path, args.write_inventory)
    after = load(inventory_path)
    assert all(after.get(name) == value for name, value in before.items())
    assert len(after) == len(before) + 1
    print(json.dumps({"files": result["files"], "admission_exit": result["admission_exit"], "output": str(output)}))


if __name__ == "__main__":
    main()
