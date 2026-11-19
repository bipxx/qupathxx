"""Stage one validated GeoPackage module with its pinned SQLite source."""
import argparse
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from check_material_staging import check_archive, inventory
from stage_annotation_sources import Package, ROOT, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("module", "validation", "kit", "sqlite", "output", "archive"):
        parser.add_argument("--" + key, type=Path, required=True)
    parser.add_argument("--registration-validation", type=Path)
    args = parser.parse_args()
    module = args.module.read_bytes()
    manifest = args.module.with_name("format-plugins.json").read_bytes()
    item, = json.loads(manifest)["plugins"]
    validated = json.loads(args.validation.read_bytes())
    assert validated["passed"] and validated["module_sha256"] == sha(module)
    assert validated["manifest_sha256"] == sha(manifest) and item["module"]["sha256"] == sha(module)
    assert item["module"]["path"] == args.module.name and item["provider_id"] == "org.qupathxx.geopackage"
    assert item["abi"] == 3 and not item["dependencies"]
    registration_sha = None
    if item["version"] == "1.1.0":
        assert args.registration_validation and item["contract"] == "gpkg-cartesian-image-pixel/2"
        registered = json.loads(args.registration_validation.read_bytes())
        assert registered["passed"] and registered["module_sha256"] == sha(module)
        assert registered["manifest_sha256"] == sha(manifest)
        assert len(registered["imports"]) == len(registered["reopens"]) == 3
        assert len(registered["refusals"]) == 13
        registration_sha = sha(args.registration_validation.read_bytes())
    assert not args.output.exists() and not args.archive.exists()
    inv = ROOT / "tests/fixtures/manifest.json"
    original = inv.read_bytes()
    admitted = inventory([inv])
    pkg = Package(json.loads(original), "Native image-pixel GeoPackage annotation importer and pinned private SQLite",
                  "geopackage_annotation_plugin")
    product = (
        *(f"plugins/geopackage/{name}" for name in ("CMakeLists.txt", "geopackage.cpp", "sqlite_pin.cmake")),
        *(f"plugins/format-common/{name}" for name in (
            "annotation_source.hpp", "binary_input.hpp", "reader_input.hpp", "wkb_geometry.hpp",
            "json_tree.hpp", "plugin_build.cmake", "format_plugin_profile.hpp.in", "write_manifest.cmake")),
    )
    if registration_sha:
        product += ("cpp/include/qupathxx/detail/affine_numeric.hpp",)
    parents = [pkg.add("source/" + name, (ROOT / name).read_bytes(), name) for name in product]
    for name in ("plugins/geopackage/tests/CMakeLists.txt", "plugins/geopackage/tests/project_probe.cpp",
                 "plugins/geopackage/tests/write_fixture.py", "tools/ci/check_geopackage_annotations.py",
                 "tools/ci/check_project_matrix_index.py", "tools/ci/check_geopackage_registration.py",
                 "tools/ci/stage_geopackage_annotations.py"):
        pkg.add("source/" + name, (ROOT / name).read_bytes(), name)
    for name in ("include/qupathxx/format_plugin.h", "include/qupathxx/reader_plugin.h",
                 "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE"):
        parents.append(pkg.add("source/format-plugin-kit/" + name, (args.kit / name).read_bytes(), name,
                               kind="notice" if name.endswith("LICENSE") else "code"))
    sqlite = args.sqlite.read_bytes()
    notice = (ROOT / "third_party/sqlite-LICENSE.txt").read_bytes()
    assert notice == (args.module.parent / "licenses/sqlite/sqlite-LICENSE.txt").read_bytes()
    for file, ident, data in (
        ("source/third_party/sqlite-amalgamation-3540100.zip", "release-src-sqlite", sqlite),
        ("source/third_party/sqlite-LICENSE.txt", "annotation-geometry-blessing-f6d54bedc1907c4fed5c3e6c", notice),
        ("licenses/sqlite/sqlite-LICENSE.txt", "annotation-geometry-blessing-f6d54bedc1907c4fed5c3e6c", notice),
    ):
        assert sha(data) == admitted[ident]["sha256"] and len(data) == admitted[ident]["bytes"]
        pkg._member(file, data, ident)
        parents.append(ident)
    pkg.add("README.md", (ROOT / "plugins/geopackage/README.md").read_bytes(), "GeoPackage guide", kind="asset")
    pkg.add("licenses/qupathxx/LICENSE", (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")
    binary = pkg.add(args.module.name, module, "Qualified native GeoPackage provider", parents)
    pkg.add("format-plugins.json", manifest, "Qualified manifest", [binary], "asset")
    metadata = {
        "providers": {item["provider_id"]: {"module_sha256": sha(module), "abi": item["abi"],
                                            "version": item["version"], "contract": item["contract"]}},
        "validation_sha256": sha(args.validation.read_bytes()), "validated_host_sha256": validated["host_sha256"],
        "source_files": {name: sha(data) for name, data in pkg.files.items() if name.startswith("source/")},
    }
    if registration_sha:
        metadata["registration_validation_sha256"] = registration_sha
    try:
        pkg.finish(args.output, sha(module), metadata, inv, True)
        with ZipFile(args.archive, "x", ZIP_DEFLATED) as z:
            for name in sorted(pkg.members):
                z.write(args.output / name, name)
        check_archive(args.archive, inventory([inv]), expected=pkg.members)
    except Exception:
        inv.write_bytes(original)
        raise
    print(f"Admitted {len(pkg.members)} GeoPackage delivery and ZIP members")


if __name__ == "__main__":
    main()
