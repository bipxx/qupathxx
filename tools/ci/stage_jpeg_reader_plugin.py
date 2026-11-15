"""Stage the selected JPEG module, exact sources and notices for local handoff.

Development packaging only. Admission records extend the existing fixture/
material inventory; this is not a runtime downloader or licensing database.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
PRODUCT = ("CMakeLists.txt", "jpeg_reader.cpp", "jpeg_decoder.c", "jpeg_decoder.h",
           "jpeg_memory_backend.c", "jpeg_memory_backend.h", "reader_plugin_profile.hpp.in",
           "write_manifest.cmake", "libjpeg-source-lock.json")
OWN_FILES = tuple("plugins/jpeg/" + name for name in PRODUCT) + (
    "plugins/jpeg/README.md", "plugins/jpeg/tests/jpeg_plugin_tests.cpp",
    "plugins/jpeg/tests/jpeg_project_consumer.cpp", "tools/ci/prepare_jpeg_reader_sources.py",
    "tools/ci/check_jpeg_reader_plugin.py", "tools/ci/stage_jpeg_reader_plugin.py")
sha = lambda data: hashlib.sha256(data).hexdigest()


def update_group(path, value):
    """Patch one top-level inventory value; preserve all unrelated raw text."""
    text = path.read_text(encoding="utf8")
    encoded = json.dumps(value, indent=2, ensure_ascii=False)
    encoded = encoded.replace("\n", "\n  ")
    match = re.search(r'^  "jpeg_reader_plugin": ', text, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(text[start:])
        text = text[:start] + encoded + text[start + length:]
    else:
        end = text.rfind("}")
        text = text[:end].rstrip() + ',\n  "jpeg_reader_plugin": ' + encoded + "\n}\n"
    json.loads(text)
    path.write_text(text, encoding="utf8", newline="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("module", "validation", "compiled-inputs", "kit", "sources", "output"):
        parser.add_argument("--" + key, type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    validation = json.loads(args.validation.read_text(encoding="utf8"))
    module = args.module.read_bytes()
    if not validation.get("passed") or not validation.get("artifacts_unchanged") or validation.get("module_sha256") != sha(module):
        raise ValueError("Need passing validation bound to these unchanged module bytes")
    compiled = {key.replace("\\", "/"): value for key, value in
                json.loads(args.compiled_inputs.read_text(encoding="utf8")).items()}
    product_hashes = {}
    for name in PRODUCT:
        key = "plugins/jpeg/" + name
        digest = sha((ROOT / key).read_bytes())
        if compiled.get(key) != digest:
            raise ValueError("Product input differs from compiled snapshot: " + key)
        product_hashes[key] = digest
    context = sha(json.dumps(product_hashes, sort_keys=True, separators=(",", ":")).encode())
    manifest_path = args.module.parent / "reader-plugins.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf8"))
    entries = manifest["plugins"]
    if len(entries) != 1 or entries[0]["provider_id"] != "org.qupathxx.jpeg" or entries[0]["module"]["sha256"] != sha(module):
        raise ValueError("Module manifest differs from selected JPEG provider")
    if (entries[0]["version"] != "1.0.0" or entries[0]["contract"] != "jpeg8-oriented-decoded-samples/1"
            or entries[0]["abi"] != 1 or entries[0]["dependencies"] != []
            or entries[0]["capabilities"] != {"display_rgba": True, "raw_samples": True}):
        raise ValueError("Module manifest differs from the selected JPEG contract")
    if manifest_path.read_bytes() and validation.get("manifest_sha256") != sha(manifest_path.read_bytes()):
        raise ValueError("Manifest differs from validation")
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Use a fresh staging directory; existing evidence is never overwritten")
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf8"))
    group = inventory.get("jpeg_reader_plugin", {"scope": "Local Windows x64 MSVC1944 Release /MD JPEG plugin and exact source/notice handoff; no host, wheel or UI rebuild/publication", "members": [], "deliveries": {}})
    records = {row["material"]["id"]: row for row in group["members"]}
    files, expected = {}, {}

    def add(name, data, license_id, origin, parents=(), url=None, kind="code", credit="qupathxx contributors", evidence=None):
        digest = sha(data)
        ident = "jpeg-plugin-" + re.sub(r"[^A-Za-z0-9-]+", "-", license_id) + "-" + digest[:24]
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": digest,
            "material": {"id": ident, "category": "admitted_open", "kind": kind,
                "license_id": license_id, "license_evidence": evidence or url or "LICENSE; repository author declaration",
                "attribution": credit, "source_ids": list(parents)}}
        if url: record["url"] = url
        records.setdefault(ident, record)
        files[name] = data; expected[name] = ident
        return ident

    firstparty = []
    for rel in OWN_FILES:
        firstparty.append(add("source/" + rel, (ROOT / rel).read_bytes(), "AGPL-3.0-only", rel,
            ("qupathxx-root-license",), kind="asset" if rel.endswith(".md") else "code"))
    receipt = "docs/benchmarks/jpeg-reader-plugin/README.md"
    if (ROOT / receipt).is_file():
        add("source/" + receipt, (ROOT / receipt).read_bytes(), "AGPL-3.0-only", receipt, ("qupathxx-root-license",), kind="asset")
    kit_parents = []
    for rel in ("include/qupathxx/reader_plugin.h", "share/qupathxx/reader-plugin/profile.cmake", "share/qupathxx/reader-plugin/LICENSE"):
        kit_parents.append(add("source/reader-plugin-kit/" + rel, (args.kit / rel).read_bytes(), "AGPL-3.0-only",
            "unchanged relocated ReaderPluginKit " + rel, ("qupathxx-root-license",), kind="notice" if rel.endswith("LICENSE") else "code"))
    deps = []
    for family, lock_name, license_id, upstream, credit in (
        ("libjpeg-turbo", "libjpeg-source-lock.json", "BSD-3-Clause AND IJG AND Zlib",
         "https://raw.githubusercontent.com/libjpeg-turbo/libjpeg-turbo/3.1.3/",
         "Independent JPEG Group; D. R. Commander; libjpeg-turbo contributors; retained per-file notices"),):
        lock = json.loads((ROOT / "plugins/jpeg" / lock_name).read_text(encoding="utf8"))
        for rel, entry in lock["members"].items():
            data = (args.sources / family / rel).read_bytes()
            if sha(data) != entry["sha256"] or len(data) != entry["bytes"]:
                raise ValueError("Changed locked dependency: " + family + "/" + rel)
            ident = add("source/deps/" + family + "/" + rel, data, license_id,
                "locked " + family + " source " + rel,
                ("release-src-libjpeg-turbo",), url=upstream + rel,
                kind="notice" if rel in ("LICENSE.md", "README.ijg") else "code", credit=credit, evidence=upstream + "LICENSE.md; " + upstream + "README.ijg")
            deps.append(ident)
            if rel in ("LICENSE.md", "README.ijg"):
                add("licenses/" + family + "/" + rel, data, license_id, "retained " + family + " license",
                    ("release-src-libjpeg-turbo",), url=upstream + rel, kind="notice", credit=credit)
    add("licenses/qupathxx/LICENSE", (ROOT / "LICENSE").read_bytes(), "AGPL-3.0-only", "LICENSE", ("qupathxx-root-license",), kind="notice")
    add("source/LICENSE", (ROOT / "LICENSE").read_bytes(), "AGPL-3.0-only", "LICENSE", ("qupathxx-root-license",), kind="notice")
    binary_id = add("qpx_jpeg_plugin.dll", module, "AGPL-3.0-only", "selected JPEG module; static libjpeg-turbo retains its separate parent licenses",
        tuple(dict.fromkeys(firstparty + kit_parents + deps)))
    add("reader-plugins.json", manifest_path.read_bytes(), "AGPL-3.0-only", "hash-bound JPEG runtime manifest", (binary_id,), kind="asset")
    package_readme = ("# Native JPEG plugin package\n\n"
        "Provider `org.qupathxx.jpeg`, version1.0.0, contract `jpeg8-oriented-decoded-samples/1`.\n\n"
        "See the [complete profile and build/use guide](source/plugins/jpeg/README.md) "
        "and [delivery receipt](source/docs/benchmarks/jpeg-reader-plugin/README.md).\n\n"
        "Configure the absolute reader-plugins.json path with --reader-plugins and select "
        "--reader org.qupathxx.jpeg explicitly. Keep the DLL, manifest and licenses together.\n\n"
        "The source/ directory contains the exact offline native build inputs: plugins/jpeg, "
        "reader-plugin-kit and deps/libjpeg-turbo. No Python or application core is needed "
        "for that default module build. See SOURCE.json and NOTICES.txt for identities and components.\n\n"
        "Windows x64, recorded MSVC1944/compiler19.44.35225.0, Release /MD only. "
        "Close applications using the module before removal; preserve user files.\n")
    add("README.md", package_readme.encode(), "AGPL-3.0-only", "generated package-relative JPEG guide", ("qupathxx-root-license",), kind="asset")
    notice = ("JPEG reader adapter: qupathxx contributors, AGPL-3.0-only (licenses/qupathxx/LICENSE).\n"
              "Static libjpeg-turbo3.1.3: Independent JPEG Group, D. R. Commander and contributors.\n"
              "Applicable IJG/BSD-3-Clause/Zlib terms: licenses/libjpeg-turbo/LICENSE.md and README.ijg.\n"
              "This software is based in part on the work of the Independent JPEG Group.\n"
              "Upstream component terms and notices are retained; the adapter license does not replace them.\n")
    add("NOTICES.txt", notice.encode(), "AGPL-3.0-only", "generated JPEG package component notice", tuple(dict.fromkeys(deps)), kind="notice")
    provenance = {"schema": "qpx.jpeg-reader-package/1", "provider_id": "org.qupathxx.jpeg", "version": "1.0.0",
        "contract": "jpeg8-oriented-decoded-samples/1", "profile": entries[0]["profile"],
        "module_sha256": sha(module), "product_source_sha256": context, "product_inputs": product_hashes,
        "validation_sha256": sha(args.validation.read_bytes()), "components": {"adapter": "AGPL-3.0-only", "libjpeg-turbo": "BSD-3-Clause AND IJG AND Zlib"},
        "source_build": "source/plugins/jpeg; default tests OFF; source/reader-plugin-kit and source/deps/libjpeg-turbo are complete offline build inputs"}
    add("SOURCE.json", (json.dumps(provenance, indent=2) + "\n").encode(), "AGPL-3.0-only", "generated exact JPEG source/build provenance", (binary_id,), kind="asset")
    group["members"] = list(records.values())
    group["deliveries"][sha(module)] = {"product_source_sha256": context, "validation_sha256": provenance["validation_sha256"], "members": expected}
    if args.write_inventory:
        update_group(inventory_path, group)
    else:
        present = {row["material"]["id"] for row in inventory.get("jpeg_reader_plugin", {}).get("members", [])}
        if any(ident not in present for ident in expected.values()):
            raise ValueError("Exact package members need their reviewed inventory update")
    for name, data in files.items():
        path = output / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
    command = [sys.executable, str(ROOT / "tools/ci/check_material_staging.py"), "--stage", str(output), "--inventory", str(inventory_path)]
    checked = subprocess.run(command, capture_output=True, text=True)
    result = {"module_sha256": sha(module), "product_source_sha256": context, "files": len(files),
        "members": {name: {"sha256": sha(data), "bytes": len(data), "material_id": expected[name]} for name, data in files.items()},
        "admission_exit": checked.returncode, "admission_output": checked.stdout + checked.stderr}
    (output.parent / (output.name + "-admission.json")).write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    if checked.returncode: raise ValueError(result["admission_output"])
    print(json.dumps({"files": len(files), "admitted": True, "output": str(output), "module_sha256": sha(module)}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(str(error))
