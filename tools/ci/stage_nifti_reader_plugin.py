"""Stage the native NIfTI-1 module, exact offline sources, and license notices."""
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
CONTRACT = "nifti-scalar-window/2"
CAPABILITIES = {"display_rgba": True, "raw_samples": True, "source_files": True}
ZLIB_SHA256 = "bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"
ZLIB_SOURCE = "source/third_party/zlib-1.3.2.tar.gz"
ZLIB_NOTICE = "licenses/zlib/LICENSE"
ZLIB_MATERIAL = "release-src-zlib"
PROVIDER = "org.qupathxx.nifti"
MODULE = "qpx_nifti_plugin.dll"
PRODUCT = (
    "plugins/nifti/CMakeLists.txt",
    "plugins/nifti/nifti_reader.cpp",
    "plugins/image-common/rgba8_reader.hpp",
    "plugins/image-common/gzip_member.hpp",
    "plugins/image-common/plugin_build.cmake",
    "plugins/image-common/reader_plugin_profile.hpp.in",
    "plugins/image-common/write_manifest.cmake",
)
OWN_FILES = PRODUCT + (
    "plugins/nifti/README.md",
    "plugins/nifti/tests/nifti_plugin_tests.cpp",
    "plugins/nifti/tests/nifti_project_consumer.cpp",
    "tools/ci/check_nifti_reader_plugin.py",
    "tools/ci/stage_nifti_reader_plugin.py",
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def update_group(path, value):
    """Replace the NIfTI-1 inventory group while preserving surrounding text."""
    text = path.read_text(encoding="utf8")
    encoded = json.dumps(value, indent=2, ensure_ascii=False).replace("\n", "\n  ")
    match = re.search(r'^  "nifti_reader_plugin": ', text, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(text[start:])
        text = text[:start] + encoded + text[start + length:]
    else:
        end = text.rfind("}")
        text = text[:end].rstrip() + ',\n  "nifti_reader_plugin": ' + encoded + "\n}\n"
    json.loads(text)
    path.write_text(text, encoding="utf8", newline="")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("module", "validation", "compiled-inputs", "kit", "zlib-archive", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    validation = json.loads(args.validation.read_text(encoding="utf8"))
    module = args.module.read_bytes()
    if (not validation.get("passed") or not validation.get("artifacts_unchanged")
            or validation.get("module_sha256") != sha(module)):
        raise ValueError("Need passing validation bound to these unchanged NIfTI module bytes")
    compiled = {key.replace("\\", "/"): value for key, value in
                json.loads(args.compiled_inputs.read_text(encoding="utf8")).items()}
    product_hashes = {}
    for key in PRODUCT:
        digest = sha((ROOT / key).read_bytes())
        if compiled.get(key) != digest:
            raise ValueError("Product input differs from compiled snapshot: " + key)
        product_hashes[key] = digest
    codec = args.zlib_archive.read_bytes()
    if sha(codec) != ZLIB_SHA256 or compiled.get(ZLIB_SOURCE) != ZLIB_SHA256:
        raise ValueError("The bundled zlib source must match the admitted and compiled archive")
    product_hashes[ZLIB_SOURCE] = ZLIB_SHA256
    context = sha(json.dumps(product_hashes, sort_keys=True, separators=(",", ":")).encode())
    manifest_path = args.module.parent / "reader-plugins.json"
    manifest_bytes = manifest_path.read_bytes()
    manifest = json.loads(manifest_bytes)
    entries = manifest["plugins"]
    if (manifest.get("schema") != "qpx.reader-plugins/1"
            or len(entries) != 1 or entries[0]["provider_id"] != PROVIDER
            or entries[0]["module"].get("path") != MODULE
            or entries[0].get("display_name") != "NIfTI"
            or entries[0]["module"]["sha256"] != sha(module)
            or entries[0]["version"] != VERSION or entries[0]["contract"] != CONTRACT
            or entries[0]["abi"] != 1 or entries[0]["dependencies"] != []
            or "options" in entries[0]
            or entries[0]["capabilities"] != CAPABILITIES
            or validation.get("manifest_sha256") != sha(manifest_bytes)):
        raise ValueError("Manifest differs from the selected hash-bound NIfTI contract")
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Use a fresh staging directory; preserve existing evidence")
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf8"))
    group = inventory.get("nifti_reader_plugin", {
        "scope": "Local Windows x64 MSVC Release /MD NIfTI-1 module and exact offline source handoff",
        "members": [],
        "deliveries": {},
    })
    records = {row["material"]["id"]: row for row in group["members"]}
    recorded_ids = set(records) | {ZLIB_MATERIAL}
    files, expected = {}, {}

    def add(name, data, origin, parents=("qupathxx-root-license",), kind="code", *,
            license_id="AGPL-3.0-only", evidence="LICENSE; repository author declaration",
            attribution="qupathxx contributors"):
        ident = "nifti-plugin-" + license_id.replace(".", "-") + "-" + sha(data)[:24]
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
    receipt = "docs/benchmarks/nifti-reader-plugin/README.md"
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
    binary = add(MODULE, module, "Selected independent NIfTI module with private static zlib",
                 tuple(dict.fromkeys(product_parents + kit_parents + [ZLIB_MATERIAL, codec_notice])))
    add("reader-plugins.json", manifest_bytes, "Hash-bound NIfTI runtime manifest", (binary,), kind="asset")
    guide = ("# Native NIfTI plugin package\n\n"
             f"Provider `{PROVIDER}` version {VERSION}, contract `{CONTRACT}`.\n\n"
             "See the [profile and build guide](source/plugins/nifti/README.md) "
             "and [delivery receipt](source/docs/benchmarks/nifti-reader-plugin/README.md).\n\n"
             "Keep the DLL, reader-plugins.json and licenses together. Supply the absolute manifest "
             "path with --reader-plugins and select --reader org.qupathxx.nifti. The selected profile "
             "opens scalar XY, XYZ or XYZT NIfTI-1 and NIfTI-2 images in single files or same-stem pairs. "
             "For paired data, select the `.hdr` or `.hdr.gz` and keep the companion beside it. "
             "Each member may independently use gzip. "
             "Display uses Z0/T0. Raw regions retain stored values at the requested Z and T.\n\n"
             "The offline build uses source/plugins/nifti, source/plugins/image-common, "
             "source/reader-plugin-kit and the zlib archive. Set QUPATHXX_NIFTI_ZLIB_ARCHIVE "
             f"to {ZLIB_SOURCE}. Tests default to OFF. See SOURCE.json and NOTICES.txt. "
             f"The module was built for {entries[0]['profile']}.\n\n"
             "Close applications using the module before removing package-owned files. Preserve user data.\n")
    add("README.md", guide.encode(), "Generated package-relative NIfTI-1 guide", kind="asset")
    notice = ("NIfTI reader and authored source: qupathxx contributors, AGPL-3.0-only.\n"
              "Full text at licenses/qupathxx/LICENSE. Exact corresponding source under source/.\n"
              f"The module statically links unmodified zlib 1.3.2 under the Zlib license. Source at {ZLIB_SOURCE}.\n"
              f"The original zlib notice is at {ZLIB_NOTICE}. Preserve these notices when redistributing.\n"
              "The platform CRT follows the recorded kit profile.\n"
              "The adapter was written from the published NIfTI-1 and NIfTI-2 format definitions.\n"
              "NiBabel 5.3.2 supplies independent development checks and is outside the distributed package. "
              "Its core uses MIT and its COPYING file records third-party declarations.\n")
    add("NOTICES.txt", notice.encode(), "Generated NIfTI-1 component notice", kind="notice")
    provenance = {"schema": "qpx.nifti-reader-package/1", "provider_id": PROVIDER, "version": VERSION,
                  "contract": CONTRACT, "profile": entries[0]["profile"], "capabilities": CAPABILITIES,
                  "module_sha256": sha(module),
                  "product_source_sha256": context, "product_inputs": product_hashes,
                  "validation_sha256": sha(args.validation.read_bytes()),
                  "components": {"adapter": "AGPL-3.0-only", "zlib": {"version": "1.3.2", "license": "Zlib",
                                 "source": ZLIB_SOURCE, "sha256": ZLIB_SHA256, "notice": ZLIB_NOTICE}},
                  "source_build": "source/plugins/nifti; tests default OFF; source/reader-plugin-kit; set QUPATHXX_NIFTI_ZLIB_ARCHIVE"}
    add("SOURCE.json", (json.dumps(provenance, indent=2) + "\n").encode(), "Generated exact NIfTI build provenance", (binary,), kind="asset")
    group["members"] = list(records.values())
    group["deliveries"][sha(module)] = {"product_source_sha256": context, "validation_sha256": provenance["validation_sha256"], "members": expected}
    if args.write_inventory:
        update_group(inventory_path, group)
    elif not set(expected.values()).issubset(recorded_ids):
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
