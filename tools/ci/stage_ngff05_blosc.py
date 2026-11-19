# SPDX-License-Identifier: AGPL-3.0-only
"""Stage the NGFF 0.5 Blosc module with its exact offline build inputs."""

import argparse
import json
from pathlib import Path
import zipfile

from check_material_staging import check_archive, inventory
from check_ngff05 import FLEXIBLE_INDEX_CONTRACT
from stage_ngff05 import (INVENTORY, LABELS, PACKET, PROVIDER, ImagePackage, compiled_inputs)
from stage_ngff_labels import ROOT, encoded, file_sha, load


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("runtime", "output", "zip"):
        parser.add_argument(f"--{key}", type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    assert not args.output.exists() and not args.zip.exists()
    runtime = args.runtime
    previous = PACKET / "package-blosc-all-01"
    approved = load(previous.with_name("package-blosc-all-01-admission.json"))
    assert approved["admission_exit"] == 0
    package = ImagePackage(load(INVENTORY), "NGFF 0.5 Windows module with admitted Blosc", "ngff05_image_plugin")
    availability = load(previous / "FROZEN-SOURCE-AVAILABILITY.json")
    for row in [availability["core"], availability["first_party_source"]]:
        assert file_sha(Path(row["path"])) == row["sha256"]
    for row in availability["upstream_sources"]:
        assert file_sha(Path(row["local_path"])) == row["sha256"]
    for name in approved["members"]:
        if name.startswith(("licenses/", "source/frozen-sdk-interface/")) or name in (
            "bin/libopenslide-1.dll", "FROZEN-SOURCE-AVAILABILITY.json", "source/LICENSE",
            "source/third_party/zstd-1.5.7.tar.gz", "source/reader-plugin-kit/share/qupathxx/reader-plugin/profile.cmake",
            "source/reader-plugin-kit/share/qupathxx/reader-plugin/LICENSE",
            "source/third_party/c-blosc-v1.21.6.tar.gz", "source/plugins/blosc-bytes/THIRD_PARTY_NOTICES.txt",
            "source/third_party/snappy-1.2.2.tar.gz",
        ):
            package.borrow(name, previous, approved)
    assert file_sha(runtime / "libopenslide-1.dll") == approved["members"]["bin/libopenslide-1.dll"]["sha256"]

    vendor = ROOT / "plugins/blosc-bytes"
    lock = load(vendor / "source-lock.json")
    source = "source/third_party/c-blosc-v1.21.6.tar.gz"
    assert approved["members"][source]["sha256"] == lock["archive_sha256"]
    snappy_name = "snappy-1.2.2.tar.gz"
    snappy_lock = load(ROOT / "plugins/parquet_matrix/decoder-sources.json")[snappy_name]
    snappy_source = "source/third_party/" + snappy_name
    assert approved["members"][snappy_source]["sha256"] == snappy_lock["sha256"]
    parents, compile_receipt = compiled_inputs(package, load(LABELS / "package-final-admission.json"),
                                               module_only=True, blosc_sources=True,
                                               snappy_archive=previous / snappy_source)
    module = runtime / "qpx_ngff05_plugin.dll"
    manifest = runtime / "ngff05-reader-plugins.json"
    entry, = load(manifest)["plugins"]
    assert (entry["provider_id"], entry["version"], entry["contract"]) == (PROVIDER, "1.17.0", FLEXIBLE_INDEX_CONTRACT)
    binary_sha = file_sha(module)
    assert binary_sha == entry["module"]["sha256"] == file_sha(PACKET / "cache/Release" / module.name)
    host_sha = file_sha(runtime / "qupathxx_region.exe")
    validation = {}
    for label, count, kind in (
        ("v117-index-start-check-v117-02", 13, "checks"),
        ("v117-index-end-check-v117", 12, "checks"),
        ("v117-index-end-crc-check-v117", 13, "checks"),
        ("sharded-producer-01-check-v117", 13, "checks"),
        ("v117-legacy-shards", 46, "commands"),
    ):
        path = PACKET / label / "results.json"
        result = load(path)
        assert result["passed"] and len(result[kind]) == count
        assert result["module_sha256"] == binary_sha and result["region_sha256"] == host_sha
        assert result["manifest_sha256"] == file_sha(manifest)
        validation[label] = file_sha(path)
    frames = PACKET / "v117-frame-regression/results.json"
    frame_check = load(frames)
    assert frame_check["passed"] and len(frame_check["checks"]) == 19
    assert frame_check["manifest_sha256"] == file_sha(manifest)
    validation["frames"] = file_sha(frames)
    parents.extend([package.members[source], package.members[snappy_source],
                    package.members["source/third_party/zstd-1.5.7.tar.gz"],
                    package.members["bin/libopenslide-1.dll"], package.members["FROZEN-SOURCE-AVAILABILITY.json"]])
    binary = package.add("bin/" + module.name, module.read_bytes(), "Validated NGFF 0.5 Blosc DLL",
                         parents, "derived_asset")
    package.add("bin/" + manifest.name, manifest.read_bytes(), "Exact native module manifest", (binary,), "asset")
    package.add("README.md", (ROOT / "plugins/ngff05/README.md").read_bytes(), "Install and rebuild guide", kind="asset")
    package.add("NOTICE.txt", b"qupathxx: AGPL-3.0-only. Zstandard and Snappy: BSD-3-Clause. See licenses/blosc/THIRD_PARTY_NOTICES.txt for C-Blosc and LZ4 terms. Preserve the SDK and OpenSlide distribution and LGPL relinking terms in the retained notices.\n",
                "Component notices", kind="notice")
    metadata = {"schema": "qpx.ngff05-blosc-delivery/1", "providers": {PROVIDER: entry},
                "region_sha256": host_sha, "module_only": True, "compiled": compile_receipt,
                "validation_sha256": validation, "blosc_source_sha256": lock["archive_sha256"],
                "snappy_source_sha256": snappy_lock["sha256"],
                "blosc_library_sha256": file_sha(PACKET / "cache/Release/ngff05_blosc.lib")}
    result = package.finish(args.output, binary_sha, metadata, INVENTORY, args.write_inventory)
    with zipfile.ZipFile(args.zip, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as destination:
        for name in sorted(package.members):
            destination.write(args.output / name, name)
    files = check_archive(args.zip, inventory([INVENTORY]), expected=package.members)
    assert files == result["files"]
    receipt = {"archive": args.zip.name, "sha256": file_sha(args.zip), "members": files}
    args.zip.with_suffix(".admission.json").write_bytes(encoded(receipt))
    print(json.dumps(receipt))


if __name__ == "__main__":
    main()
