"""Stage the MRC2014 native module, exact offline sources and license notices.

Development packaging only. The existing material inventory/guard owns admission;
this script adds one scoped group and does not implement product source logic.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[2]
VERSION = "1.5.0"
CONTRACT = "mrc2014-scalar-window/3"
ZLIB_SHA256 = "bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
ZLIB_SOURCE = "source/third_party/zlib-1.3.2.tar.gz"
ZLIB_NOTICE = "licenses/zlib/LICENSE"
ZLIB_MATERIAL = "release-src-zlib"
BZIP2_SHA256 = "ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269"
BZIP2_SOURCE = "source/third_party/bzip2-1.0.8.tar.gz"
BZIP2_NOTICE = "licenses/bzip2/LICENSE"
BZIP2_MATERIAL = "nrrd-plugin-bzip2-1-0-6-ab5a03176ee106d3f0fa90e3"
BZIP2_NOTICE_MATERIAL = "nrrd-plugin-bzip2-1-0-6-c6dbbf828498be844a89eaa3"
PRODUCT = ("plugins/mrc/CMakeLists.txt", "plugins/mrc/mrc_reader.cpp",
    "plugins/image-common/rgba8_reader.hpp", "plugins/image-common/plugin_build.cmake",
    "plugins/image-common/reader_plugin_profile.hpp.in", "plugins/image-common/write_manifest.cmake",
    "plugins/image-common/gzip_member.hpp")
OWN_FILES = PRODUCT + ("plugins/mrc/README.md",
    "plugins/mrc/tests/mrc_plugin_tests.cpp", "plugins/mrc/tests/mrc_project_consumer.cpp",
    "tools/ci/check_mrc_reader_plugin.py",
    "tools/ci/stage_mrc_reader_plugin.py")
sha = lambda data: hashlib.sha256(data).hexdigest()


def update_group(path, value):
    """Replace one planning-independent material group; preserve other raw text."""
    text = path.read_text(encoding="utf8")
    encoded = json.dumps(value, indent=2, ensure_ascii=False).replace("\n", "\n  ")
    match = re.search(r'^  "mrc_reader_plugin": ', text, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(text[start:])
        text = text[:start] + encoded + text[start + length:]
    else:
        end = text.rfind("}")
        text = text[:end].rstrip() + ',\n  "mrc_reader_plugin": ' + encoded + "\n}\n"
    json.loads(text)
    path.write_text(text, encoding="utf8", newline="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("module", "validation", "compiled-inputs", "kit", "output", "zlib-archive", "bzip2-archive"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    validation = json.loads(args.validation.read_text(encoding="utf8"))
    module = args.module.read_bytes()
    if (not validation.get("passed") or not validation.get("artifacts_unchanged")
            or validation.get("module_sha256") != sha(module)):
        raise ValueError("Need passing validation bound to these unchanged MRC2014 module bytes")
    compiled = {key.replace("\\", "/"): value for key, value in
                json.loads(args.compiled_inputs.read_text(encoding="utf8")).items()}
    product_hashes = {}
    for name in PRODUCT:
        key = name
        digest = sha((ROOT / key).read_bytes())
        if compiled.get(key) != digest:
            raise ValueError("Product input differs from compiled snapshot: " + key)
        product_hashes[key] = digest
    codec = args.zlib_archive.read_bytes()
    if sha(codec) != ZLIB_SHA256 or compiled.get(ZLIB_SOURCE) != ZLIB_SHA256:
        raise ValueError("The bundled zlib source must match the admitted and compiled archive")
    product_hashes[ZLIB_SOURCE] = ZLIB_SHA256
    bzip2 = args.bzip2_archive.read_bytes()
    if sha(bzip2) != BZIP2_SHA256 or compiled.get(BZIP2_SOURCE) != BZIP2_SHA256:
        raise ValueError("The bundled libbzip2 source must match the admitted and compiled archive")
    product_hashes[BZIP2_SOURCE] = BZIP2_SHA256
    context = sha(json.dumps(product_hashes, sort_keys=True, separators=(",", ":")).encode())
    manifest_path = args.module.parent / "reader-plugins.json"
    manifest_bytes = manifest_path.read_bytes()
    entries = json.loads(manifest_bytes)["plugins"]
    if (len(entries) != 1 or entries[0]["provider_id"] != "org.qupathxx.mrc"
            or entries[0]["module"].get("path") != "qpx_mrc_plugin.dll"
            or entries[0]["module"]["sha256"] != sha(module)
            or entries[0].get("display_name") != "MRC2014"
            or entries[0]["version"] != VERSION or entries[0]["contract"] != CONTRACT
            or entries[0]["abi"] != 1 or entries[0]["dependencies"] != []
            or "options" in entries[0]
            or entries[0]["capabilities"] != {"display_rgba": True, "raw_samples": True}
            or validation.get("manifest_sha256") != sha(manifest_bytes)):
        raise ValueError("Manifest differs from the selected hash-bound MRC2014 contract")
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Use a fresh staging directory; preserve existing evidence")
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf8"))
    group = inventory.get("mrc_reader_plugin", {"scope": "Local Windows x64 MSVC1944 Release /MD MRC2014 plugin/source handoff; plugin-only package; separate native frontend evidence; no core/GUI/wheel rebuild/publication", "members": [], "deliveries": {}})
    records = {row["material"]["id"]: row for row in group["members"]}
    files, expected = {}, {}

    def add(name, data, origin, parents=("qupathxx-root-license",), kind="code",
            license_id="AGPL-3.0-only", evidence="LICENSE; repository author declaration",
            attribution="qupathxx contributors"):
        ident = "mrc-plugin-" + license_id.replace(".", "-") + "-" + sha(data)[:24]
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
            "material": {"id": ident, "category": "admitted_open", "kind": kind,
                         "license_id": license_id, "license_evidence": evidence,
                         "attribution": attribution, "source_ids": list(dict.fromkeys(parents))}}
        records.setdefault(ident, record)
        files[name] = data
        expected[name] = ident
        return ident

    product_parents = []
    for rel in OWN_FILES:
        ident = add("source/" + rel, (ROOT / rel).read_bytes(), rel, kind="asset" if rel.endswith(".md") else "code")
        if rel in product_hashes:
            product_parents.append(ident)
    receipt = "docs/benchmarks/mrc-reader-plugin/README.md"
    add("source/" + receipt, (ROOT / receipt).read_bytes(), receipt, kind="asset")
    kit_parents = []
    for rel in ("include/qupathxx/reader_plugin.h", "share/qupathxx/reader-plugin/profile.cmake", "share/qupathxx/reader-plugin/LICENSE"):
        kit_parents.append(add("source/reader-plugin-kit/" + rel, (args.kit / rel).read_bytes(),
                               "unchanged relocated ReaderPluginKit " + rel, kind="notice" if rel.endswith("LICENSE") else "code"))
    for path in ("source/LICENSE", "licenses/qupathxx/LICENSE"):
        add(path, (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")
    with tarfile.open(fileobj=io.BytesIO(codec), mode="r:gz") as archive:
        with archive.extractfile("zlib-1.3.2/LICENSE") as original_notice:
            codec_notice = add(ZLIB_NOTICE, original_notice.read(), "Unmodified zlib 1.3.2 LICENSE",
                               (ZLIB_MATERIAL,), kind="notice", license_id="Zlib",
                               evidence=ZLIB_NOTICE, attribution="Jean-loup Gailly and Mark Adler")
    files[ZLIB_SOURCE] = codec
    expected[ZLIB_SOURCE] = ZLIB_MATERIAL
    with tarfile.open(fileobj=io.BytesIO(bzip2), mode="r:gz") as archive:
        with archive.extractfile("bzip2-1.0.8/LICENSE") as original_notice:
            files[BZIP2_NOTICE] = original_notice.read()
    files[BZIP2_SOURCE] = bzip2
    expected[BZIP2_SOURCE] = BZIP2_MATERIAL
    expected[BZIP2_NOTICE] = BZIP2_NOTICE_MATERIAL
    binary = add("qpx_mrc_plugin.dll", module, "Independent MRC2014 module with private static zlib and libbzip2",
                 tuple(dict.fromkeys(product_parents + kit_parents +
                                    [ZLIB_MATERIAL, codec_notice, BZIP2_MATERIAL, BZIP2_NOTICE_MATERIAL])))
    add("reader-plugins.json", manifest_bytes, "Hash-bound MRC2014 runtime manifest", (binary,), kind="asset")
    guide = ("# Native MRC2014 plugin package\n\n"
             f"Provider `org.qupathxx.mrc` version {VERSION}, contract `{CONTRACT}`.\n\n"
             "See the [complete profile/build/use guide](source/plugins/mrc/README.md) "
             "and [delivery receipt](source/docs/benchmarks/mrc-reader-plugin/README.md).\n\n"
             "Keep the DLL, reader-plugins.json and license together. Configure the absolute manifest "
             "with --reader-plugins and select --reader org.qupathxx.mrc. The raw reader preserves typed stored "
             "samples in declared X/Y/Z index order, with calibration converted from angstroms to micrometers. "
             "Z0 display uses the selected window. A .gz or .bz2 file may wrap the MRC in one complete stream.\n\n"
             "The offline build uses source/plugins/mrc, source/plugins/image-common and source/reader-plugin-kit. "
             f"Set QUPATHXX_MRC_ZLIB_ARCHIVE to {ZLIB_SOURCE} and QUPATHXX_MRC_BZIP2_ARCHIVE to {BZIP2_SOURCE}. "
             "Default tests OFF. Host/core and Python are unnecessary for the module. "
             "See SOURCE.json and NOTICES.txt. Windows x64 MSVC1944/compiler19.44.35225.0, Release /MD.\n\n"
             "Close applications using the module before removing package-owned files; preserve user data.\n")
    add("README.md", guide.encode(), "Generated package-relative MRC2014 guide", kind="asset")
    notice = ("MRC2014 reader and authored sources/fixtures: qupathxx contributors, AGPL-3.0-only.\n"
              "Full text: licenses/qupathxx/LICENSE; exact corresponding source: source/.\n"
              f"The module statically links unmodified zlib 1.3.2 under the Zlib license. Source: {ZLIB_SOURCE}. Notice: {ZLIB_NOTICE}.\n"
              f"The module also statically links unmodified libbzip2 1.0.8 under bzip2-1.0.6. Source: {BZIP2_SOURCE}. Notice: {BZIP2_NOTICE}.\n"
              "Platform CRT requirements follow the recorded kit profile.\n"
              "The published MRC2014 specification describes the file format. The separately pinned BSD-3-Clause "
              "mrcfile 1.5.4 Python package serves as a development oracle. The package contains independently "
              "authored implementation and test sources.\n")
    add("NOTICES.txt", notice.encode(), "Generated MRC2014 component notice", kind="notice")
    provenance = {"schema": "qpx.mrc-reader-package/1", "provider_id": "org.qupathxx.mrc", "version": VERSION,
                  "contract": CONTRACT, "profile": entries[0]["profile"], "module_sha256": sha(module),
                  "product_source_sha256": context, "product_inputs": product_hashes,
                  "validation_sha256": sha(args.validation.read_bytes()),
                  "components": {"adapter": "AGPL-3.0-only", "zlib": {"version": "1.3.2", "license": "Zlib",
                                  "source": ZLIB_SOURCE, "sha256": ZLIB_SHA256, "notice": ZLIB_NOTICE},
                                 "bzip2": {"version": "1.0.8", "license": "bzip2-1.0.6", "source": BZIP2_SOURCE,
                                           "sha256": BZIP2_SHA256, "notice": BZIP2_NOTICE}},
                  "source_build": "source/plugins/mrc plus source/plugins/image-common; default tests OFF; source/reader-plugin-kit; set QUPATHXX_MRC_ZLIB_ARCHIVE and QUPATHXX_MRC_BZIP2_ARCHIVE"}
    add("SOURCE.json", (json.dumps(provenance, indent=2) + "\n").encode(), "Generated exact MRC2014 build provenance", (binary,), kind="asset")
    group["members"] = list(records.values())
    group["deliveries"][sha(module)] = {"product_source_sha256": context, "validation_sha256": provenance["validation_sha256"], "members": expected}
    if args.write_inventory:
        update_group(inventory_path, group)
    elif any(ident not in {row["material"]["id"] for row in inventory.get("mrc_reader_plugin", {}).get("members", [])}
             for ident in expected.values()
             if ident not in {ZLIB_MATERIAL, BZIP2_MATERIAL, BZIP2_NOTICE_MATERIAL}):
        raise ValueError("Exact package members need a scoped inventory update")
    for name, data in files.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    command = [sys.executable, str(ROOT / "tools/ci/check_material_staging.py"), "--stage", str(output), "--inventory", str(inventory_path)]
    checked = subprocess.run(command, capture_output=True, text=True, encoding="utf8")
    result = {"module_sha256": sha(module), "product_source_sha256": context, "files": len(files),
              "members": {name: {"sha256": sha(data), "bytes": len(data), "material_id": expected[name]} for name, data in files.items()},
              "admission_exit": checked.returncode, "admission_output": checked.stdout + checked.stderr}
    (output.parent / (output.name + "-admission.json")).write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    if checked.returncode:
        raise ValueError(result["admission_output"])
    print(json.dumps({"admitted": True, "files": len(files), "module_sha256": sha(module), "output": str(output)}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(str(error))
