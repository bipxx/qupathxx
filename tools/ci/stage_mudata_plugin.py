"""Stage the qualified optional MuData provider and matching source inputs."""
import argparse
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from check_material_staging import check_archive, inventory
from stage_annotation_sources import Package, ROOT, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("module", "validation", "kit", "loom-stage", "loom-admission",
                "hdf5-archive", "zlib-archive", "output", "archive"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    module = args.module.read_bytes()
    manifest = args.module.with_name("format-plugins.json").read_bytes()
    item, = json.loads(manifest)["plugins"]
    validated = json.loads(args.validation.read_bytes())
    assert validated["passed"] and validated["module_sha256"] == sha(module)
    assert validated["manifest_sha256"] == sha(manifest) and item["module"]["sha256"] == sha(module)
    assert item["module"]["path"] == args.module.name and item["provider_id"] == "org.qupathxx.mudata"
    assert item["abi"] == 5 and not item["dependencies"]
    assert not args.output.exists() and not args.archive.exists()
    inv = ROOT / "tests/fixtures/manifest.json"
    original = inv.read_bytes()
    existing = inventory([inv])
    pkg = Package(json.loads(original), "Native MuData modality providers with exact HDF5 and zlib inputs",
                  "mudata_matrix_plugin")
    source = (
        "plugins/mudata/CMakeLists.txt", "plugins/mudata/mudata.cpp", "plugins/mudata/mudata_source.hpp",
        *(f"plugins/format-common/{name}" for name in (
            "hdf5_input.hpp", "reader_input.hpp", "json_tree.hpp", "plugin_build.cmake",
            "format_plugin_profile.hpp.in", "write_manifest.cmake")),
        "cmake/H5adReader.cmake", "cmake/PinnedZlib.cmake",
    )
    parents = [pkg.add("source/" + name, (ROOT / name).read_bytes(), name) for name in source]
    for name in ("plugins/mudata/tests/CMakeLists.txt", "plugins/mudata/tests/project_probe.cpp",
                 "tools/ci/check_mudata_plugin.py", "tools/ci/check_counts_npz_plugin.py"):
        pkg.add("source/" + name, (ROOT / name).read_bytes(), name)
    for name in ("include/qupathxx/format_plugin.h", "include/qupathxx/reader_plugin.h",
                 "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE"):
        parents.append(pkg.add("source/format-plugin-kit/" + name, (args.kit / name).read_bytes(), name,
                               kind="notice" if name.endswith("LICENSE") else "code"))
    admitted_loom = json.loads(args.loom_admission.read_bytes())
    assert admitted_loom["admission_exit"] == 0
    header_prefixes = ("source/third_party/loom/hdf5/generated/", "source/third_party/loom/hdf5/src/",
                       "source/third_party/loom/zlib/include/")
    for name in sorted(admitted_loom["members"]):
        if name.startswith(header_prefixes) or name in ("licenses/hdf5/LICENSE", "licenses/zlib/LICENSE"):
            pkg.borrow(name, args.loom_stage, admitted_loom)
            parents.append(pkg.members[name])
    for path, ident, name in ((args.hdf5_archive, "release-src-hdf5", "hdf5-2.2.0.tar.gz"),
                              (args.zlib_archive, "release-src-zlib", "zlib-1.3.2.tar.gz")):
        data = path.read_bytes()
        assert sha(data) == existing[ident]["sha256"] and len(data) == existing[ident]["bytes"]
        pkg._member("source/third_party/" + name, data, ident)
        parents.append(ident)
    for name in ("hdf5", "zlib"):
        assert pkg.files[f"licenses/{name}/LICENSE"] == (args.module.parent / "licenses" / name / "LICENSE").read_bytes()
    pkg.add("README.md", (ROOT / "plugins/mudata/README.md").read_bytes(), "MuData instructions", kind="asset")
    pkg.add("licenses/qupathxx/LICENSE", (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")
    binary = pkg.add(args.module.name, module, "Qualified native MuData provider", parents)
    pkg.add("format-plugins.json", manifest, "Qualified manifest", [binary], "asset")
    metadata = {
        "providers": {item["provider_id"]: {"module_sha256": sha(module), "abi": item["abi"], "contract": item["contract"]}},
        "validation_sha256": sha(args.validation.read_bytes()), "validated_host_sha256": validated["host_sha256"],
        "static_link_inputs": {
            "hdf5_sha256": "ef5a6cfebd2fd9d5bf2b5f449980fcf8676cb64fc42048e952f3c9f3b272f43c",
            "zlib_sha256": "4ec525ee88a54ccf5d33ef1a5c8105533ad19f0ac946b71b52c8a824cbefb63d",
        },
        "source_files": {key: sha(data) for key, data in pkg.files.items() if key.startswith("source/")},
    }
    try:
        pkg.finish(args.output, sha(module), metadata, inv, True)
        with ZipFile(args.archive, "x", ZIP_DEFLATED) as z:
            for name in sorted(pkg.members):
                z.write(args.output / name, name)
        check_archive(args.archive, inventory([inv]), expected=pkg.members)
    except Exception:
        inv.write_bytes(original)
        raise
    print(f"Admitted {len(pkg.members)} MuData package and ZIP members")


if __name__ == "__main__":
    main()
