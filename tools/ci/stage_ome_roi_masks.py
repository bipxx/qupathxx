"""Stage the native OME ROI mask module with its exact corresponding sources."""
import argparse
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from check_material_staging import check_archive, inventory
from stage_annotation_sources import Package, ROOT, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("module", "validation", "kit", "previous", "codec-package", "output", "archive"):
        parser.add_argument("--" + key, type=Path, required=True)
    args = parser.parse_args()
    receipt_path = args.previous.with_name(args.previous.name + "-admission.json")
    prior = json.loads(receipt_path.read_bytes())
    assert prior["admission_exit"] == 0
    module = args.module.read_bytes()
    manifest = args.module.with_name("format-plugins.json").read_bytes()
    entry, = json.loads(manifest)["plugins"]
    validated = json.loads(args.validation.read_bytes())
    assert validated["passed"] and validated["manifest_sha256"] == sha(manifest)
    assert validated["module_sha256"] == entry["module"]["sha256"] == sha(module)
    assert entry["version"] == "1.3.0" and entry["abi"] == 5 and not entry["dependencies"]
    expected = {"mixed", "translated", "zlib", "bzip2", "grid-identity", "grid-flip-x", "grid-flip-y",
                "grid-rotate-180", "grid-transpose", "grid-rotate-90", "grid-rotate-270", "grid-antitranspose"}
    assert set(validated["imports"]) == set(validated["reopens"]) == expected
    assert len(validated["refusals"]) == 35 and len(validated["vector_regressions"]) == 5
    assert not args.output.exists() and not args.archive.exists()
    inv = ROOT / "tests/fixtures/manifest.json"
    original = inv.read_bytes()
    pkg = Package(json.loads(original), "Native OME 2016-06 vector and bit-mask provider", "ome_roi_plugin")
    parents = []
    for name in prior["members"]:
        if name.startswith(("source/deps/expat/", "licenses/expat/")) or name in (
            "source/references/ome-2016-06.xsd", "licenses/qupathxx/LICENSE", "source/LICENSE",
        ):
            pkg.borrow(name, args.previous, prior)
            parents.append(pkg.members[name])
    dependency_receipt = args.codec_package.with_name(args.codec_package.name + "-admission.json")
    admitted_codecs = json.loads(dependency_receipt.read_bytes())
    assert admitted_codecs["admission_exit"] == 0
    for name in ("source/third_party/zlib-1.3.2.tar.gz", "licenses/zlib/LICENSE",
                 "source/third_party/bzip2-1.0.8.tar.gz", "licenses/bzip2/LICENSE"):
        pkg.borrow(name, args.codec_package, admitted_codecs)
        parents.append(pkg.members[name])
    product = [
        *(f"plugins/ome_roi/{name}" for name in (
            "CMakeLists.txt", "MaskCodecs.cmake", "mask.hpp", "mask_compression.hpp", "ome_roi.cpp",
            "options.hpp", "xml.hpp", "reference-lock.json")),
        *(f"plugins/format-common/{name}" for name in (
            "annotation_source.hpp", "annotation_xml.hpp", "annotation_xml_expat.cmake", "binary_input.hpp",
            "reader_input.hpp", "plugin_build.cmake", "format_plugin_profile.hpp.in", "write_manifest.cmake")),
    ]
    for name in product:
        parents.append(pkg.add("source/" + name, (ROOT / name).read_bytes(), name))
    for file in (
        "include/qupathxx/format_plugin.h", "include/qupathxx/reader_plugin.h",
        "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE",
    ):
        parents.append(pkg.add("source/format-plugin-kit/" + file, (args.kit / file).read_bytes(), file,
                               kind="notice" if file.endswith("LICENSE") else "code"))
    for name in ("plugins/ome_roi/tests/CMakeLists.txt", "plugins/ome_roi/tests/project_probe.cpp",
                 "tools/ci/check_ome_roi_masks.py", "tools/ci/stage_ome_roi_masks.py"):
        pkg.add("source/" + name, (ROOT / name).read_bytes(), name)
    pkg.add("README.md", (ROOT / "plugins/ome_roi/README.md").read_bytes(), "OME ROI usage", kind="asset")
    pkg.add("NOTICE.txt", b"qupathxx module and tools: AGPL-3.0-only. Expat 2.10.0: MIT, with SipHash CC0-1.0. zlib 1.3.2: Zlib. libbzip2 1.0.8: bzip2-1.0.6. This work is derived in part from the OME specification. Copyright (C) 2002-2016 Open Microscopy Environment. The unmodified OME schema is CC BY 4.0, https://creativecommons.org/licenses/by/4.0/. Matching source and original notices are included.\n",
            "Per-component notices", kind="notice")
    binary = pkg.add(args.module.name, module, "Verified native OME mask module", parents, "derived_asset")
    pkg.add("format-plugins.json", manifest, "Verified native format manifest", (binary,), "asset")
    metadata = {"schema": "qpx.ome-roi-mask-delivery/1", "providers": {entry["provider_id"]: entry},
                "validation_sha256": sha(args.validation.read_bytes()),
                "validated_host_sha256": validated["host_sha256"],
                "source_sha256": {name: sha(value) for name, value in pkg.files.items()
                                  if name.startswith("source/")}}
    try:
        pkg.finish(args.output, sha(module), metadata, inv, True)
        with ZipFile(args.archive, "x", ZIP_DEFLATED) as archive:
            for name in sorted(pkg.members):
                archive.write(args.output / name, name)
        check_archive(args.archive, inventory([inv]), expected=pkg.members)
    except Exception:
        inv.write_bytes(original)
        raise
    print(f"Admitted {len(pkg.members)} OME ROI package and ZIP members")


if __name__ == "__main__":
    main()
