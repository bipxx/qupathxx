"""Package a verified CSV or TSV matrix module with its exact native source."""
import argparse
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

from check_material_staging import check_archive, inventory
from stage_annotation_sources import Package, ROOT, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("install", "validation", "kit", "output", "archive"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    assert not args.output.exists() and not args.archive.exists()
    manifest = (args.install / "format-plugins.json").read_bytes()
    item, = json.loads(manifest)["plugins"]
    module = item["module"]["path"]
    binary = (args.install / module).read_bytes()
    evidence = args.validation.read_bytes()
    verified = json.loads(evidence)
    provider = item["provider_id"]
    profiles = {"org.qupathxx.csv-matrix": "csv", "org.qupathxx.tsv-matrix": "tsv"}
    kind = profiles[provider]
    folder = kind + "_matrix"
    assert item["domain"] == "matrix" and item["abi"] == 2
    assert not item["dependencies"] and verified["passed"]
    assert verified["module_sha256"] == sha(binary) == item["module"]["sha256"]
    assert verified["manifest_sha256"] == sha(manifest)

    ledger = ROOT / "tests/fixtures/manifest.json"
    original = ledger.read_bytes()
    package = Package(json.loads(original), f"Optional native {kind.upper()} expression matrices", folder + "_plugin")
    compiled = [f"plugins/{folder}/{name}" for name in ("CMakeLists.txt", folder + ".cpp")]
    compiled += ["plugins/format-common/" + name for name in (
        "binary_input.hpp", "reader_input.hpp", "matrix_table.hpp", "json_tree.hpp", "delimited_matrix.hpp",
        "plugin_build.cmake", "format_plugin_profile.hpp.in", "write_manifest.cmake")]
    parents = [package.add("source/" + path, (ROOT / path).read_bytes(), path) for path in compiled]
    for name in ("include/qupathxx/format_plugin.h", "include/qupathxx/reader_plugin.h",
                 "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE"):
        parents.append(package.add("source/format-plugin-kit/" + name, (args.kit / name).read_bytes(), name,
                                   kind="notice" if name.endswith("/LICENSE") else "code"))
    license_text = (ROOT / "LICENSE").read_bytes()
    assert (args.install / "licenses/qupathxx/LICENSE").read_bytes() == license_text
    package.add("licenses/qupathxx/LICENSE", license_text, "LICENSE", kind="notice")
    for path in ("tools/ci/check_csv_matrix.py", "plugins/arrow_matrix/tests/project_probe.cpp",
                 "plugins/arrow_matrix/tests/CMakeLists.txt"):
        package.add("source/" + path, (ROOT / path).read_bytes(), path)
    readme = f"plugins/{folder}/README.md"
    package.add("README.md", (ROOT / readme).read_bytes(), readme, kind="asset")
    binary_id = package.add(module, binary, f"Exact validated {kind.upper()} matrix module", parents)
    package.add("format-plugins.json", manifest, "Installed provider manifest", (binary_id,), "asset")
    source = {"providers": {provider: item}, "validated_host_sha256": verified["host_sha256"],
              "validation_sha256": sha(evidence),
              "source_files": {p: sha(data) for p, data in package.files.items() if p.startswith("source/")}}
    try:
        package.finish(args.output, sha(binary), source, ledger, True)
        with ZipFile(args.archive, "x", compression=ZIP_DEFLATED) as archive:
            for name in sorted(package.files):
                archive.write(args.output / name, name)
        check_archive(args.archive, inventory([ledger]), expected=package.members)
    except Exception:
        ledger.write_bytes(original)
        raise
    print(f"Admitted {len(package.files)} {kind.upper()} matrix package and ZIP members")


if __name__ == "__main__":
    main()
