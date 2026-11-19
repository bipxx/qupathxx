"""Stage the QOI native module, exact offline sources and license notices.

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
PRODUCT = ("plugins/qoi/CMakeLists.txt", "plugins/qoi/qoi_reader.cpp",
    "plugins/image-common/rgba8_reader.hpp", "plugins/image-common/plugin_build.cmake",
    "plugins/image-common/reader_plugin_profile.hpp.in", "plugins/image-common/write_manifest.cmake")
OWN_FILES = PRODUCT + ("plugins/qoi/README.md", "plugins/qoi/reference-lock.json",
    "plugins/qoi/tests/qoi_plugin_tests.cpp", "plugins/qoi/tests/qoi_project_consumer.cpp",
    "plugins/qoi/tests/qoi_reference.cpp", "tools/ci/check_qoi_reader_plugin.py",
    "tools/ci/stage_qoi_reader_plugin.py")
sha = lambda data: hashlib.sha256(data).hexdigest()


def update_group(path, value):
    """Replace one planning-independent material group; preserve other raw text."""
    text = path.read_text(encoding="utf8")
    encoded = json.dumps(value, indent=2, ensure_ascii=False).replace("\n", "\n  ")
    match = re.search(r'^  "qoi_reader_plugin": ', text, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(text[start:])
        text = text[:start] + encoded + text[start + length:]
    else:
        end = text.rfind("}")
        text = text[:end].rstrip() + ',\n  "qoi_reader_plugin": ' + encoded + "\n}\n"
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
        raise ValueError("Need passing validation bound to these unchanged QOI module bytes")
    compiled = {key.replace("\\", "/"): value for key, value in
                json.loads(args.compiled_inputs.read_text(encoding="utf8")).items()}
    product_hashes = {}
    for name in PRODUCT:
        key = name
        digest = sha((ROOT / key).read_bytes())
        if compiled.get(key) != digest:
            raise ValueError("Product input differs from compiled snapshot: " + key)
        product_hashes[key] = digest
    context = sha(json.dumps(product_hashes, sort_keys=True, separators=(",", ":")).encode())
    manifest_path = args.module.parent / "reader-plugins.json"
    manifest_bytes = manifest_path.read_bytes()
    entries = json.loads(manifest_bytes)["plugins"]
    if (len(entries) != 1 or entries[0]["provider_id"] != "org.qupathxx.qoi"
            or entries[0]["module"]["sha256"] != sha(module)
            or entries[0]["version"] != "1.0.0" or entries[0]["contract"] != "qoi-rgba8-native-samples/1"
            or entries[0]["abi"] != 1 or entries[0]["dependencies"] != []
            or "options" in entries[0]
            or entries[0]["capabilities"] != {"display_rgba": True, "raw_samples": True}
            or validation.get("manifest_sha256") != sha(manifest_bytes)):
        raise ValueError("Manifest differs from the selected hash-bound QOI contract")
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Use a fresh staging directory; preserve existing evidence")
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf8"))
    group = inventory.get("qoi_reader_plugin", {"scope": "Local Windows x64 MSVC1944 Release /MD QOI plugin/source handoff; plugin-only package; separate native frontend evidence; no core/GUI/wheel rebuild/publication", "members": [], "deliveries": {}})
    records = {row["material"]["id"]: row for row in group["members"]}
    files, expected = {}, {}

    def add(name, data, origin, parents=("qupathxx-root-license",), kind="code"):
        ident = "qoi-plugin-AGPL-3-0-only-" + sha(data)[:24]
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
    receipt = "docs/benchmarks/qoi-reader-plugin/README.md"
    if (ROOT / receipt).is_file():
        add("source/" + receipt, (ROOT / receipt).read_bytes(), receipt, kind="asset")
    kit_parents = []
    for rel in ("include/qupathxx/reader_plugin.h", "share/qupathxx/reader-plugin/profile.cmake", "share/qupathxx/reader-plugin/LICENSE"):
        kit_parents.append(add("source/reader-plugin-kit/" + rel, (args.kit / rel).read_bytes(),
                               "unchanged relocated ReaderPluginKit " + rel, kind="notice" if rel.endswith("LICENSE") else "code"))
    for path in ("source/LICENSE", "licenses/qupathxx/LICENSE"):
        add(path, (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")
    binary = add("qpx_qoi_plugin.dll", module, "Selected independent QOI module; platform runtime only",
                 tuple(dict.fromkeys(product_parents + kit_parents)))
    add("reader-plugins.json", manifest_bytes, "Hash-bound QOI runtime manifest", (binary,), kind="asset")
    guide = ("# Native QOI plugin package\n\n"
             "Provider `org.qupathxx.qoi` version1.0.0, contract `qoi-rgba8-native-samples/1`.\n\n"
             "See the [complete profile/build/use guide](source/plugins/qoi/README.md) "
             "and [delivery receipt](source/docs/benchmarks/qoi-reader-plugin/README.md).\n\n"
             "Keep the DLL, reader-plugins.json and license together. Configure the absolute manifest "
             "with --reader-plugins and select --reader org.qupathxx.qoi. The native profile accepts empty options; raw is exact canonical RGBA8, and linear color hints affect display only.\n\n"
             "source/plugins/qoi plus source/plugins/image-common and source/reader-plugin-kit are complete offline native build inputs. "
             "Default tests OFF; no external codec, host/core or Python is needed for the module. "
             "See SOURCE.json and NOTICES.txt. Windows x64 MSVC1944/compiler19.44.35225.0, Release /MD.\n\n"
             "Close applications using the module before removing package-owned files; preserve user data.\n")
    add("README.md", guide.encode(), "Generated package-relative QOI guide", kind="asset")
    notice = ("QOI reader and authored sources/fixtures: qupathxx contributors, AGPL-3.0-only.\n"
              "Full text: licenses/qupathxx/LICENSE; exact corresponding source: source/.\n"
              "This module has no external codec dependency; platform CRT requirements follow the recorded kit profile.\n"
              "QOI specification1.0 is author-published CC0; the separately pinned MIT reference codec is a development oracle only, not packaged or linked. QOI/shared reader implementation is independently authored.\n")
    add("NOTICES.txt", notice.encode(), "Generated QOI component notice", kind="notice")
    provenance = {"schema": "qpx.qoi-reader-package/1", "provider_id": "org.qupathxx.qoi", "version": "1.0.0",
                  "contract": "qoi-rgba8-native-samples/1", "profile": entries[0]["profile"], "module_sha256": sha(module),
                  "product_source_sha256": context, "product_inputs": product_hashes,
                  "validation_sha256": sha(args.validation.read_bytes()), "components": {"adapter": "AGPL-3.0-only"},
                  "source_build": "source/plugins/qoi plus source/plugins/image-common; default tests OFF; source/reader-plugin-kit; no external codec/core/Python"}
    add("SOURCE.json", (json.dumps(provenance, indent=2) + "\n").encode(), "Generated exact QOI build provenance", (binary,), kind="asset")
    group["members"] = list(records.values())
    group["deliveries"][sha(module)] = {"product_source_sha256": context, "validation_sha256": provenance["validation_sha256"], "members": expected}
    if args.write_inventory:
        update_group(inventory_path, group)
    elif any(ident not in {row["material"]["id"] for row in inventory.get("qoi_reader_plugin", {}).get("members", [])} for ident in expected.values()):
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
