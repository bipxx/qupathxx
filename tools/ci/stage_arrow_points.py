"""Stage a verified native Arrow module with corresponding library sources."""
import argparse
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from check_material_staging import check_archive, inventory
from prepare_arrow_points_sources import UPSTREAM
from stage_annotation_sources import COMMON, Package, ROOT, sha

PROVIDERS = {
    "org.qupathxx.arrow-points": ("arrow_points", 3, "annotation", "Arrow IPC image-pixel points"),
    "org.qupathxx.arrow-matrix": ("arrow_matrix", 2, "matrix", "Arrow IPC expression matrix"),
}
ZSTD_SOURCE_ID = "ngff05-BSD-3-Clause-source_archive-eb33e51f49a15e023950cd78-ec4cece39fb4"
ZSTD_NOTICE_ID = "ngff05-BSD-3-Clause-notice-7055266497633c9025b777c7"
PINS = {
    "nanoarrow-0.9.0": "6935c5b84e0906b1843b5c93eb20ffab6aab9c06311ebb3a9a5b97fbb151d60f",
    "lz4-1.10.0": "e9fb04130b7fc33727fa9e8e747ed067a8a0e7bef4b3769d689e2953a245d9c0",
}
PARENT_NOTICE = {
    "nanoarrow-0.9.0": {
        "nanoarrow/LICENSE.txt": "LICENSE.txt",
        "nanoarrow/NOTICE.txt": "NOTICE.txt",
        "flatcc/LICENSE": "thirdparty/flatcc/LICENSE",
    },
    "lz4-1.10.0": {"lz4/LICENSE": "lib/LICENSE"},
}
LICENSES = {
    "nanoarrow-0.9.0": ("Apache-2.0", "The Apache Software Foundation and Mikkel F. Jørgensen"),
    "lz4-1.10.0": ("BSD-2-Clause", "Yann Collet"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("install", "validation", "kit", "libs", "zstd", "output", "archive"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    manifest = (args.install / "format-plugins.json").read_bytes()
    item, = json.loads(manifest)["plugins"]
    provider_id = item["provider_id"]
    plugin, abi, domain, description = PROVIDERS[provider_id]
    binary = (args.install / item["module"]["path"]).read_bytes()
    verified = json.loads(args.validation.read_bytes())
    assert verified["passed"] and verified["module_sha256"] == sha(binary) == item["module"]["sha256"]
    assert verified["manifest_sha256"] == sha(manifest)
    assert item["abi"] == abi and item["domain"] == domain and not item["dependencies"]
    assert not args.output.exists() and not args.archive.exists()

    inv = ROOT / "tests/fixtures/manifest.json"
    original = inv.read_bytes()
    package = Package(json.loads(original), description, plugin + "_plugin")

    def external(name, data, component, kind, parents=()):
        license_id, author = LICENSES[component]
        ident = f"arrow-points-{component}-{sha(data)[:24]}"
        url, upstream_sha, _ = UPSTREAM[component]
        if ident not in package.known_records:
            package.records[ident] = {
                "filename": Path(name).name, "source": url, "upstream_sha256": upstream_sha,
                "bytes": len(data), "sha256": sha(data),
                "material": {
                    "id": ident, "category": "admitted_open", "kind": kind, "license_id": license_id,
                    "license_evidence": ", ".join("licenses/" + p for p in PARENT_NOTICE[component]),
                    "attribution": author, "source_ids": list(parents),
                },
            }
        package._member(name, data, ident)
        return ident

    sources = (*COMMON, "plugins/format-common/json_tree.hpp",
               f"plugins/{plugin}/CMakeLists.txt", f"plugins/{plugin}/{plugin}.cpp",
               *(f"plugins/arrow_points/{name}" for name in
                 ("arrow_decoder.cmake", "arrow_file.hpp", "arrow_file.cpp", "decoder_memory.c", "decoder_memory.h",
                  "decoder_allocators.h", "dictionary_append.h",
                  "ipc_bounds.c", "ipc_bounds.h", "nanoarrow_dictionary.cmake")))
    if domain == "matrix":
        sources += ("plugins/format-common/matrix_table.hpp",)
    else:
        sources += ("plugins/format-common/point_columns.hpp",)
    parents = [package.add("source/" + name, (ROOT / name).read_bytes(), name) for name in sources]
    for name in (f"plugins/{plugin}/tests/CMakeLists.txt", f"plugins/{plugin}/tests/project_probe.cpp",
                 f"tools/ci/check_{plugin}.py", "tools/ci/prepare_arrow_points_sources.py"):
        package.add("source/" + name, (ROOT / name).read_bytes(), name)
    for name in ("include/qupathxx/format_plugin.h", "include/qupathxx/reader_plugin.h",
                 "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE"):
        parents.append(package.add("source/format-plugin-kit/" + name, (args.kit / name).read_bytes(), name,
                                   kind="notice" if name.endswith("LICENSE") else "code"))

    for component, digest in PINS.items():
        path = args.libs / f"{component}-library.zip"
        data = path.read_bytes()
        assert sha(data) == digest
        with ZipFile(path) as archive:
            for path_in_package, path_in_source in PARENT_NOTICE[component].items():
                name = "licenses/" + path_in_package
                text = archive.read(UPSTREAM[component][2] + "/" + path_in_source)
                assert text == (args.install / name).read_bytes()
                external(name, text, component, "notice")
        parents.append(external("source/third_party/" + path.name, data, component, "source_archive"))

    zstd_archive = args.zstd.read_bytes()
    zstd_notice = (args.install / "licenses/zstd/LICENSE").read_bytes()
    for name, data, ident in (
        ("source/third_party/zstd-1.5.7.tar.gz", zstd_archive, ZSTD_SOURCE_ID),
        ("licenses/zstd/LICENSE", zstd_notice, ZSTD_NOTICE_ID),
    ):
        admitted = package.known_records[ident]
        assert sha(data) == admitted["sha256"] and len(data) == admitted["bytes"]
        package._member(name, data, ident)
    parents.append(ZSTD_SOURCE_ID)

    package.add("README.md", (ROOT / f"plugins/{plugin}/README.md").read_bytes(), description, kind="asset")
    package.add("licenses/qupathxx/LICENSE", (args.install / "licenses/qupathxx/LICENSE").read_bytes(),
                "FormatPluginKit LICENSE", kind="notice")
    binary_id = package.add(item["module"]["path"], binary, "Verified native provider " + provider_id, parents)
    package.add("format-plugins.json", manifest, "Verified module manifest", [binary_id], "asset")
    metadata = {
        "providers": {provider_id: {"module_sha256": sha(binary), "abi": abi, "contract": item["contract"],
                                 "version": item["version"], "profile": item["profile"]}},
        "libraries": {name: {"source_zip_sha256": digest, "license_id": LICENSES[name][0],
                             "upstream_sha256": UPSTREAM[name][1]} for name, digest in PINS.items()},
        "validation_sha256": sha(args.validation.read_bytes()),
        "validated_host_sha256": verified["host_sha256"],
        "source_files": {name: sha(data) for name, data in package.files.items() if name.startswith("source/")},
    }
    metadata["libraries"]["zstd-1.5.7"] = {
        "source_archive_sha256": sha(zstd_archive), "license_id": "BSD-3-Clause",
        "source_material_id": ZSTD_SOURCE_ID,
    }
    try:
        package.finish(args.output, sha(binary), metadata, inv, True)
        with ZipFile(args.archive, "x", ZIP_DEFLATED) as archive:
            for name in sorted(package.members):
                archive.write(args.output / name, name)
        check_archive(args.archive, inventory([inv]), expected=package.members)
    except Exception:
        inv.write_bytes(original)
        raise
    print(f"Admitted {len(package.members)} {provider_id} delivery and ZIP members")


if __name__ == "__main__":
    main()
