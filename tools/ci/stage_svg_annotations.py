"""Stage the installed SVG annotation module and corresponding source."""
import argparse
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from check_material_staging import check_archive, inventory
from stage_annotation_sources import Package, ROOT, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("module", "validation", "kit", "expat-stage", "expat-admission", "output", "archive"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    module = args.module.read_bytes()
    manifest = args.module.with_name("format-plugins.json").read_bytes()
    item, = json.loads(manifest)["plugins"]
    validated = json.loads(args.validation.read_bytes())
    assert validated["passed"] and validated["module_sha256"] == sha(module)
    assert validated["manifest_sha256"] == sha(manifest) and item["module"]["sha256"] == sha(module)
    assert item["module"]["path"] == args.module.name and item["provider_id"] == "org.qupathxx.svg-annotations"
    assert item["abi"] == 6 and not item["dependencies"]
    assert not args.output.exists() and not args.archive.exists()
    inv = ROOT / "tests/fixtures/manifest.json"
    original = inv.read_bytes()
    pkg = Package(json.loads(original), "Native SVG image-pixel vector annotation provider using admitted private Expat",
                  "svg_annotations_plugin")
    product = (
        *(f"plugins/svg_annotations/{name}" for name in (
            "CMakeLists.txt", "svg_annotations.cpp", "svg_geometry.hpp", "reference-lock.json")),
        *(f"plugins/format-common/{name}" for name in (
            "annotation_xml.hpp", "annotation_source.hpp", "binary_input.hpp", "reader_input.hpp",
            "annotation_xml_expat.cmake", "plugin_build.cmake", "format_plugin_profile.hpp.in", "write_manifest.cmake")),
    )
    parents = [pkg.add("source/" + name, (ROOT / name).read_bytes(), name) for name in product]
    for name in ("plugins/svg_annotations/tests/CMakeLists.txt", "plugins/svg_annotations/tests/project_probe.cpp",
                 "plugins/svg_annotations/tests/write_fixture.py", "tools/ci/check_svg_annotations.py",
                 "tools/ci/check_project_matrix_index.py"):
        pkg.add("source/" + name, (ROOT / name).read_bytes(), name)
    for name in ("include/qupathxx/format_plugin.h", "include/qupathxx/reader_plugin.h",
                 "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE"):
        parents.append(pkg.add("source/format-plugin-kit/" + name, (args.kit / name).read_bytes(), name,
                               kind="notice" if name.endswith("LICENSE") else "code"))
    expat = json.loads(args.expat_admission.read_bytes())
    assert expat["admission_exit"] == 0
    for key in sorted(expat["members"]):
        if key.startswith(("source/deps/expat/", "licenses/expat/")):
            pkg.borrow(key, args.expat_stage, expat)
            parents.append(pkg.members[key])
    assert pkg.files["licenses/expat/COPYING"] == (args.module.parent / "licenses/expat/COPYING").read_bytes()
    pkg.add("README.md", (ROOT / "plugins/svg_annotations/README.md").read_bytes(), "SVG guide", kind="asset")
    pkg.add("licenses/qupathxx/LICENSE", (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")
    binary = pkg.add(args.module.name, module, "Validated SVG annotation provider", parents)
    pkg.add("format-plugins.json", manifest, "Validated format manifest", [binary], "asset")
    metadata = {
        "providers": {item["provider_id"]: {"module_sha256": sha(module), "abi": item["abi"], "contract": item["contract"]}},
        "validation_sha256": sha(args.validation.read_bytes()), "validated_host_sha256": validated["host_sha256"],
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
    print(f"Admitted {len(pkg.members)} SVG package and ZIP members")


if __name__ == "__main__":
    main()
