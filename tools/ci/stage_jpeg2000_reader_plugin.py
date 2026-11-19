"""Stage the hash-bound JPEG 2000 module, corresponding sources and notices."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tarfile
import zipfile

from check_material_staging import check_archive, check_stage, inventory as read_inventory

ROOT = Path(__file__).resolve().parents[2]
RELEASE = "https://github.com/jasper-software/jasper/releases/download/version-4.2.9/jasper-4.2.9.tar.gz"
RELEASE_HASH = "f71cf643937a5fcaedcfeb30a22ba406912948ad4413148214df280afc425454"
LOCK = "plugins/jpeg2000/jasper-sources.sha256"
GROUP = "jpeg2000_reader_plugin"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encode(value):
    return (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def stage(args):
    if sha(args.jasper.read_bytes()) != RELEASE_HASH:
        raise ValueError("JasPer release archive differs from the pinned upstream asset")
    verification = args.validation.read_bytes()
    report = json.loads(verification)
    manifest = args.module.with_name("reader-plugins.json").read_bytes()
    entry, = json.loads(manifest)["plugins"]
    module = args.module.read_bytes()
    digest = sha(module)
    if (not report.get("passed") or report["module_sha256"] != digest or
            report["manifest_sha256"] != sha(manifest) or entry["module"]["sha256"] != digest or
            entry["provider_id"] != "org.qupathxx.jpeg2000" or entry["dependencies"]):
        raise ValueError("Validation and manifest must identify the exact JPEG 2000 module")

    inventory_path = ROOT / "tests/fixtures/manifest.json"
    original = inventory_path.read_text(encoding="utf-8")
    group = json.loads(original).get(GROUP, {"members": [], "deliveries": {}})
    records = {item["material"]["id"]: item for item in group["members"]}
    files, membership = {}, {}

    def add(name, data, origin, license_id="AGPL-3.0-only", kind="code", parents=None):
        ident = "jpeg2000-" + license_id + "-" + sha(data)[:24]
        evidence = "LICENSE; repository author declaration"
        attribution = "qupathxx contributors"
        if license_id == "JasPer-2.0":
            evidence = "source/jasper/LICENSE.txt and source/jasper/COPYRIGHT.txt"
            attribution = "Michael David Adams, Image Power, Inc., The University of British Columbia"
        records.setdefault(ident, {
            "filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
            "material": {"id": ident, "category": "admitted_open", "kind": kind,
                         "license_id": license_id, "license_evidence": evidence,
                         "attribution": attribution, "source_ids": parents if parents is not None
                         else ["qupathxx-root-license"]},
        })
        files[name] = data
        membership[name] = ident
        return ident

    source_ids = []
    lock = (ROOT / LOCK).read_bytes()
    with tarfile.open(args.jasper, "r:gz") as source:
        for line in lock.decode("ascii").splitlines():
            digest_expected, name = line.split("  ", 1)
            member = source.getmember("jasper-4.2.9/" + name)
            if not member.isfile():
                raise ValueError("Expected a regular JasPer source file")
            data = source.extractfile(member).read()
            if sha(data) != digest_expected:
                raise ValueError("JasPer source lock mismatch for " + name)
            notice = name in ("LICENSE.txt", "COPYRIGHT.txt")
            source_ids.append(add("source/jasper/" + name, data, RELEASE + "#" + name,
                                  "JasPer-2.0", "notice" if notice else "code", []))
            if notice:
                add("licenses/jasper/" + name, data, RELEASE + "#" + name, "JasPer-2.0", "notice", [])

    product = (LOCK, "plugins/jpeg2000/CMakeLists.txt", "plugins/jpeg2000/framing.hpp",
               "plugins/jpeg2000/jpeg2000_reader.cpp", "plugins/image-common/plugin_build.cmake",
               "plugins/image-common/reader_plugin_profile.hpp.in",
               "plugins/image-common/write_manifest.cmake", "plugins/image-common/rgba8_reader.hpp")
    for name in (*product, "plugins/jpeg2000/README.md", "plugins/jpeg2000/tests/jpeg2000_abi_test.cpp",
                 "tools/ci/check_jpeg2000_reader_plugin.py"):
        ident = add("source/" + name, (ROOT / name).read_bytes(), name)
        if name in product:
            source_ids.append(ident)
    for name in ("include/qupathxx/reader_plugin.h", "share/qupathxx/reader-plugin/profile.cmake",
                 "share/qupathxx/reader-plugin/LICENSE"):
        source_ids.append(add("source/reader-plugin-kit/" + name, (args.kit / name).read_bytes(),
                              "ReaderPluginKit " + name))
    add("licenses/qupathxx/LICENSE", (ROOT / "LICENSE").read_bytes(), "LICENSE", kind="notice")
    add("README.md", (ROOT / "plugins/jpeg2000/README.md").read_bytes(), "plugins/jpeg2000/README.md")
    binary = add(args.module.name, module, "qupathxx JPEG 2000 plugin with private static JasPer",
                 parents=sorted(set(source_ids)))
    add("reader-plugins.json", manifest, "Hash-bound native module manifest", kind="asset", parents=[binary])
    add("SOURCE.json", encode({"module_sha256": digest, "jasper_archive_sha256": RELEASE_HASH,
                              "source_lock_sha256": sha(lock), "validation_sha256": sha(verification),
                              "components": {"adapter": "AGPL-3.0-only", "JasPer": "JasPer-2.0"},
                              "private_jasper_adjustment": "Packet completeness check in source/plugins/jpeg2000/CMakeLists.txt",
                              "profile": entry["profile"]}),
        "JPEG 2000 package provenance", kind="asset", parents=[binary])
    group["members"] = list(records.values())
    group["deliveries"][digest] = {"members": membership}
    replacement = encode(group).decode("utf-8").strip().replace("\n", "\n  ")
    match = re.search(r'^  "' + GROUP + '": ', original, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(original[start:])
        updated = original[:start] + replacement + original[start + length:]
    else:
        updated = original[:original.rfind("}")].rstrip() + f',\n  "{GROUP}": ' + replacement + "\n}\n"
    json.loads(updated)
    args.output.mkdir(parents=True)
    for name, data in files.items():
        target = args.output / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    inventory_path.write_text(updated, encoding="utf-8", newline="")
    entries = read_inventory([inventory_path])
    check_stage(args.output, entries)
    with zipfile.ZipFile(args.archive, "x", zipfile.ZIP_DEFLATED) as output:
        for name in sorted(files):
            output.write(args.output / name, name)
    check_archive(args.archive, entries, expected=membership)
    print(f"Admitted {len(files)} JPEG 2000 package and archive members")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("module", "validation", "jasper", "kit", "output", "archive"):
        parser.add_argument("--" + name, type=Path, required=True)
    stage(parser.parse_args())
