"""Stage two qualified typed-family DLLs and their exact offline AGPL sources.

Development packaging only: native providers/services own all interpretation.
The existing material admission guard owns package admission. Host/SDK runtime
and corresponding-source receipts are separate from this module/kit package.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
GROUP = "format_family_plugins"
PROFILE = "windows-x64-msvc1944-release-md"
PRODUCT = (
    "plugins/counts_tsv/CMakeLists.txt", "plugins/counts_tsv/counts_tsv.cpp",
    "plugins/point_tsv/CMakeLists.txt", "plugins/point_tsv/point_tsv.cpp",
    "plugins/format-common/strict_tsv.hpp", "plugins/format-common/plugin_build.cmake",
    "plugins/format-common/format_plugin_profile.hpp.in", "plugins/format-common/write_manifest.cmake",
)
KIT_FILES = (
    "include/qupathxx/reader_plugin.h", "include/qupathxx/format_plugin.h",
    "share/qupathxx/format-plugin/profile.cmake", "share/qupathxx/format-plugin/LICENSE",
)
OPTIONAL = (
    "plugins/counts_tsv/README.md", "plugins/point_tsv/README.md",
    "plugins/format-tests/format_plugin_consumer.cpp",
    "cpp/tests/format_plugin_tests.cpp", "cpp/tests/format_import_tests.cpp",
    "cpp/tests/format_plugin_bad_module.cpp", "cpp/tests/format_plugin_c_header_test.c",
    "tools/ci/check_format_family_plugins.py", "tools/ci/stage_format_family_plugins.py",
    "docs/benchmarks/format-family-plugins/README.md",
)
PROVIDERS = {
    "matrix_module": {
        "provider_id": "org.qupathxx.counts-tsv", "format_id": "counts-tsv", "domain": "matrix",
        "display_name": "Dense counts TSV", "version": "1.0.0",
        "contract": "dense-observation-feature-uint64-tsv/1", "abi": 1,
        "profile": PROFILE, "dependencies": [], "filename": "qpx_counts_tsv_plugin.dll",
    },
    "annotation_module": {
        "provider_id": "org.qupathxx.point-tsv", "format_id": "point-tsv", "domain": "annotation",
        "display_name": "Pixel point TSV", "version": "1.0.0",
        "contract": "pixel-point-annotation-tsv/1", "abi": 1,
        "profile": PROFILE, "dependencies": [], "filename": "qpx_point_tsv_plugin.dll",
    },
}


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def encoded(value) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def identity(value) -> str:
    return sha(json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8"))


def update_group(path: Path, value) -> None:
    """Update only this group; preserve all unrelated inventory raw text."""
    with path.open("r", encoding="utf-8", newline="") as stream:
        text = stream.read()
    replacement = json.dumps(value, indent=2, ensure_ascii=False).replace("\n", "\n  ")
    match = re.search(r'^  "' + GROUP + r'": ', text, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(text[start:])
        text = text[:start] + replacement + text[start + length:]
    else:
        end = text.rfind("}")
        if end < 0:
            raise ValueError("Inventory has no root object")
        text = text[:end].rstrip() + ',\n  "' + GROUP + '": ' + replacement + "\n}\n"
    json.loads(text)
    path.write_text(text, encoding="utf-8", newline="")


def command_exit(row, artifacts) -> None:
    """Keep expected CLI refusals distinct from failed validation commands."""
    expected, actual = row.get("expected_exit", 0), row.get("exit")
    if type(expected) is not int or type(actual) is not int or actual != expected:
        raise ValueError("Native validation command exit differs from its explicit expectation")
    if expected == 0:
        return
    if expected != 2:
        raise ValueError("Unexpected nonzero native validation expectation")
    argv = row.get("argv", [])
    allowed = {
        "cli-bad-hash.log": ("matrix-index", "org.qupathxx.counts-tsv", "bad_hash_plugins.json", 9),
        "cli-matrix-wrong-domain.log": ("matrix-index", "org.qupathxx.point-tsv", "manifest", 9),
        "cli-points-wrong-domain.log": ("points-import", "org.qupathxx.counts-tsv", "manifest", 11),
    }
    contract = allowed.get(Path(row.get("log", "")).name)
    if not contract or "cli" not in artifacts:
        raise ValueError("Exit2 is reserved for the three declared CLI refusal controls")
    operation, provider, manifest, length = contract
    if (len(argv) != length or argv[1] != "--format-plugins" or argv[3:5] != [operation, provider]
            or manifest not in artifacts
            or Path(argv[0]).resolve() != Path(artifacts["cli"]["path"]).resolve()
            or Path(argv[2]).resolve() != Path(artifacts[manifest]["path"]).resolve()):
        raise ValueError("Exit2 command differs from the declared hash/domain refusal control")


def validate_artifacts(report, modules: dict[str, bytes]) -> tuple[Path, bytes]:
    if (report.get("schema") != "qpx.format-family-validation/1" or report.get("passed") is not True
            or report.get("artifacts_unchanged") is not True or report.get("fixtures_unchanged") is not True):
        raise ValueError("Need passing typed-family validation with unchanged artifacts and fixtures")
    before, after = report["artifacts_before"], report["artifacts_after"]
    if not isinstance(before, dict) or before != after:
        raise ValueError("Validation before/after artifacts disagree")
    required = {*PROVIDERS, "manifest", "abi_test", "import_test", "bad_manifest"}
    if not required.issubset(before):
        raise ValueError("Validation is missing required native artifacts")
    # Every retained controller artifact, including SDK and changed-module
    # controls when present, must still agree with the passing report.
    for name, row in before.items():
        path = Path(row["path"])
        if not path.is_absolute() or sha(path.read_bytes()) != row["sha256"]:
            raise ValueError("Validated artifact changed or disappeared: " + name)
        if name in modules and row["sha256"] != sha(modules[name]):
            raise ValueError("Selected DLL differs from validated bytes: " + name)
    commands = report.get("commands", [])
    if not commands:
        raise ValueError("Validation contains no successful complete native command set")
    for row in commands:
        command_exit(row, before)
        if sha(Path(row["log"]).read_bytes()) != row["log_sha256"]:
            raise ValueError("Retained native validation log changed")
    abi = [row["argv"] for row in commands if len(row.get("argv", [])) == 5 and row["argv"][1] == "--abi"]
    if len(abi) != 1:
        raise ValueError("Need the direct native two-provider ABI command")
    fixtures = Path(abi[0][4])
    fixture_bytes = (fixtures / "fixtures.json").read_bytes()
    if sha(fixture_bytes) != report["fixtures_sha256"]:
        raise ValueError("Validated fixture manifest changed")
    for name, digest in json.loads(fixture_bytes)["files"].items():
        relative = Path(name)
        if relative.is_absolute() or relative.drive or ".." in relative.parts:
            raise ValueError("Unexpected authored fixture path")
        if sha((fixtures / relative).read_bytes()) != digest:
            raise ValueError("Validated authored fixture changed: " + name)
    manifest = Path(before["manifest"]["path"])
    return manifest, manifest.read_bytes()


def selected_manifest(path: Path, data: bytes, modules: dict[str, bytes]) -> dict:
    document = json.loads(data)
    if set(document) != {"schema", "plugins"} or document["schema"] != "qpx.format-plugins/1":
        raise ValueError("Unexpected typed-family manifest schema")
    entries = document["plugins"]
    if not isinstance(entries, list) or len(entries) != 2:
        raise ValueError("Need exactly the two selected typed providers")
    by_id = {row["provider_id"]: row for row in entries}
    if len(by_id) != 2:
        raise ValueError("Duplicate provider IDs in validated manifest")
    result = {"schema": "qpx.format-plugins/1", "plugins": []}
    for name, expected in PROVIDERS.items():
        entry = by_id[expected["provider_id"]]
        fields = {key: value for key, value in expected.items() if key != "filename"}
        if set(entry) != set(fields) | {"module"} or any(entry[key] != value for key, value in fields.items()):
            raise ValueError("Manifest differs from selected typed contract: " + name)
        module = entry["module"]
        if set(module) != {"path", "sha256"} or module["sha256"] != sha(modules[name]):
            raise ValueError("Manifest DLL digest disagrees: " + name)
        source = Path(module["path"])
        if source.is_absolute() or ".." in source.parts or source.drive:
            raise ValueError("Validated runtime DLL path must be package relative")
        if sha((path.parent / source).read_bytes()) != module["sha256"]:
            raise ValueError("Validated runtime module changed: " + name)
        result["plugins"].append({**fields, "module": {"path": expected["filename"], "sha256": module["sha256"]}})
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("matrix-module", "annotation-module", "validation", "compiled-inputs", "kit", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--write-inventory", action="store_true")
    args = parser.parse_args()
    modules = {name: getattr(args, name).read_bytes() for name in PROVIDERS}
    for name, expected in PROVIDERS.items():
        if getattr(args, name).name != expected["filename"]:
            raise ValueError("Unexpected module filename: " + name)
    validation_bytes = args.validation.read_bytes()
    validation = json.loads(validation_bytes)
    manifest_path, validated_manifest = validate_artifacts(validation, modules)
    runtime_manifest = selected_manifest(manifest_path, validated_manifest, modules)
    runtime_bytes = encoded(runtime_manifest)
    compiled = {key.replace("\\", "/"): value for key, value in
                json.loads(args.compiled_inputs.read_text(encoding="utf-8")).items()}
    source_bytes = {rel: (ROOT / rel).read_bytes() for rel in (*PRODUCT, "LICENSE")}
    product_hashes = {}
    for rel, data in source_bytes.items():
        if compiled.get(rel) != sha(data):
            raise ValueError("Product input differs from compiled snapshot: " + rel)
        product_hashes[rel] = sha(data)
    kit_bytes = {rel: (args.kit / rel).read_bytes() for rel in KIT_FILES}
    for rel in KIT_FILES[:2]:
        source = "cpp/" + rel
        if sha(kit_bytes[rel]) != compiled.get(source) or kit_bytes[rel] != (ROOT / source).read_bytes():
            raise ValueError("Typed kit header differs from compiled snapshot: " + rel)
    profile = ('set(QUPATHXX_FORMAT_PLUGIN_MSVC_VERSION "1944")\n'
               'set(QUPATHXX_FORMAT_PLUGIN_CXX_COMPILER_VERSION "19.44.35225.0")\n'
               'set(QUPATHXX_FORMAT_PLUGIN_ABI_VERSION 1)\n'
               'set(QUPATHXX_FORMAT_PLUGIN_PROFILE_ID "' + PROFILE + '")\n').encode()
    # CMake writes platform line endings on Windows. Admit these two exact
    # spellings, and retain the original bytes/hash as the compiler input.
    if kit_bytes[KIT_FILES[2]] not in (profile, profile.replace(b"\n", b"\r\n")) or kit_bytes[KIT_FILES[3]] != source_bytes["LICENSE"]:
        raise ValueError("Typed kit profile/license differs from the selected compiler/license contract")
    kit_hashes = {rel: sha(data) for rel, data in kit_bytes.items()}
    context = identity({"product_inputs": product_hashes, "kit_inputs": kit_hashes})
    output = args.output.resolve()
    if output.exists():
        raise ValueError("Use a fresh staging directory; preserve existing evidence")
    inventory_path = ROOT / "tests/fixtures/manifest.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    group = copy.deepcopy(inventory.get(GROUP, {
        "scope": "Local Windows x64 MSVC1944 Release /MD typed counts/point modules and offline kit/source; matching host/SDK qualification is separate; no wheel/UI/publication",
        "members": [], "deliveries": {},
    }))
    records = {row["material"]["id"]: row for row in group["members"]}
    files, members = {}, {}

    def add(name, data, origin, parents=("qupathxx-root-license",), kind="code"):
        ident = "format-family-AGPL-3-0-only-" + sha(data)[:24]
        record = {"filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
                  "material": {"id": ident, "category": "admitted_open", "kind": kind,
                               "license_id": "AGPL-3.0-only", "license_evidence": "LICENSE; repository author declaration",
                               "attribution": "qupathxx contributors", "source_ids": list(dict.fromkeys(parents))}}
        records.setdefault(ident, record)
        files[name] = data
        members[name] = ident
        return ident

    product_parents = [add("source/" + rel, source_bytes[rel], rel) for rel in PRODUCT]
    controls = {}
    for rel in OPTIONAL:
        if (ROOT / rel).is_file():
            data = (ROOT / rel).read_bytes()
            if Path(rel).suffix in {".cpp", ".c"} and compiled.get(rel) != sha(data):
                raise ValueError("Native test/control source differs from compiled snapshot: " + rel)
            controls[rel] = sha(data)
            add("source/" + rel, data, rel, kind="asset" if rel.endswith(".md") else "code")
    kit_parents = [add("source/format-plugin-kit/" + rel, data, "Matching relocated FormatPluginKit " + rel,
                       kind="notice" if rel.endswith("LICENSE") else "code") for rel, data in kit_bytes.items()]
    for name in ("source/LICENSE", "licenses/qupathxx/LICENSE"):
        add(name, source_bytes["LICENSE"], "LICENSE", kind="notice")
    binaries = {}
    for name, expected in PROVIDERS.items():
        binaries[name] = add(expected["filename"], modules[name], "Qualified independent " + expected["provider_id"] + "; platform CRT only",
                             tuple(dict.fromkeys(product_parents + kit_parents)))
    add("format-plugins.json", runtime_bytes, "Combined hash-bound typed runtime manifest", tuple(binaries.values()), kind="asset")
    add("source/evidence/validation.json", validation_bytes, "Exact passing native typed-family validation", kind="asset")
    add("source/evidence/validated-format-plugins.json", validated_manifest, "Exact validated pre-staging typed manifest", kind="asset")
    receipt = "source/docs/benchmarks/format-family-plugins/README.md"
    guide = ("# Native counts and point format plugins\n\n"
             "Two independent AGPL-3.0-only modules: `org.qupathxx.counts-tsv` and `org.qupathxx.point-tsv`, version1.0.0.\n\n"
             "Read the [counts contract](source/plugins/counts_tsv/README.md) and "
             "[point contract](source/plugins/point_tsv/README.md).\n\n")
    if receipt in files:
        guide += "See the [delivery receipt](" + receipt + ").\n\n"
    guide += ("Keep both DLLs, format-plugins.json and licenses together. Use the matching FP04 native host/SDK; "
              "its runtime and source receipts are supplied separately. These modules do not change the frozen image-provider runtime. "
              "Options are empty; the host owns position joins, identity/currentness, QMX publication and annotation preview/apply.\n\n"
              "The providers accept source labels/IDs of at most 4096 UTF-8 bytes. Native publication applies the existing "
              "nonempty UTF-8 identifier limit of 256 bytes to matrix feature/observation IDs, feature display names and annotation source IDs; "
              "longer values refuse before publication. Observation display names must be empty. Annotation source class labels "
              "and mapped native class keys retain their separate 4096-byte bounds.\n\n"
              "The source/plugins/counts_tsv and source/plugins/point_tsv trees require their sibling source/plugins/format-common "
              "and source/format-plugin-kit. Build each standalone CMake project with QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX set to the "
              "absolute kit prefix. No download, core SDK, Python, Java or codec is needed to build these two modules. "
              "Windows x64 MSVC1944/compiler19.44.35225.0, Release /MD.\n\n"
              "Source test/control programs are development evidence and may additionally require the separate matching native SDK. "
              "SOURCE.json binds exact module, source, kit and validation bytes. NOTICES.txt and licenses/qupathxx/LICENSE retain "
              "the actual AGPL obligations; commercial use does not remove them.\n\n"
              "Close users of these DLLs before removing package-owned files. Preserve user data and the separate native host/SDK.\n")
    add("README.md", guide.encode(), "Generated package-relative typed-family guide", kind="asset")
    notices = ("Counts TSV, point TSV, shared helpers, C ABI headers and authored controls: qupathxx contributors, AGPL-3.0-only.\n"
               "Full license: licenses/qupathxx/LICENSE. Exact corresponding module/kit source: source/.\n"
               "Commercial use is permitted under the actual AGPL terms; this is not a permissive or proprietary-code license.\n"
               "Distribution and modified network interaction retain applicable AGPL source/notice obligations.\n"
               "No third-party parser, codec, model, fixture or Java/Python runtime is included in either module.\n"
               "Platform CRT/compiler requirements follow the exact FormatPluginKit profile. Host/SDK components and notices are separate.\n")
    add("NOTICES.txt", notices.encode(), "Generated typed-family component and source notice", kind="notice")
    provenance = {
        "schema": "qpx.format-family-package/1", "profile": PROFILE,
        "providers": [{key: value for key, value in item.items() if key != "filename"}
                      | {"module": item["filename"], "module_sha256": sha(modules[name])} for name, item in PROVIDERS.items()],
        "product_source_sha256": context, "product_inputs": product_hashes, "kit_inputs": kit_hashes,
        "control_and_doc_inputs": controls,
        "compiled_inputs_sha256": sha(args.compiled_inputs.read_bytes()),
        "validation_sha256": sha(validation_bytes), "validated_manifest_sha256": sha(validated_manifest),
        "runtime_manifest_sha256": sha(runtime_bytes), "components": {"providers_and_kit": "AGPL-3.0-only"},
        "source_build": "source/plugins/{counts_tsv,point_tsv}; sibling format-common; source/format-plugin-kit; no external core/codec/Python/Java",
        "host_sdk": "Matching native candidate required; its components and source receipts are separately supplied",
    }
    add("SOURCE.json", encoded(provenance), "Generated exact typed-family build provenance", tuple(binaries.values()), kind="asset")
    delivery = identity({"modules": {name: sha(data) for name, data in modules.items()},
                         "product_source_sha256": context, "validation_sha256": sha(validation_bytes),
                         "runtime_manifest_sha256": sha(runtime_bytes)})
    group["members"] = list(records.values())
    group["deliveries"][delivery] = {"product_source_sha256": context,
        "module_sha256": {name: sha(data) for name, data in modules.items()},
        "validation_sha256": sha(validation_bytes), "runtime_manifest_sha256": sha(runtime_bytes), "members": members}
    if args.write_inventory:
        update_group(inventory_path, group)
    elif any(ident not in {row["material"]["id"] for row in inventory.get(GROUP, {}).get("members", [])} for ident in members.values()):
        raise ValueError("Exact package members need a scoped format_family_plugins inventory update")
    for name, data in files.items():
        target = output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    command = [sys.executable, str(ROOT / "tools/ci/check_material_staging.py"), "--stage", str(output), "--inventory", str(inventory_path)]
    checked = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
    result = {"delivery_id": delivery, "module_sha256": {name: sha(data) for name, data in modules.items()},
              "product_source_sha256": context, "files": len(files),
              "members": {name: {"sha256": sha(data), "bytes": len(data), "material_id": members[name]} for name, data in files.items()},
              "admission_exit": checked.returncode, "admission_output": checked.stdout + checked.stderr}
    (output.parent / (output.name + "-admission.json")).write_bytes(encoded(result))
    if checked.returncode:
        raise ValueError(result["admission_output"])
    print(json.dumps({"admitted": True, "files": len(files), "delivery_id": delivery,
                      "module_sha256": result["module_sha256"], "output": str(output)}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit(str(error))
