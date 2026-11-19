"""Stage admitted geometry module/kit and a separate matching native SDK overlay.

Development packaging only; no product import/identity/publication logic lives in
Python. Exact unchanged base SDK sources/notices retain their separate receipt.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
GROUP = "annotation_geometry_plugin"
PRODUCT = ("plugins/geometry_tsv/CMakeLists.txt", "plugins/geometry_tsv/geometry_tsv.cpp",
           "plugins/format-common/strict_tsv.hpp", "plugins/format-common/plugin_build.cmake",
           "plugins/format-common/format_plugin_profile.hpp.in", "plugins/format-common/write_manifest.cmake")
HOST = ("cpp/format_plugin_loader.cpp", "cpp/format_plugin_runtime.cpp", "cpp/format_import.cpp", "cpp/format_import_main.cpp",
        "cpp/annotation_geometry_value.cpp")
EXTRA = ("plugins/geometry_tsv/README.md", "cpp/tests/annotation_geometry_tests.cpp", "cpp/tests/annotation_geometry_bad_module.cpp",
         "cpp/tests/format_plugin_c_header_test.c", "tools/ci/check_annotation_geometry.py", "tools/ci/check_format_family_plugins.py",
         "tools/ci/stage_annotation_geometry.py", "examples/annotation_geometry/CMakeLists.txt", "examples/annotation_geometry/main.cpp",
         "examples/annotation_geometry_host/CMakeLists.txt", "docs/benchmarks/annotation-geometry/README.md")
BASE_CORE = "4bf957c9ea6e46294c11b9bfb7d9c2496cf4fca607a9c28ba837768d301eb6bb"
JSON_ARCHIVE = "42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa"
SQLITE_ARCHIVE = "0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9"
sha = lambda data: hashlib.sha256(data).hexdigest()


def own_headers(sources=HOST):
    """All first-party transitive includes of the four compiled host sources."""
    pending, seen = [ROOT / name for name in sources], set()
    while pending:
        path = pending.pop().resolve()
        if path in seen:
            continue
        seen.add(path)
        for include in re.findall(r'^#include\s+["<]([^">]+)[">]', path.read_text(encoding="utf8"), re.M):
            for candidate in [path.parent / include, ROOT / "cpp/include" / include, ROOT / "cpp" / include]:
                if candidate.is_file():
                    pending.append(candidate)
                    break
    return sorted(path.relative_to(ROOT).as_posix() for path in seen)


def update_group(path, group, name=GROUP):
    text = path.read_text(encoding="utf8")
    encoded = json.dumps(group, ensure_ascii=False, indent=2).replace("\n", "\n  ")
    match = re.search(r'^  "' + re.escape(name) + '": ', text, re.M)
    if match:
        start = match.end()
        _, size = json.JSONDecoder().raw_decode(text[start:])
        text = text[:start] + encoded + text[start + size:]
    else:
        end = text.rfind("}")
        text = text[:end].rstrip() + ',\n  "' + name + '": ' + encoded + "\n}\n"
    json.loads(text)
    path.write_text(text, encoding="utf8", newline="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["module", "overlay", "validation", "compiled-inputs", "kit", "sdk", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    module, overlay = args.module.read_bytes(), args.overlay.read_bytes()
    validation = json.loads(args.validation.read_text(encoding="utf8"))
    assert validation["passed"] and validation["artifacts_unchanged"] and validation["fixtures_unchanged"]
    for name, data in [("module", module), ("overlay", overlay)]:
        assert validation["artifacts_before"][name]["sha256"] == sha(data), "Unqualified artifact: " + name
    compiled = json.loads(args.compiled_inputs.read_text(encoding="utf8"))
    for name, digest in compiled.items():
        assert sha((ROOT / name).read_bytes()) == digest, "Changed compiled source: " + name
    headers = own_headers()
    source_hashes = {name: sha((ROOT / name).read_bytes()) for name in PRODUCT}
    context = sha(json.dumps(source_hashes, sort_keys=True, separators=(",", ":")).encode())
    host_hashes = {name: sha((ROOT / name).read_bytes()) for name in headers}
    host_context = sha(json.dumps(host_hashes, sort_keys=True, separators=(",", ":")).encode())
    sdk_core = args.sdk / "lib/qupathxx/qupathxx_core.lib"
    assert sha(sdk_core.read_bytes()) == BASE_CORE, "The frozen base SDK changed"
    base = json.loads((ROOT / "build/format-family-plugins/results.json").read_text(encoding="utf8"))
    assert sha(Path(base["native_source_snapshot"]["archive"]).read_bytes()) == base["native_source_snapshot"]["archive_sha256"]
    manifest = args.module.parent / "format-plugins.json"
    manifest_bytes = manifest.read_bytes()
    descriptor = json.loads(manifest_bytes)
    item = descriptor["plugins"][0]
    assert len(descriptor["plugins"]) == 1 and descriptor["schema"] == "qpx.format-plugins/1"
    assert item["module"]["sha256"] == sha(module) and item["provider_id"] == "org.qupathxx.geometry-tsv" and item["abi"] == 2
    assert item["version"] == "1.0.0" and item["contract"] == "pixel-geometry-annotation-tsv/1" and item["dependencies"] == []
    assert validation["artifacts_before"]["manifest"]["sha256"] == sha(manifest_bytes)
    output = args.output.resolve()
    assert not output.exists(), "Use a fresh stage; preserve prior evidence"
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf8"))
    group = inventory.get(GROUP, {"scope": "Local Windows x64 geometry module/ABI2 kit and separate native service overlay over exact frozen FP04 SDK; no base SDK/runtime repack, publication, GUI or wheel", "members": [], "deliveries": {}})
    records = {row["material"]["id"]: row for row in group["members"]}
    files, members, identities = {}, {}, {}

    def add(name, data, origin, parents=("qupathxx-root-license",), kind="code", license_id="AGPL-3.0-only", evidence="LICENSE; repository author declaration", attribution="qupathxx contributors"):
        ident = "annotation-geometry-" + license_id + "-" + sha(data)[:24]
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
                  "material": {"id": ident, "category": "admitted_open", "kind": kind, "license_id": license_id,
                               "license_evidence": evidence, "attribution": attribution, "source_ids": list(dict.fromkeys(parents))}}
        records.setdefault(ident, record)
        files[name] = data
        members[name] = ident
        identities[name] = dict(sha256=sha(data), bytes=len(data), material_id=ident)
        return ident

    for name in sorted(set(PRODUCT + HOST + EXTRA + tuple(headers))):
        add("source/" + name, (ROOT / name).read_bytes(), name, kind="asset" if name.endswith(".md") else "code")
    kit_ids = []
    kit_hashes = {}
    for name in ["include/qupathxx/reader_plugin.h", "include/qupathxx/format_plugin.h", "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE"]:
        data = (args.kit / name).read_bytes()
        kit_hashes[name] = sha(data)
        kit_ids.append(add("source/format-plugin-kit/" + name, data, "matching annotation ABI2 kit " + name, kind="notice" if name.endswith("LICENSE") else "code"))
    assert (args.kit / "include/qupathxx/format_plugin.h").read_bytes() == (ROOT / "cpp/include/qupathxx/format_plugin.h").read_bytes()
    for name in ["source/LICENSE", "licenses/qupathxx/LICENSE"]:
        add(name, (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")

    # Verify the actual native-build headers against their already pinned source
    # archives. These components belong only to the native overlay, not the DLL.
    json_archive = ROOT / "build/deps/json-3.12.0.tar.xz"
    sqlite_archive = ROOT / "build/deps/sqlite-amalgamation-3540100.zip"
    assert sha(json_archive.read_bytes()) == JSON_ARCHIVE and sha(sqlite_archive.read_bytes()) == SQLITE_ARCHIVE
    dependency_ids = []
    with tarfile.open(json_archive) as archive:
        license_bytes = archive.extractfile("json/LICENSE.MIT").read()
        license_ident = add("licenses/nlohmann-json/LICENSE.MIT", license_bytes, "Pinned json3.12.0/LICENSE.MIT in source archive " + JSON_ARCHIVE,
                            (), "notice", "MIT", "licenses/nlohmann-json/LICENSE.MIT", "Niels Lohmann")
        for member in archive.getmembers():
            if member.isfile() and member.name.startswith("json/include/nlohmann/"):
                relative = member.name[len("json/include/"):]
                data = archive.extractfile(member).read()
                assert data == (ROOT / "build/ci/gridfrontends167-package/_deps/json-src/include" / relative).read_bytes(), "Changed native JSON header"
                assert b"SPDX-License-Identifier: MIT" in data[:4096], "Review any differently licensed JSON source separately"
                authors = "; ".join(re.findall(r"SPDX-FileCopyrightText: ([^\r\n]+)", data.decode("utf8")))
                dependency_ids.append(add("source/third_party/native-overlay/json/include/" + relative, data,
                    "Pinned MIT json3.12.0 " + member.name + "; archive " + JSON_ARCHIVE, (license_ident,), "code", "MIT", "licenses/nlohmann-json/LICENSE.MIT", authors))
    with zipfile.ZipFile(sqlite_archive) as archive:
        data = archive.read("sqlite-amalgamation-3540100/sqlite3.h")
        assert data == (ROOT / "build/deps/sqlite-amalgamation-3540100/sqlite3.h").read_bytes()
        notice = (args.sdk / "share/qupathxx/licenses/sqlite/sqlite-LICENSE.txt").read_bytes()
        license_ident = add("licenses/sqlite/sqlite-LICENSE.txt", notice, "Existing matching SQLite3.54.1 SDK notice and header author declaration",
                            (), "notice", "blessing", "licenses/sqlite/sqlite-LICENSE.txt; sqlite3.h author declaration", "SQLite authors")
        dependency_ids.append(add("source/third_party/native-overlay/sqlite/sqlite3.h", data,
            "Pinned SQLite3.54.1 sqlite3.h in source archive " + SQLITE_ARCHIVE, (license_ident,), "code", "blessing", "licenses/sqlite/sqlite-LICENSE.txt; sqlite3.h author declaration", "SQLite authors"))
    module_parents = [members["source/" + name] for name in PRODUCT] + kit_ids
    module_ident = add("qpx_geometry_tsv_plugin.dll", module, "Qualified independent geometry provider; no external codec/core/parser dependency", module_parents)
    add("format-plugins.json", manifest_bytes, "Exact geometry provider runtime manifest", (module_ident,), "asset")
    overlay_ident = add("native/annotation_overlay.lib", overlay, "Qualified separate native service archive; external frozen SDK stays separate",
                         [members["source/" + name] for name in headers] + dependency_ids)
    config = '''include(CMakeFindDependencyMacro)
find_dependency(qupathxx 0.1.0 CONFIG COMPONENTS core)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR NOT MSVC_VERSION STREQUAL "1944" OR NOT CMAKE_CXX_COMPILER_VERSION STREQUAL "19.44.35225.0")
  message(FATAL_ERROR "Use the recorded Windows x64 MSVC1944/compiler19.44.35225.0 profile")
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Native geometry configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "The native geometry overlay requires Release")
endif()
get_target_property(qpx_base_core qupathxx::core IMPORTED_LOCATION_RELEASE)
file(SHA256 "${qpx_base_core}" qpx_base_core_sha)
if(NOT qpx_base_core_sha STREQUAL "''' + BASE_CORE + '''")
  message(FATAL_ERROR "The native geometry overlay needs its exact frozen FP04 SDK core")
endif()
get_filename_component(qpx_geometry_package "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET qpx_annotation_geometry::native)
  add_library(qpx_annotation_geometry::native STATIC IMPORTED)
  set_target_properties(qpx_annotation_geometry::native PROPERTIES
    IMPORTED_LOCATION "${qpx_geometry_package}/native/annotation_overlay.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${qpx_geometry_package}/source/cpp/include"
    INTERFACE_LINK_LIBRARIES "qupathxx::core" INTERFACE_COMPILE_FEATURES cxx_std_17)
endif()
'''
    add("lib/cmake/qpx_annotation_geometry/qpx_annotation_geometryConfig.cmake", config.encode(), "Generated exact-base native SDK overlay config", (overlay_ident,), "code")
    guide = ("# Native annotation geometry package\n\n"
             "Provider org.qupathxx.geometry-tsv version1.0.0, annotation ABI2, pixel-geometry-annotation-tsv/1.\n"
             "See source/plugins/geometry_tsv/README.md and source/docs/benchmarks/annotation-geometry/README.md.\n\n"
             "The module/source/format-plugin-kit is independent of the core/SDK and Python. Configure/build source/plugins/geometry_tsv "
             "with QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX set to the included absolute source/format-plugin-kit. No downloads are needed.\n\n"
             "native/annotation_overlay.lib and source/cpp/include are a separate matching native service overlay. They require the exact "
             "retained FP04 SDK core recorded by SOURCE.json; do not replace only an old SDK header. Its core, runtime libraries, full corresponding "
             "source archive and dependency notices remain separate retained products. No base SDK/runtime, GUI, wheel or release is repackaged here.\n\n"
             "For a public-only SDK consumer, configure source/examples/annotation_geometry with CMAKE_PREFIX_PATH containing this package root "
             "and the matching SDK. For a source rebuild of the overlay/CLI, configure source/examples/annotation_geometry_host with the matching SDK; "
             "exact MIT JSON and public-domain SQLite interface headers/notices are supplied under source/third_party/native-overlay. "
             "The DLL does not link those components. Full AGPL source/notices and actual dependency terms accompany their components.\n\n"
             "Keep the module/manifest together; select the provider in the source-matched host. Close module users before removing package-owned files. "
             "Preserve project/image/result/note data and the separate frozen SDK.\n")
    add("README.md", guide.encode(), "Generated package-local geometry/SDK build guide", kind="asset")
    notices = ("Geometry provider, format/ABI/kit, native overlay, authored fixtures/tools/examples: qupathxx contributors, AGPL-3.0-only.\n"
               "Exact new corresponding source: source/. Full text: licenses/qupathxx/LICENSE. Applicable AGPL source/notice obligations remain.\n"
               "Native overlay uses nlohmann-json3.12.0 headers: MIT, Niels Lohmann; exact source/license included.\n"
               "Native overlay source uses SQLite3.54.1 public interface header: public domain author declaration/blessing; exact header/notice included.\n"
               "The standalone geometry DLL has no external parser/codec/core/Python dependency. Platform CRT/compiler follow the kit.\n"
               "The exact existing SDK core/runtime and its dependencies/source/notices retain the separate recorded FP04 receipt and actual terms.\n")
    add("NOTICES.txt", notices.encode(), "Generated actual component/source notices", kind="notice")
    provenance = {"schema": "qpx.annotation-geometry-package/1", "module_sha256": sha(module), "overlay_sha256": sha(overlay),
                  "product_source_sha256": context, "product_inputs": source_hashes, "host_source_sha256": host_context,
                  "host_inputs": host_hashes, "kit_inputs": kit_hashes, "validation_sha256": sha(args.validation.read_bytes()),
                  "base_sdk": {"core_sha256": BASE_CORE, "source_context_sha256": base["native_source_context_sha256"],
                               "source_archive": Path(base["native_source_snapshot"]["archive"]).name,
                               "source_archive_sha256": base["native_source_snapshot"]["archive_sha256"],
                               "scope": "Separate exact retained SDK/core/runtime/source/notices; not repackaged or requalified by the overlay"},
                  "components": {"module_and_native_overlay": "AGPL-3.0-only", "native_json_headers": "MIT", "native_sqlite_header": "blessing"},
                  "upstream_sources": {"nlohmann-json3.12.0": JSON_ARCHIVE, "sqlite3.54.1": SQLITE_ARCHIVE}}
    add("SOURCE.json", (json.dumps(provenance, indent=2) + "\n").encode(), "Exact module/host/kit/component/base source identities", (module_ident, overlay_ident), "asset")
    group["members"] = list(records.values())
    group["deliveries"][sha(module)] = {"product_source_sha256": context, "host_source_sha256": host_context,
                                        "overlay_sha256": sha(overlay), "members": members}
    if args.write_inventory:
        update_group(inventory_path, group)
    else:
        assert all(ident in {r["material"]["id"] for r in inventory.get(GROUP, {}).get("members", [])} for ident in members.values()), "New exact members need a scoped inventory update"
    for name, data in files.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    r = subprocess.run([sys.executable, str(ROOT / "tools/ci/check_material_staging.py"), "--stage", str(output), "--inventory", str(inventory_path)], capture_output=True, text=True, encoding="utf8")
    record = dict(provenance, files=len(files), members=identities, admission_exit=r.returncode, admission_output=r.stdout + r.stderr)
    (output.parent / (output.name + "-admission.json")).write_text(json.dumps(record, indent=2) + "\n", encoding="utf8")
    assert r.returncode == 0, record["admission_output"]
    print(json.dumps(dict(admitted=True, files=len(files), module_sha256=sha(module), overlay_sha256=sha(overlay))))


if __name__ == "__main__":
    main()
