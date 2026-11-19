"""Stage one exact, qualified optional native NGFF-label packet.

Development packaging only: no scientific interpretation, builds, downloads,
deployment or ZIP creation. The frozen FP04 SDK/core and its complete source
products remain separate; copied dependency DLLs retain their existing terms.
"""
import argparse
from functools import lru_cache
import hashlib
import json
from pathlib import Path
import re
import zipfile

from check_material_staging import admitted, inventory as read_inventory
from stage_annotation_sources import Package, ROOT


SDK = ROOT / "build/format-family-plugins/sdk"
BASE_CORE = "4bf957c9ea6e46294c11b9bfb7d9c2496cf4fca607a9c28ba837768d301eb6bb"
BASE_SOURCE = ROOT / "build/format-family-plugins/native-source-fb5289df36ac.zip"
BASE_SOURCE_SHA = "d28660b71dc99367e49d2f123a9cf12455533ee4345bdc74db15b5c68c7df7a9"
BASE_CONTEXT = "fb5289df36ac708fa1ee45b581ad1c2d66daa404c3dc6eed5460480160c78f1c"
SDK_ABI_SHA = "45834ee3da1e22c1e2edc6a194f11025cf3bb4a6f435263233503fe381fe48cf"
SDK_ABI_TEMPLATE_SHA = "19e29c510f9b0def2344b862d992d0b1b69722c9012b503b38e277c71baeac70"
SDK_ABI_RECIPE_SHA = "bc139538a2de5184167c6286450c6328d7af4c102c48f54d5fc91c872d1c383f"
NATIVE_SHA = "b0c86a57518c49654359a784f3344a1f15c55e60bf6fa07523ad9a6e7fd94a84"
W3C_SHA = "83726d459db392d19c5145134ad14236056713019f3f2276e3683519ba4bd737"
PROVIDER = "org.qupathxx.ngff-labels"
CONTRACT = "ngff04-zarr2-integral-labels/1"
PROFILE = "windows-x64-msvc1944-release-md"
RUNTIMES = {
    "geos.dll": "9bff21e1082b57e9914909a156ad121d508d2203a8c6ab4d8d41eb74cc19fa0e",
    "geos_c.dll": "64c8a686deccd9e3d5ae66f96ba1276c23faf6ab00564feee421ae91f552b7d6",
    "libopenslide-1.dll": "dc8e35ad0ce001c4e878f2bebc23e87fb85d2bdebec5a542af2ecec3cb5cd635",
}
OBJECTS = {"label_raster", "superpixels", "subcellular", "tissue_contours", "fluorescence_cells"}
HOST_UNITS = {
    "cpp/directory_inventory.cpp", "cpp/directory_store.cpp", "cpp/ome_zarr_store.cpp",
    "cpp/label_codes.cpp", "cpp/label_mask.cpp", "cpp/label_workflow.cpp", "cpp/label_cli.cpp",
    "cpp/reader_plugin_catalog.cpp", "cpp/reader_plugin_loader.cpp", "cpp/reader_plugin_bridge.cpp",
    "cpp/reader_plugin_link.cpp", "cpp/reader_plugin_runtime.cpp", "cpp/project.cpp",
    "cpp/project_source.cpp", "cpp/label_mask_frame.cpp", "cpp/ngff_label_mask.cpp",
    "cpp/label_mask_archive.cpp",
}
EXTRA = {
    "cmake/qpx_ngff_labelsConfig.cmake.in", "docs/label-analysis.md",
    "docs/benchmarks/ngff-labels/README.md", "docs/reader-license-policy.md",
    "docs/licensing/restricted-materials.md", "tools/ci/check_ngff_labels.py",
    "tools/ci/check_ngff_foundations.py", "tools/ci/stage_ngff_labels.py",
    "tools/ci/stage_annotation_sources.py", "tools/ci/stage_annotation_geometry.py",
    "tools/ci/check_material_staging.py", "plugins/format-common/reader_input.hpp",
    "plugins/format-common/json_tree.hpp", "plugins/format-common/binary_input.hpp",
    "plugins/format-common/annotation_json.hpp", "plugins/format-common/annotation_source.hpp",
    "cpp/include/qupathxx/format_plugin.h", "cpp/tests/json_tree_tests.cpp",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf8")


def context(value):
    return sha(json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf8"))


@lru_cache(maxsize=None)
def project_path(value):
    """Resolve real Windows case; never treat an external SDK header as ours."""
    path = Path(str(value).replace("\\", "/"))
    if not path.is_absolute():
        path = ROOT / path
    resolved = path.resolve(strict=True)
    assert resolved.is_relative_to(ROOT.resolve()), "External prerequisite is not first-party source: " + str(path)
    current = ROOT.resolve()
    for part in resolved.relative_to(current).parts:
        children = [p for p in current.iterdir() if p.name.casefold() == part.casefold()]
        exact = [p for p in children if p.name == part]
        assert len(exact or children) == 1, "Ambiguous source path case: " + str(path)
        current = (exact or children)[0]
    assert current.is_file(), "Missing exact source file: " + str(current)
    return current


def relative(path):
    return path.relative_to(ROOT.resolve()).as_posix()


def checked(path, expected):
    data = path.read_bytes()
    assert re.fullmatch("[0-9a-f]{64}", expected) and sha(data) == expected, "Changed qualified bytes: " + str(path)
    return data


class NgffPackage(Package):
    material_prefix = "ngff-labels"

    def licensed(self, name, data, origin, license_id, parents=(), kind="code",
                 attribution="qupathxx contributors", evidence="LICENSE; repository author declaration"):
        parents = list(dict.fromkeys(parents))
        suffix = "-" + context(parents)[:12] if parents and parents != ["qupathxx-root-license"] else ""
        ident = self.material_prefix + "-" + license_id + "-" + kind + "-" + sha(data)[:24] + suffix
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
                  "material": {"id": ident, "category": "admitted_open", "kind": kind,
                               "license_id": license_id, "license_evidence": evidence,
                               "attribution": attribution, "source_ids": parents}}
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
    for name in ("packet", "cache", "output", "validation", "sdk-check", "base-package", "base-admission"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    packet, cache, output = args.packet.resolve(), args.cache.resolve(), args.output.resolve()
    assert not output.exists(), "Use a fresh stage and preserve prior evidence"
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = load(inventory_path)
    entries = read_inventory([inventory_path])
    pkg = NgffPackage(inventory,
        "Source-matched optional native NGFF0.4/Zarr2 integral-label provider and application SDK addon; "
        "exact scientific objects reused, frozen FP04 SDK/core/full source remain separate required products; "
        "no GUI, Python/Java, biological/general NGFF/platform/full-catalogue qualification or publication",
        "ngff_label_plugin")
    by_hash = {}
    for ident, record in entries.items():
        by_hash.setdefault(record["sha256"], []).append(ident)

    def existing_id(data, preferred=None):
        digest = sha(data)
        ids = [i for i in by_hash.get(digest, []) if entries[i].get("bytes") == len(data)]
        if preferred:
            assert preferred in ids, "Required exact material ID is absent: " + preferred
            ids = [preferred]
        assert ids, "Uninventoried dependency material (not reclassified): " + digest
        ids.sort(key=lambda i: (not i.startswith("native-output-"), i))
        admitted(ids[0], entries)
        return ids[0]

    def known(name, data, preferred=None):
        ident = existing_id(data, preferred)
        if name in pkg.files:
            assert pkg.files[name] == data, "Conflicting staged material: " + name
            return pkg.members[name]
        else:
            pkg._member(name, data, ident)
        return ident

    compiled_path = packet / "compiled-inputs.json"
    compiled = load(compiled_path)
    assert compiled["schema"] == "qpx.ngff-native-inputs/2" and compiled["base_core_sha256"] == BASE_CORE
    assert compiled["profile"] == {"msvc": 1944, "compiler": "19.44.35225.0", "toolset": "14.44.35207",
                                    "configuration": "Release", "runtime": "MD", "cpp": 17}
    sources = {}
    for name, digest in compiled["sources"].items():
        path = project_path(name)
        key = relative(path)
        assert key not in sources or sources[key] == digest, "Conflicting source-case aliases: " + key
        checked(path, digest)
        sources[key] = digest
    groups = {name: [relative(project_path(p)) for p in paths] for name, paths in compiled["groups"].items()}
    assert all(p in sources for paths in groups.values() for p in paths), "Group input missing from exact source map"
    assert {p for p in groups["ngff_native"] if p.endswith((".cpp", ".c"))} == HOST_UNITS
    assert "ngff_json_tree_tests" in groups, "Candidate2 generic-parser evidence was not captured"
    required = {"plugins/format-common/json_tree.hpp", "plugins/format-common/reader_input.hpp",
                "plugins/ngff_labels/ngff_labels.cpp", "cpp/tests/ngff_label_workflow_tests.cpp",
                "cpp/tests/json_tree_tests.cpp", "examples/ngff_label_plugin/main.cpp"}
    assert required <= set(sources), "Final compiled capture is missing candidate2/SDK inputs: " + str(required - set(sources))
    assert {"plugins/format-common/json_tree.hpp", "plugins/format-common/reader_input.hpp"} <= set(groups["qpx_ngff_labels_plugin"])

    def product_digest(path):
        """Products must be bound by the final collector, not guessed by filename."""
        matches = []
        for name, row in compiled.get("products", {}).items():
            value = row.get("sha256") if isinstance(row, dict) else row
            if name == path.name:
                matches.append(value)
                continue
            p = Path(str(name).replace("\\", "/"))
            if not p.is_absolute():
                p = ROOT / p
            if p.resolve() == path.resolve():
                matches.append(value)
        assert matches and len(set(matches)) == 1, "Final collector lacks an exact product: " + str(path)
        return matches[0]

    binaries = cache / "Release"
    native = binaries / "ngff_native.lib"
    cli = binaries / "qupathxx_labels.exe"
    consumer = binaries / "ngff_label_consumer.exe"
    module = binaries / "qpx_ngff_labels_plugin.dll"
    manifest = binaries / "ngff-label-reader-plugins.json"
    products = {p.name: checked(p, product_digest(p)) for p in (native, cli, consumer, module, manifest)}
    assert sha(products[native.name]) == NATIVE_SHA
    validation, sdk_check = load(args.validation), load(args.sdk_check)
    assert validation["passed"] and validation["native_only_environment"]
    assert all(validation["checks"][name]["returncode"] == 0
               for name in ("generic-parser", "actual-workflow", "decoder-off"))
    assert validation["native_library_sha256"] == NATIVE_SHA
    validated = {str(Path(k).resolve()).casefold(): v for k, v in validation["binaries"].items()}
    for path in (cli, module):
        assert validated[str(path.resolve()).casefold()] == sha(products[path.name]), "Validation product differs: " + str(path)
    assert validation["provider_manifest"]["sha256"] == sha(products[manifest.name])
    assert Path(validation["provider_manifest"]["path"]).resolve() == manifest.resolve()
    assert sdk_check.get("passed", sdk_check.get("returncode") == 0)
    if "returncode" in sdk_check:
        assert sdk_check["returncode"] == 0
    assert sdk_check["native_only_environment"] and sdk_check["exact_native_csv_bundle"]
    assert sdk_check["raw_property_tokens_exact"]
    assert sdk_check["binary_sha256"] == sha(products[consumer.name])
    assert Path(sdk_check["argv"][0]).resolve() == consumer.resolve()
    sdk_manifest = sdk_check.get("manifest_sha256", sdk_check.get("provider_manifest", {}).get("sha256"))
    assert sdk_manifest == sha(products[manifest.name]), "SDK receipt is not bound to the candidate2 manifest"
    assert sdk_check["native_library_sha256"] == NATIVE_SHA
    definition = json.loads(products[manifest.name])
    assert definition["schema"] == "qpx.reader-plugins/1" and len(definition["plugins"]) == 1
    entry = definition["plugins"][0]
    assert (entry["provider_id"], entry["version"], entry["contract"], entry["abi"], entry["profile"]) == (PROVIDER, "1.0.0", CONTRACT, 1, PROFILE)
    assert entry["capabilities"] == {"display_rgba": True, "raw_samples": True, "source_store": True, "label_description": True}
    assert entry["module"] == {"path": module.name, "sha256": sha(products[module.name])}
    assert entry["dependencies"] == [{"path": "libopenslide-1.dll", "sha256": RUNTIMES["libopenslide-1.dll"]}]

    base_admission, base = load(args.base_admission), load(args.base_package / "SOURCE.json")
    assert base_admission["admission_exit"] == 0
    assert base["base_sdk"]["core_sha256"] == BASE_CORE
    assert base["base_sdk"]["source_archive_sha256"] == BASE_SOURCE_SHA
    assert file_sha(SDK / "lib/qupathxx/qupathxx_core.lib") == BASE_CORE
    assert file_sha(BASE_SOURCE) == BASE_SOURCE_SHA
    with zipfile.ZipFile(BASE_SOURCE) as archive:
        assert sha(archive.read("COMPILED-INPUTS.sha256")) == BASE_CONTEXT
        assert len(archive.infolist()) == 903
    snapshot_path = ROOT / "build/format-family-plugins/source-snapshot.json"
    base_results_path = ROOT / "build/format-family-plugins/results.json"
    snapshot, base_results = load(snapshot_path), load(base_results_path)
    assert snapshot["source_context_sha256"] == BASE_CONTEXT and snapshot["archive_sha256"] == BASE_SOURCE_SHA
    assert snapshot["compiled_manifest_exact"] and snapshot["members"] == 903
    assert base_results["native_sdk_core_sha256"] == BASE_CORE
    for name in base_admission["members"]:
        if name.startswith("source/third_party/native-overlay/") or name.startswith(
                ("licenses/nlohmann-json/", "licenses/sqlite/", "licenses/geos/")):
            pkg.borrow(name, args.base_package, base_admission)

    # Preserve every exact original SDK notice, including JSON/SQLite notices
    # omitted from COCO and all19 OpenSlide bundled-component notice files.
    sdk_notices = {}
    notice_root = SDK / "share/qupathxx/licenses"
    for path in sorted(p for p in notice_root.rglob("*") if p.is_file()):
        name = "licenses/" + path.relative_to(notice_root).as_posix()
        sdk_notices[name] = known(name, path.read_bytes())
    assert sum(n.startswith("licenses/openslide-sdk/") for n in sdk_notices) == 19
    assert "licenses/nlohmann-json/LICENSE.MIT" in sdk_notices and "licenses/sqlite/sqlite-LICENSE.txt" in sdk_notices
    root_notice = checked(ROOT / "LICENSE", "e0eedba615d5cd1b986afb6c5b3a4b1ae33713e7e9dc74d19daec5e3221f9d2e")
    known("source/LICENSE", root_notice, "format-family-AGPL-3-0-only-e0eedba615d5cd1b986afb6c")
    runtime_ids = {}
    for name, digest in RUNTIMES.items():
        data = checked(SDK / "bin" / name, digest)
        checked(binaries / name, digest)
        runtime_ids[name] = known("bin/" + name, data, "native-output-" + digest)

    staged_inputs = {}
    kit_root = packet / "reader-kit"
    sdk_scaffold = packet / "sdk-addon/source/cpp/include"
    json_root = ROOT / "build/ci/gridfrontends167-package/_deps/json-src/include"
    sqlite_root = ROOT / "build/deps/sqlite-amalgamation-3540100"
    geos_root = ROOT / "build/deps/geos-sdk/include"

    def external_sdk_abi(path, data, digest):
        # The generated header belongs to the separate frozen SDK. Bind its
        # actual authored sources and deterministic recipe, without copying it
        # or substituting the unrelated historical full-SDK material graph.
        assert path == SDK / "include/qupathxx/sdk_abi.hpp"
        assert digest == SDK_ABI_SHA and len(data) == 500
        template = checked(ROOT / "cmake/sdk_abi.hpp.in", SDK_ABI_TEMPLATE_SHA)
        recipe = checked(ROOT / "cmake/NativeSdk.cmake", SDK_ABI_RECIPE_SHA)
        with zipfile.ZipFile(BASE_SOURCE) as archive:
            assert archive.read("cmake/sdk_abi.hpp.in") == template
            assert archive.read("cmake/NativeSdk.cmake") == recipe
        assert template.count(b"@MSVC_VERSION@") == 1
        assert template.replace(b"@MSVC_VERSION@", b"1944") == data
        configure = b"configure_file(cmake/sdk_abi.hpp.in generated/sdk/sdk_abi.hpp @ONLY)"
        assert configure in recipe
        template_name = "source/frozen-sdk-interface/cmake/sdk_abi.hpp.in"
        recipe_name = "source/frozen-sdk-interface/cmake/NativeSdk.cmake"
        template_id = known(template_name, template,
            "molecular-desktop-code-19e29c510f9b0def2344b862")
        recipe_id = known(recipe_name, recipe,
            "molecular-desktop-code-bc139538a2de5184167c6286")
        receipt_name = "source/frozen-sdk-interface/DERIVATION.json"
        derivation = {
            "schema": "qpx.frozen-sdk-abi-derivation/1",
            "header": {"external": True, "required_separate_product": "frozen-fp04-sdk",
                       "path": str(path), "sdk_relative_path": "include/qupathxx/sdk_abi.hpp",
                       "bytes": len(data), "sha256": digest},
            "template": {"stage_path": template_name, "bytes": len(template),
                         "sha256": SDK_ABI_TEMPLATE_SHA, "material_id": template_id},
            "recipe": {"stage_path": recipe_name, "bytes": len(recipe),
                       "sha256": SDK_ABI_RECIPE_SHA, "material_id": recipe_id,
                       "configure_call": configure.decode("ascii")},
            "substitution": {"literal": "@MSVC_VERSION@", "replacement": "1944",
                             "occurrences": 1, "byte_exact": True},
            "corresponding_source": {"archive": str(BASE_SOURCE), "sha256": BASE_SOURCE_SHA,
                                     "compiled_source_sha256": BASE_CONTEXT,
                                     "template_member": "cmake/sdk_abi.hpp.in",
                                     "recipe_member": "cmake/NativeSdk.cmake",
                                     "members_byte_exact": True},
        }
        receipt_id = pkg.add(receipt_name, encoded(derivation),
            "Verified deterministic generated ABI-header derivation; header remains in the required separate frozen SDK",
            ["qupathxx-root-license", template_id, recipe_id], "asset")
        staged_inputs[relative(path)] = {
            "external": True, "required_separate_product": "frozen-fp04-sdk",
            "path": str(path), "sdk_relative_path": "include/qupathxx/sdk_abi.hpp",
            "bytes": len(data), "sha256": digest,
            "source_evidence_stage_path": receipt_name,
            "source_evidence_material_id": receipt_id,
        }
        return receipt_id

    def source_member(name, digest):
        path = project_path(name)
        key = relative(path)
        if key in staged_inputs:
            member = staged_inputs[key]
            if isinstance(member, dict):
                assert member["external"] and member["sha256"] == digest, "External input identity differs: " + key
                return member["source_evidence_material_id"]
            assert sha(pkg.files[member]) == digest, "Input identity differs: " + key
            return pkg.members[member]
        data = checked(path, digest)
        if path.is_relative_to(json_root):
            target = "source/third_party/native-overlay/json/include/" + path.relative_to(json_root).as_posix()
        elif path.is_relative_to(sqlite_root):
            assert path.name == "sqlite3.h", "Only the admitted SQLite public interface belongs in this overlay"
            target = "source/third_party/native-overlay/sqlite/sqlite3.h"
        elif path.is_relative_to(geos_root):
            target = "source/third_party/native-overlay/geos/" + path.relative_to(geos_root).as_posix()
        elif path.is_relative_to(SDK / "include"):
            if path == SDK / "include/qupathxx/sdk_abi.hpp":
                return external_sdk_abi(path, data, digest)
            target = "source/frozen-sdk-interface/include/" + path.relative_to(SDK / "include").as_posix()
            known(target, data)
        elif path.is_relative_to(sdk_scaffold):
            counterpart = project_path("cpp/include/" + path.relative_to(sdk_scaffold).as_posix())
            assert checked(counterpart, digest) == data, "SDK scaffold differs from active public header: " + key
            target = "source/" + relative(counterpart)
            if target not in pkg.files:
                pkg.add(target, data, relative(counterpart))
        elif path.is_relative_to(kit_root):
            target = "source/reader-plugin-kit/" + path.relative_to(kit_root).as_posix()
            known(target, data) if path.name == "LICENSE" else pkg.add(target, data, key)
        elif path.is_relative_to(cache):
            assert path.name == "reader_plugin_profile.hpp", "Unclassified generated/build input: " + key
            target = "source/generated/" + path.relative_to(cache).as_posix()
            pkg.add(target, data, key)
        else:
            assert not key.startswith("build/"), "Unclassified build dependency (not reclassified): " + key
            target = "source/" + key
            if target not in pkg.files:
                pkg.add(target, data, key, kind="asset" if path.suffix in (".md", ".json") else "code")
        assert target in pkg.files and pkg.files[target] == data, "Missing/changed borrowed interface: " + target
        staged_inputs[key] = target
        return pkg.members[target]

    for name, digest in sorted(sources.items()):
        source_member(name, digest)
    extra = set(EXTRA)
    for folder in ("plugins/ngff_labels", "plugins/image-common", "examples/ngff_labels_host", "examples/ngff_label_plugin"):
        extra.update(p.relative_to(ROOT).as_posix() for p in (ROOT / folder).rglob("*") if p.is_file())
    for name in sorted(extra):
        path = project_path(name)
        source_member(relative(path), sources.get(relative(path), file_sha(path)))
    kit_inputs = load(packet / "reader-kit-inputs.json")
    assert {p.relative_to(kit_root).as_posix() for p in kit_root.rglob("*") if p.is_file()} == {
        r["member"].replace("\\", "/") for r in kit_inputs["files"]}
    for row in kit_inputs["files"]:
        path = kit_root / row["member"].replace("\\", "/")
        source_member(relative(project_path(str(path))), row["sha256"])
    assert (kit_root / "include/qupathxx/reader_plugin.h").read_bytes() == (ROOT / "cpp/include/qupathxx/reader_plugin.h").read_bytes()

    # Retain reports under their own W3C terms; no sample software/data enters.
    reference = load(packet / "reference-intake.json")
    assert reference["commit"] == "106c3010dcb4079eb0a69868a11cc5943f942fdf" and len(reference["members"]) == 5
    w3c_intake = load(packet / "references/licenses/intake.json")
    assert w3c_intake["license_id"] == "W3C-20150513" and w3c_intake["sha256"] == W3C_SHA
    w3c_id = pkg.licensed("licenses/ngff/W3C-20150513.txt",
        checked(packet / "references/licenses/W3C-20150513.txt", W3C_SHA),
        w3c_intake["license_source"], "W3C-20150513", kind="notice",
        attribution="W3C; original notice retained", evidence=w3c_intake["license_source"])
    reference_ids = []
    for name, row in reference["members"].items():
        data = checked(packet / "references/ngff" / name, row["sha256"])
        assert len(data) == row["bytes"]
        reference_ids.append(pkg.licensed("source/references/ngff/" + name, data, row["url"],
            "W3C-20150513", [w3c_id], "notice" if name == "LICENSE.md" else "asset",
            "OME-NGFF contributors; original copyright/notices retained", w3c_intake["license_source"]))

    reuse_path = packet / "native-object-reuse.json"
    reuse = load(reuse_path)
    assert {r["name"] for r in reuse["records"]} == OBJECTS and len(reuse["records"]) == 5
    object_ids = []
    for row in reuse["records"]:
        parents = [source_member(name, digest) for name, digest in row["inputs"].items()]
        assert row["source_receipt"] and re.fullmatch("[0-9a-f]{64}", row["command_sha256"])
        path = packet / "inputs/native" / (row["name"] + ".obj")
        object_ids.append(pkg.add("native/label-objects/" + path.name, checked(path, row["object_sha256"]),
            "Exact retained scientific object; " + row["source_receipt"], parents, "derived_asset"))

    # Availability metadata is our asset. It never assigns AGPL to the separate
    # frozen core/source ZIP or to any upstream archive/container contents.
    archive_ids, visited = set(), set()

    def source_parents(ident):
        if ident in visited:
            return
        visited.add(ident)
        record = entries.get(ident, pkg.records.get(ident))
        assert record is not None, "Missing exact source-evidence parent: " + ident
        if record["material"]["kind"] == "source_archive":
            archive_ids.add(ident)
        for parent in record["material"]["source_ids"]:
            source_parents(parent)

    for ident in [*sdk_notices.values(), *runtime_ids.values(),
                  *(pkg.members[n] for n in pkg.files if n.startswith("source/frozen-sdk-interface/"))]:
        source_parents(ident)
    archive_ids.update({"release-src-geos", "release-src-nlohmann-json", "release-src-sqlite",
                        "release-src-openslide-sdk-and-dependencies"})
    archives = []
    for ident in sorted(archive_ids):
        record = entries[ident]
        admitted(ident, entries)
        filename = Path(record["filename"])
        candidates = [ROOT / filename, ROOT / "build/deps" / filename.name,
                      ROOT / "build/dependency-downloads" / filename.name,
                      ROOT / "build/gui-aggregation/molecular-delivery/dependency-sources/sources" / filename.name,
                      ROOT / "build/reader-plugin-project/dependency-sources/sources" / filename.name]
        matches = [p for p in candidates if p.is_file() and p.stat().st_size == record["bytes"] and file_sha(p) == record["sha256"]]
        assert matches, "Required retained dependency source unavailable/changed (no download): " + ident
        archives.append({"material_id": ident, "component": record.get("component", ident),
                         "version": record.get("version"), "local_path": str(matches[0].resolve()),
                         "bytes": record["bytes"], "sha256": record["sha256"],
                         "upstream": record.get("url") or record.get("source"),
                         "inventory_license_id": record["material"]["license_id"],
                         "component_notice_ids": record["material"]["source_ids"]})
    availability = {
        "schema": "qpx.ngff-frozen-source-availability/1",
        "scope": "Required separately retained frozen SDK/core/full source and upstream sources; availability only, no new base-product qualification",
        "core": {"path": str((SDK / "lib/qupathxx/qupathxx_core.lib").resolve()),
                 "bytes": (SDK / "lib/qupathxx/qupathxx_core.lib").stat().st_size, "sha256": BASE_CORE},
        "first_party_source": {"path": str(BASE_SOURCE.resolve()), "bytes": BASE_SOURCE.stat().st_size,
                               "sha256": BASE_SOURCE_SHA, "compiled_source_sha256": BASE_CONTEXT,
                               "members": 903, "scope": "First-party source and retained notices; external dependency source products remain separate"},
        "base_receipts": {"source_snapshot": {"path": str(snapshot_path), "bytes": snapshot_path.stat().st_size, "sha256": file_sha(snapshot_path)},
                          "results": {"path": str(base_results_path), "bytes": base_results_path.stat().st_size, "sha256": file_sha(base_results_path)},
                          "borrowed_package_source_sha256": file_sha(args.base_package / "SOURCE.json")},
        "upstream_sources": archives, "copied_runtime_materials": runtime_ids,
        "copied_original_notices": sdk_notices,
        "required_external_abi_header": staged_inputs[relative(SDK / "include/qupathxx/sdk_abi.hpp")],
        "openslide_license_context": "Binary record LGPL-2.1-only; source-archive inventory LGPL-2.1-or-later; archive build declares LGPL-2.1-only and bundled components retain their individual19 notices. No blanket relicensing.",
        "toolchain": "External Windows/MSVC1944 compiler19.44.35225.0 toolset14.44.35207 Release /MD; Microsoft source/runtime is not staged",
    }
    availability_id = pkg.add("FROZEN-SOURCE-AVAILABILITY.json", encoded(availability),
        "Verified exact retained product/source availability; component terms remain distinct",
        ["qupathxx-root-license", *sorted(archive_ids)], "asset")
    native_parents = [source_member(n, sources[n]) for n in groups["ngff_native"]] + object_ids + [availability_id]
    native_id = pkg.add("native/ngff_native.lib", products[native.name],
        "Qualified source-scoped17-service NGFF/native label SDK addon over separate frozen FP04 core", native_parents, "derived_asset")
    cli_id = pkg.add("bin/" + cli.name, products[cli.name], "Qualified thin native label CLI",
        [native_id, *(source_member(n, sources[n]) for n in groups["qupathxx_labels"])], "derived_asset")
    consumer_inputs = set()
    for paths in groups.values():
        if "examples/ngff_label_plugin/main.cpp" in paths:
            consumer_inputs.update(paths)
    assert consumer_inputs, "SDK consumer input group was not observed by the final collector"
    consumer_id = pkg.add("bin/" + consumer.name, products[consumer.name], "Qualified installed-route public native SDK consumer",
        [native_id, *(source_member(n, sources[n]) for n in sorted(consumer_inputs))], "derived_asset")
    module_parents = [source_member(n, sources[n]) for n in groups["qpx_ngff_labels_plugin"]]
    module_parents += [pkg.members[n] for n in pkg.files if n.startswith("source/reader-plugin-kit/")]
    module_parents += [availability_id, runtime_ids["libopenslide-1.dll"]]
    module_id = pkg.add("bin/" + module.name, products[module.name],
        "Qualified optional native NGFF0.4/Zarr2 integral-label provider; unchanged native OmeZarrSource decoder",
        module_parents, "derived_asset")
    pkg.add("bin/" + manifest.name, products[manifest.name], "Exact qualified provider manifest", [module_id], "asset")
    config = (ROOT / "cmake/qpx_ngff_labelsConfig.cmake.in").read_bytes()
    assert re.search(rb'set\(qpx_ngff_labels_NATIVE_SHA256 "' + NATIVE_SHA.encode() + rb'"\)', config)
    assert str(ROOT).encode().lower() not in config.lower(), "Installed SDK config is not relocatable"
    config_id = pkg.add("lib/cmake/qpx_ngff_labels/qpx_ngff_labelsConfig.cmake", config,
        "Relocatable exact-base SDK addon config; current matching source template", [native_id])
    for name, path, parents in [
        ("compiled-inputs.json", compiled_path, [native_id, module_id]),
        ("native-object-reuse.json", reuse_path, object_ids),
        ("validation.json", args.validation, [cli_id, module_id]),
        ("sdk-check.json", args.sdk_check, [consumer_id, config_id]),
        ("reference-intake.json", packet / "reference-intake.json", reference_ids),
        ("W3C-intake.json", packet / "references/licenses/intake.json", [w3c_id]),
        ("reader-kit-inputs.json", packet / "reader-kit-inputs.json", [module_id]),
    ]:
        pkg.add("source/receipts/" + name, path.read_bytes(), "Exact source/qualification/reference receipt " + str(path), parents, "asset")

    notice = """New provider, generic reader/JSON helpers, application services, examples, tests and development controls: AGPL-3.0-only, qupathxx contributors. Exact corresponding source is under source/; full notice is licenses/qupathxx/LICENSE. Retained scientific objects have their exact recorded source/private headers and source identities included.
JSON3.12.0 headers: MIT, Niels Lohmann and per-header contributors. Original header declarations and licenses/nlohmann-json/LICENSE.MIT remain attached. SQLite3.54.1 interface: public-domain author declaration/blessing, SQLite authors; source and licenses/sqlite/sqlite-LICENSE.txt included.
GEOS3.15.0 C headers and unchanged geos.dll/geos_c.dll: LGPL-2.1-or-later. Preserve licenses/geos/COPYING and the separately retained exact release-src-geos corresponding source. OpenSlide runtime: LGPL-2.1-only binary record. Its source archive retains its existing inventory context and all19 bundled component notices under licenses/openslide-sdk, with distinct LGPL/MIT/BSD/IJG/Zlib/libpng/libtiff/public-domain terms. See FROZEN-SOURCE-AVAILABILITY.json; plugin separation does not alter these obligations.
Pinned NGFF0.4.1 specification reports: W3C-20150513, OME-NGFF contributors; full W3C notice is licenses/ngff/W3C-20150513.txt and original report notices remain under source/references/ngff. No upstream sample implementation, image, dataset, model or restricted material is included.
The exact FP04 SDK/core and full first-party/dependency source products are separate required retained products, not rebuilt or requalified here. Copied runtime DLLs and all original SDK component notices retain their exact material IDs/terms. For distribution, provide the matching corresponding source and required notices under each actual component license. Preserve applicable LGPL replacement/relinking rights; native/label-objects and source rebuild recipes support modification/relinking as applicable. The external Microsoft toolchain/CRT follows the recorded Release /MD profile; no Microsoft source or redistributable is packaged.
"""
    pkg.add("NOTICE.txt", notice.encode("utf8"), "Actual component terms, notices and source/replacement obligations", kind="notice")
    guide = """# Optional native NGFF integral labels

Read source/plugins/ngff_labels/README.md and source/docs/label-analysis.md for
the selected NGFF0.4/Zarr2 integral-label, source/frame/plane/crop and replay
contract. This local Windows x64 Release /MD packet includes the optional
provider, native SDK addon, CLI/public consumer, exact reused objects, matching
source and component notices. The exact frozen FP04 SDK/core and source products
remain separate required products; FROZEN-SOURCE-AVAILABILITY.json records them.

Keep the bin/ provider, manifest and exact runtime DLLs together. For example:
  bin/qupathxx_labels.exe --reader-plugins bin/ngff-label-reader-plugins.json --readers
Use an absolute manifest path in an application workflow; see the label guide.
No Python, Java or network service is required by native applications.

Installed SDK consumer: configure source/examples/ngff_label_plugin in a fresh
cache with CMAKE_PREFIX_PATH="<packet>;<frozenSDK>", Windows x64 MSVC1944,
compiler19.44.35225.0/toolset14.44.35207 and Release /MD; build ngff_label_consumer.
The relocatable config verifies both the exact base core and addon library.

Native source rebuild: configure source/examples/ngff_labels_host with the
frozen SDK on CMAKE_PREFIX_PATH and QUPATHXX_LABELS_OBJECT_DIR pointing to
<packet>/native/label-objects. Alternatively omit that option and provide
QUPATHXX_LABELS_GEOS_INCLUDE=<packet>/source/third_party/native-overlay/geos to
rebuild the five scientific units from matching source. JSON/SQLite interfaces
default to source/third_party/native-overlay. A rebuild gets its own identity.

Provider source rebuild: configure source/plugins/ngff_labels with the frozen
SDK prefix, QUPATHXX_READER_PLUGIN_KIT_PREFIX=<packet>/source/reader-plugin-kit
and QUPATHXX_NGFF_LABELS_JSON_INCLUDE=<packet>/source/third_party/native-overlay/json/include.
The provider uses source/plugins/image-common and source/plugins/format-common;
matching generated profile inputs are retained under source/generated.
source/frozen-sdk-interface records the exact older SDK header context and does
not replace either current application public headers or the separate base SDK.
Its generated sdk_abi.hpp remains exclusively in the required frozen SDK;
DERIVATION.json records its exact authored template, recipe and MSVC1944
substitution. That generated header is not a packet-owned file.

SOURCE.json binds source/binary/qualification identities. PACKAGE.json owns the
exact staged files. This stager does not claim install/removal acceptance; that
evidence is recorded by the separate delivery check. Before removal, close active
module users and remove only verified package-owned files. Preserve user inputs,
projects/results/notes, the frozen SDK and retained source products. No GUI,
biological/general NGFF/platform/full-catalogue or Python/Java acceptance is
claimed by this packet. Distribution retains each component's actual obligations.
"""
    pkg.add("README.md", guide.encode("utf8"), "Packet-local use/source/build/removal and support scope", kind="asset")
    provider_sources = {n: sources[n] for n in groups["qpx_ngff_labels_plugin"]}
    metadata = {
        "schema": "qpx.ngff-label-delivery/1", "profile": PROFILE,
        "providers": {PROVIDER: {"module_sha256": sha(products[module.name]), "version": "1.0.0",
            "contract": CONTRACT, "source_context_sha256": context(provider_sources), "source_inputs": provider_sources,
            "licenses": ["AGPL-3.0-only"], "dependencies": entry["dependencies"],
            "validation_sha256": file_sha(args.validation)}},
        "native_library_sha256": NATIVE_SHA, "cli_sha256": sha(products[cli.name]),
        "sdk_consumer_sha256": sha(products[consumer.name]), "sdk_check_sha256": file_sha(args.sdk_check),
        "compiled_inputs_sha256": file_sha(compiled_path), "compiled_context_sha256": context({"sources": sources, "groups": groups, "profile": compiled["profile"]}),
        "native_source_context_sha256": context(sources), "host_inputs": {n: sources[n] for n in groups["ngff_native"]},
        "compiled_input_stage_paths": staged_inputs, "native_object_reuse": reuse,
        "base_sdk": base["base_sdk"], "frozen_source_availability_sha256": sha(encoded(availability)),
        "component_terms": {"new_code": "AGPL-3.0-only", "json": "MIT", "sqlite": "blessing",
            "geos": "LGPL-2.1-or-later", "openslide_binary": "LGPL-2.1-only",
            "openslide_source_inventory": "LGPL-2.1-or-later; individual bundled notices retained",
            "ngff_reports": "W3C-20150513"},
        "scope": "Selected local native integral-label import/publication/export/replay and source-matched addon; separate frozen SDK/core, no broad format/UI/biological/platform/full-goal claim",
        "install_or_removal_qualification": "Separate delivery check follows staging; not asserted by this writer",
        "archive_created": False,
    }
    result = pkg.finish(output, "ngff-labels-" + sha(products[module.name])[:16] + "-" + context(sources)[:8],
                        metadata, inventory_path, args.write_inventory)
    print(json.dumps({"files": result["files"], "admission_exit": result["admission_exit"], "output": str(output)}))


if __name__ == "__main__":
    main()
