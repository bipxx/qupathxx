"""Stage a verified native Parquet plugin and its pinned source dependencies."""
import argparse
import json
from pathlib import Path
import tarfile
from zipfile import ZIP_DEFLATED, ZipFile

from check_material_staging import check_archive, inventory
from stage_annotation_sources import COMMON, Package, ROOT, sha
from stage_arrow_points import ZSTD_NOTICE_ID, ZSTD_SOURCE_ID

PROVIDERS = {
    "org.qupathxx.parquet-matrix": ("parquet_matrix", 2, "matrix", "Parquet expression matrix",
                                  "matrix_table.hpp", "check_arrow_matrix.py", "arrow_matrix"),
    "org.qupathxx.parquet-points": ("parquet_points", 3, "annotation", "Parquet image-pixel points",
                                  "point_columns.hpp", "check_parquet_points.py", "arrow_points"),
    "org.qupathxx.geoparquet": ("geoparquet", 3, "annotation", "GeoParquet image-pixel annotations",
                               "wkb_geometry.hpp", "check_geoparquet.py", "geoparquet"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("install", "validation", "kit", "archives", "decoder-config", "output", "archive"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    inv = ROOT / "tests/fixtures/manifest.json"
    original = inv.read_bytes()
    manifest = (args.install / "format-plugins.json").read_bytes()
    item, = json.loads(manifest)["plugins"]
    provider = item["provider_id"]
    folder, abi, domain, description, shared, harness, probe = PROVIDERS[provider]
    pkg = Package(json.loads(original), description, folder + "_plugin")
    validation = json.loads(args.validation.read_bytes())
    binary = (args.install / item["module"]["path"]).read_bytes()
    assert item["abi"] == abi and item["domain"] == domain and not item["dependencies"]
    assert validation["passed"] and validation["module_sha256"] == sha(binary) == item["module"]["sha256"]
    assert validation["manifest_sha256"] == sha(manifest)
    assert not args.output.exists() and not args.archive.exists()

    plugin = f"plugins/{folder}/"
    lock = "plugins/parquet_matrix/decoder-sources.json"
    pins = json.loads((ROOT / lock).read_bytes())
    parents = []
    provider_sources = (plugin + "geoarrow_geometry.hpp", "cpp/include/qupathxx/detail/affine_numeric.hpp"
                        ) if folder == "geoparquet" else ()
    for name in (*COMMON, "plugins/format-common/json_tree.hpp", "plugins/format-common/parquet_file.hpp",
                 f"plugins/format-common/{shared}", plugin + "CMakeLists.txt", plugin + folder + ".cpp", lock,
                 "tools/ci/build_parquet_decoder.ps1", *provider_sources):
        parents.append(pkg.add("source/" + name, (ROOT / name).read_bytes(), name))
    for name in ("tools/ci/" + harness, f"plugins/{probe}/tests/project_probe.cpp",
                 f"plugins/{probe}/tests/CMakeLists.txt"):
        pkg.add("source/" + name, (ROOT / name).read_bytes(), name)
    for name in ("include/qupathxx/format_plugin.h", "include/qupathxx/reader_plugin.h",
                 "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE"):
        parents.append(pkg.add("source/format-plugin-kit/" + name, (args.kit / name).read_bytes(), name,
                               kind="notice" if name.endswith("LICENSE") else "code"))

    def external(name, data, pin, kind, source_ids=(), reuse=None):
        ident = reuse or f"parquet-{pin['license']}-{kind}-{sha(data)[:26]}"
        if ident in pkg.known_records:
            record = pkg.known_records[ident]
            assert sha(data) == record["sha256"] and len(data) == record["bytes"]
        else:
            pkg.records[ident] = {
                "filename": Path(name).name, "source": pin["url"], "bytes": len(data), "sha256": sha(data),
                "material": {
                    "id": ident, "category": "admitted_open", "kind": kind, "license_id": pin["license"],
                    "license_evidence": ", ".join(pin["notices"]),
                    "attribution": "Upstream contributors identified in the bundled notices",
                    "source_ids": list(dict.fromkeys(source_ids)),
                },
            }
        pkg._member(name, data, ident)
        return ident

    for name, pin in pins.items():
        path = args.archives / name
        archive = path.read_bytes()
        assert sha(archive) == pin["sha256"], name
        is_zstd = name == "zstd-1.5.7.tar.gz"
        notices = []
        with tarfile.open(path) as tar:
            for filename in pin["notices"]:
                notice = tar.extractfile(filename).read()
                notices.append(external("licenses/" + name + "/" + filename, notice, pin, "notice",
                                        reuse=ZSTD_NOTICE_ID if is_zstd else None))
        parents.append(external("source/third_party/" + name, archive, pin, "source_archive", notices,
                                reuse=ZSTD_SOURCE_ID if is_zstd else None))

    pkg.add("README.md", (ROOT / (plugin + "README.md")).read_bytes(), description, kind="asset")
    pkg.add("licenses/qupathxx/LICENSE", (args.install / "licenses/qupathxx/LICENSE").read_bytes(),
            "FormatPluginKit LICENSE", kind="notice")
    pkg.add("decoder-config.json", args.decoder_config.read_bytes(), "Exact native decoder configuration", kind="asset")
    binary_id = pkg.add(item["module"]["path"], binary, "Verified native Parquet reader", parents)
    pkg.add("format-plugins.json", manifest, "Verified module manifest", [binary_id], "asset")
    metadata = {
        "providers": {provider: {"module_sha256": sha(binary), "abi": abi, "contract": item["contract"],
                                 "version": item["version"], "profile": item["profile"]}},
        "libraries": pins,
        "validation_sha256": sha(args.validation.read_bytes()),
        "validated_host_sha256": validation["host_sha256"],
        "source_files": {name: sha(data) for name, data in pkg.files.items() if name.startswith("source/")},
    }
    try:
        pkg.finish(args.output, sha(binary), metadata, inv, True)
        with ZipFile(args.archive, "x", ZIP_DEFLATED) as archive:
            for name in sorted(pkg.members):
                archive.write(args.output / name, name)
        check_archive(args.archive, inventory([inv]), expected=pkg.members)
    except Exception:
        inv.write_bytes(original)
        raise
    print(f"Admitted {len(pkg.members)} native Parquet delivery members")


if __name__ == "__main__":
    main()
