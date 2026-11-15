"""Stage the ICS native module, exact offline sources and license notices.

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
PRODUCT = ("CMakeLists.txt", "ics_reader.cpp", "reader_plugin_profile.hpp.in", "write_manifest.cmake")
OWN_FILES = tuple("plugins/ics/" + name for name in PRODUCT) + (
    "plugins/ics/README.md", "plugins/ics/tests/ics_plugin_tests.cpp",
    "plugins/ics/tests/ics_project_consumer.cpp", "plugins/ics/tests/ics_reference_dump.cpp",
    "plugins/ics/tests/libics-reference-lock.json", "tools/ci/check_ics_reader_plugin.py",
    "tools/ci/stage_ics_reader_plugin.py")
sha = lambda data: hashlib.sha256(data).hexdigest()


def update_group(path, value):
    """Replace one planning-independent material group; preserve other raw text."""
    text = path.read_text(encoding="utf8")
    encoded = json.dumps(value, indent=2, ensure_ascii=False).replace("\n", "\n  ")
    match = re.search(r'^  "ics_reader_plugin": ', text, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(text[start:])
        text = text[:start] + encoded + text[start + length:]
    else:
        end = text.rfind("}")
        text = text[:end].rstrip() + ',\n  "ics_reader_plugin": ' + encoded + "\n}\n"
    json.loads(text)
    path.write_text(text, encoding="utf8", newline="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("module", "validation", "compiled-inputs", "kit", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    validation = json.loads(args.validation.read_text(encoding="utf8"))
    module = args.module.read_bytes()
    if (not validation.get("passed") or not validation.get("artifacts_unchanged")
            or validation.get("module_sha256") != sha(module)):
        raise ValueError("Need passing validation bound to these unchanged ICS module bytes")
    compiled = {key.replace("\\", "/"): value for key, value in
                json.loads(args.compiled_inputs.read_text(encoding="utf8")).items()}
    product_hashes = {}
    for name in PRODUCT:
        key = "plugins/ics/" + name
        digest = sha((ROOT / key).read_bytes())
        if compiled.get(key) != digest:
            raise ValueError("Product input differs from compiled snapshot: " + key)
        product_hashes[key] = digest
    context = sha(json.dumps(product_hashes, sort_keys=True, separators=(",", ":")).encode())
    manifest_path = args.module.parent / "reader-plugins.json"
    manifest_bytes = manifest_path.read_bytes()
    entries = json.loads(manifest_bytes)["plugins"]
    if (len(entries) != 1 or entries[0]["provider_id"] != "org.qupathxx.ics"
            or entries[0]["module"]["sha256"] != sha(module)
            or entries[0]["version"] != "1.0.0" or entries[0]["contract"] != "ics-raw-channel-window/1"
            or entries[0]["abi"] != 1 or entries[0]["dependencies"] != []
            or entries[0].get("options") != {"channel": 0, "window": [0, 255]}
            or entries[0]["capabilities"] != {"display_rgba": True, "raw_samples": True, "source_files": True}
            or validation.get("manifest_sha256") != sha(manifest_bytes)):
        raise ValueError("Manifest differs from the selected hash-bound ICS contract")
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Use a fresh staging directory; preserve existing evidence")
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf8"))
    group = inventory.get("ics_reader_plugin", {"scope": "Local Windows x64 MSVC1944 Release /MD ICS plugin/source handoff; no host, wheel or UI rebuild/publication", "members": [], "deliveries": {}})
    records = {row["material"]["id"]: row for row in group["members"]}
    files, expected = {}, {}

    def add(name, data, origin, parents=("qupathxx-root-license",), kind="code"):
        ident = "ics-plugin-AGPL-3-0-only-" + sha(data)[:24]
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
            "material": {"id": ident, "category": "admitted_open", "kind": kind,
                         "license_id": "AGPL-3.0-only", "license_evidence": "LICENSE; repository author declaration",
                         "attribution": "qupathxx contributors", "source_ids": list(dict.fromkeys(parents))}}
        records.setdefault(ident, record)
        files[name] = data
        expected[name] = ident
        return ident

    product_parents = []
    for rel in OWN_FILES:
        ident = add("source/" + rel, (ROOT / rel).read_bytes(), rel, kind="asset" if rel.endswith(".md") else "code")
        if rel in product_hashes:
            product_parents.append(ident)
    receipt = "docs/benchmarks/ics-reader-plugin/README.md"
    if (ROOT / receipt).is_file():
        add("source/" + receipt, (ROOT / receipt).read_bytes(), receipt, kind="asset")
    kit_parents = []
    for rel in ("include/qupathxx/reader_plugin.h", "share/qupathxx/reader-plugin/profile.cmake", "share/qupathxx/reader-plugin/LICENSE"):
        kit_parents.append(add("source/reader-plugin-kit/" + rel, (args.kit / rel).read_bytes(),
                               "unchanged relocated ReaderPluginKit " + rel, kind="notice" if rel.endswith("LICENSE") else "code"))
    for path in ("source/LICENSE", "licenses/qupathxx/LICENSE"):
        add(path, (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")
    binary = add("qpx_ics_plugin.dll", module, "Selected independent ICS module; platform runtime only",
                 tuple(dict.fromkeys(product_parents + kit_parents)))
    add("reader-plugins.json", manifest_bytes, "Hash-bound ICS runtime manifest", (binary,), kind="asset")
    guide = ("# Native ICS plugin package\n\n"
             "Provider `org.qupathxx.ics` version1.0.0, contract `ics-raw-channel-window/1`.\n\n"
             "See the [complete profile/build/use guide](source/plugins/ics/README.md) "
             "and [delivery receipt](source/docs/benchmarks/ics-reader-plugin/README.md).\n\n"
             "Keep the DLL, reader-plugins.json and license together. Configure the absolute manifest "
             "with --reader-plugins and select --reader org.qupathxx.ics. Empty options select channel0 and display window0..255; "
             "the manifest makes that default explicit. Raw scalar samples stay exact.\n\n"
             "source/plugins/ics plus source/reader-plugin-kit are complete offline native build inputs. "
             "Default tests OFF; no external codec, host/core or Python is needed for the module. "
             "See SOURCE.json and NOTICES.txt. Windows x64 MSVC1944/compiler19.44.35225.0, Release /MD.\n\n"
             "Close applications using the module before removing package-owned files; preserve user data.\n")
    add("README.md", guide.encode(), "Generated package-relative ICS guide", kind="asset")
    notice = ("ICS reader and authored sources/fixtures: qupathxx contributors, AGPL-3.0-only.\n"
              "Full text: licenses/qupathxx/LICENSE; exact corresponding source: source/.\n"
              "This module has no external codec dependency; platform CRT requirements follow the recorded kit profile.\n"
              "Published ICS definitions are references, not relicensed or packaged third-party implementation.\n"
              "Optional native libics1.7.2 reference tests use the separate pinned Library-GPL2-or-later "
              "sources and bundled Lesser-GPL2.1 notice; that library is not linked or bundled here.\n")
    add("NOTICES.txt", notice.encode(), "Generated ICS component notice", kind="notice")
    provenance = {"schema": "qpx.ics-reader-package/1", "provider_id": "org.qupathxx.ics", "version": "1.0.0",
                  "contract": "ics-raw-channel-window/1", "profile": entries[0]["profile"], "module_sha256": sha(module),
                  "product_source_sha256": context, "product_inputs": product_hashes,
                  "validation_sha256": sha(args.validation.read_bytes()), "components": {"adapter": "AGPL-3.0-only"},
                  "source_build": "source/plugins/ics; default tests OFF; source/reader-plugin-kit; no external codec/core/Python"}
    add("SOURCE.json", (json.dumps(provenance, indent=2) + "\n").encode(), "Generated exact ICS build provenance", (binary,), kind="asset")
    group["members"] = list(records.values())
    group["deliveries"][sha(module)] = {"product_source_sha256": context, "validation_sha256": provenance["validation_sha256"], "members": expected}
    if args.write_inventory:
        update_group(inventory_path, group)
    elif any(ident not in {row["material"]["id"] for row in inventory.get("ics_reader_plugin", {}).get("members", [])} for ident in expected.values()):
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
