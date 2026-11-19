"""Stage the WebP native module, exact offline sources and license notices.

Development packaging only. The existing material inventory/guard owns admission;
this script adds one scoped group and does not implement product source logic.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
VERSION = "1.2.0"
CONTRACT = "webp-static-uint8-samples/3"
PRODUCT = ("CMakeLists.txt", "webp_reader.cpp", "reader_plugin_profile.hpp.in", "write_manifest.cmake",
           "webp_memory_backend.c", "webp_memory_backend.h", "libwebp-source-lock.json")
OWN_FILES = tuple("plugins/webp/" + name for name in PRODUCT) + (
    "plugins/webp/README.md", "plugins/webp/tests/webp_plugin_tests.cpp",
    "plugins/webp/tests/webp_project_consumer.cpp", "tools/ci/check_webp_reader_plugin.py",
    "tools/ci/stage_webp_reader_plugin.py", "tools/ci/prepare_webp_reader_sources.py")
sha = lambda data: hashlib.sha256(data).hexdigest()


def update_group(path, value):
    """Replace one planning-independent material group; preserve other raw text."""
    text = path.read_text(encoding="utf8")
    encoded = json.dumps(value, indent=2, ensure_ascii=False).replace("\n", "\n  ")
    match = re.search(r'^  "webp_reader_plugin": ', text, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(text[start:])
        text = text[:start] + encoded + text[start + length:]
    else:
        end = text.rfind("}")
        text = text[:end].rstrip() + ',\n  "webp_reader_plugin": ' + encoded + "\n}\n"
    json.loads(text)
    path.write_text(text, encoding="utf8", newline="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("module", "validation", "compiled-inputs", "kit", "sources", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    validation = json.loads(args.validation.read_text(encoding="utf8"))
    module = args.module.read_bytes()
    if (not validation.get("passed") or not validation.get("artifacts_unchanged")
            or validation.get("module_sha256") != sha(module)):
        raise ValueError("Need passing validation bound to these unchanged WebP module bytes")
    compiled = {key.replace("\\", "/"): value for key, value in
                json.loads(args.compiled_inputs.read_text(encoding="utf8")).items()}
    product_hashes = {}
    for name in PRODUCT:
        key = "plugins/webp/" + name
        digest = sha((ROOT / key).read_bytes())
        if compiled.get(key) != digest:
            raise ValueError("Product input differs from compiled snapshot: " + key)
        product_hashes[key] = digest
    context = sha(json.dumps(product_hashes, sort_keys=True, separators=(",", ":")).encode())
    manifest_path = args.module.parent / "reader-plugins.json"
    manifest_bytes = manifest_path.read_bytes()
    entries = json.loads(manifest_bytes)["plugins"]
    if (len(entries) != 1 or entries[0]["provider_id"] != "org.qupathxx.webp"
            or entries[0]["module"]["sha256"] != sha(module)
            or entries[0]["version"] != VERSION or entries[0]["contract"] != CONTRACT
            or entries[0]["abi"] != 1 or entries[0]["dependencies"] != []
            or "options" in entries[0]
            or entries[0]["capabilities"] != {"display_rgba": True, "raw_samples": True}
            or validation.get("manifest_sha256") != sha(manifest_bytes)):
        raise ValueError("Manifest differs from the selected hash-bound WebP contract")
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Use a fresh staging directory; preserve existing evidence")
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf8"))
    group = inventory.get("webp_reader_plugin", {"scope": "Local Windows x64 MSVC1944 Release /MD WebP plugin/source handoff; no host, wheel or UI rebuild/publication", "members": [], "deliveries": {}})
    records = {row["material"]["id"]: row for row in group["members"]}
    files, expected = {}, {}

    def add(name, data, origin, parents=("qupathxx-root-license",), kind="code", license_id="AGPL-3.0-only", url=None):
        ident = "webp-plugin-" + re.sub(r"[^A-Za-z0-9-]+", "-", license_id) + "-" + sha(data)[:24]
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
            "material": {"id": ident, "category": "admitted_open", "kind": kind,
                         "license_id": license_id, "license_evidence": url or "LICENSE; repository author declaration",
                         "attribution": "Google Inc.; retained AUTHORS and per-file notices" if url else "qupathxx contributors", "source_ids": list(dict.fromkeys(parents))}}
        if url: record["url"] = url
        records.setdefault(ident, record)
        files[name] = data
        expected[name] = ident
        return ident

    product_parents = []
    for rel in OWN_FILES:
        ident = add("source/" + rel, (ROOT / rel).read_bytes(), rel, kind="asset" if rel.endswith(".md") else "code")
        if rel in product_hashes:
            product_parents.append(ident)
    receipt = "docs/benchmarks/webp-reader-plugin/README.md"
    if (ROOT / receipt).is_file():
        add("source/" + receipt, (ROOT / receipt).read_bytes(), receipt, kind="asset")
    kit_parents = []
    for rel in ("include/qupathxx/reader_plugin.h", "share/qupathxx/reader-plugin/profile.cmake", "share/qupathxx/reader-plugin/LICENSE"):
        kit_parents.append(add("source/reader-plugin-kit/" + rel, (args.kit / rel).read_bytes(),
                               "unchanged relocated ReaderPluginKit " + rel, kind="notice" if rel.endswith("LICENSE") else "code"))
    deps = []
    lock = json.loads((ROOT / "plugins/webp/libwebp-source-lock.json").read_text(encoding="utf8"))
    if lock["commit"] != "4fa21912338357f89e4fd51cf2368325b59e9bd9" or lock["license_id"] != "BSD-3-Clause":
        raise ValueError("Unexpected libwebp pin/terms")
    for rel, row in lock["members"].items():
        data = (args.sources / "libwebp" / rel).read_bytes()
        if sha(data) != row["sha256"] or len(data) != row["bytes"]:
            raise ValueError("Changed locked libwebp member: " + rel)
        ident = add("source/deps/libwebp/" + rel, data, "Exact unmodified libwebp1.6.0 source selection: " + rel,
                    parents=(), kind="notice" if rel in ("COPYING", "PATENTS", "AUTHORS") else "code",
                    license_id="BSD-3-Clause", url=lock["members"]["COPYING"]["url"])
        deps.append(ident)
        records[ident]["url"] = row["url"]
        if rel in ("COPYING", "PATENTS", "AUTHORS"):
            add("licenses/libwebp/" + rel, data, "Full libwebp notice: " + rel, parents=(), kind="notice",
                license_id="BSD-3-Clause", url=lock["members"]["COPYING"]["url"])
    for path in ("source/LICENSE", "licenses/qupathxx/LICENSE"):
        add(path, (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")
    binary = add("qpx_webp_plugin.dll", module, "Selected WebP adapter/private static libwebp decoder; platform runtime only",
                 tuple(dict.fromkeys(product_parents + kit_parents + deps)))
    add("reader-plugins.json", manifest_bytes, "Hash-bound WebP runtime manifest", (binary,), kind="asset")
    guide = ("# Native WebP plugin package\n\n"
             f"Provider `org.qupathxx.webp` version {VERSION}, contract `{CONTRACT}`.\n\n"
             "See the [complete profile/build/use guide](source/plugins/webp/README.md) "
             "and [delivery receipt](source/docs/benchmarks/webp-reader-plugin/README.md).\n\n"
             "Keep the DLL, reader-plugins.json and license together. Configure the absolute manifest "
             "with --reader-plugins and select --reader org.qupathxx.webp. Pass empty native options. "
             "The profile reads still VP8L and VP8, including selected alpha layouts and bounded EXIF orientation. "
             "Display and raw regions share the same pixel coordinates.\n\n"
             "source/plugins/webp, source/reader-plugin-kit and source/deps/libwebp supply the complete offline native build inputs. "
             "Tests default to OFF. "
             "See SOURCE.json and NOTICES.txt. Windows x64 MSVC1944/compiler19.44.35225.0, Release /MD.\n\n"
             "Close applications using the module before removing package-owned files; preserve user data.\n")
    add("README.md", guide.encode(), "Generated package-relative WebP guide", kind="asset")
    notice = ("WebP reader and authored sources/fixtures: qupathxx contributors, AGPL-3.0-only.\n"
              "Full text: licenses/qupathxx/LICENSE; exact corresponding source: source/.\n"
              "Private static libwebp1.6.0: BSD-3-Clause, full COPYING/PATENTS/AUTHORS in licenses/libwebp.\n"
              "Platform CRT requirements follow the recorded kit profile.\n"
              "No upstream test images, encoder, mux/demux or restricted materials are included.\n")
    add("NOTICES.txt", notice.encode(), "Generated WebP component notice", kind="notice")
    provenance = {"schema": "qpx.webp-reader-package/1", "provider_id": "org.qupathxx.webp", "version": VERSION,
                  "contract": CONTRACT, "profile": entries[0]["profile"], "module_sha256": sha(module),
                  "product_source_sha256": context, "product_inputs": product_hashes,
                  "validation_sha256": sha(args.validation.read_bytes()), "components": {"adapter": "AGPL-3.0-only", "libwebp": "BSD-3-Clause; PATENTS/AUTHORS retained"},
                  "codec_commit": lock["commit"],
                  "source_build": "source/plugins/webp; default tests OFF; source/reader-plugin-kit; private static libwebp; no core/Python"}
    add("SOURCE.json", (json.dumps(provenance, indent=2) + "\n").encode(), "Generated exact WebP build provenance", (binary,), kind="asset")
    group["members"] = list(records.values())
    group["deliveries"][sha(module)] = {"product_source_sha256": context, "validation_sha256": provenance["validation_sha256"], "members": expected}
    if args.write_inventory:
        update_group(inventory_path, group)
    elif any(ident not in {row["material"]["id"] for row in inventory.get("webp_reader_plugin", {}).get("members", [])} for ident in expected.values()):
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
