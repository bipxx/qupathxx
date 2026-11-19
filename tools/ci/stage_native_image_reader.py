"""Stage a validated packed image plugin with source, kit and notices."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
import zipfile

from check_material_staging import check_archive, check_stage, inventory as read_inventory
import prepare_vti_xz_sources as vti_xz
from stage_png_reader_plugin import PRODUCT as PNG_PRODUCT
from stage_webp_reader_plugin import PRODUCT as WEBP_PRODUCT

ROOT = Path(__file__).resolve().parents[2]
SHARED_PRODUCT = (
    "plugins/image-common/rgba8_reader.hpp", "plugins/image-common/plugin_build.cmake",
    "plugins/image-common/reader_plugin_profile.hpp.in", "plugins/image-common/write_manifest.cmake",
)
DEPENDENCIES = {
    "zlib": ("1.3.2", "LICENSE", "Jean-loup Gailly and Mark Adler",
             "bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16"),
    "tinyxml2": ("11.0.0", "LICENSE.txt", "tinyxml2 upstream authors",
                 "5556deb5081fb246ee92afae73efd943c889cef0cafea92b0b82422d6a18f289"),
}
READER_DEPENDENCIES = {"metaimage": ("zlib",), "openexr": ("zlib",), "vti": ("tinyxml2", "zlib")}
JPEGXL_ARCHIVES = {
    "libjxl": ("0.13.0", "24d18c9af2d1f5a17730b3f8d3f70a5c16e5ac303f5d9d5fe9dc00cf7ca82af2",
               "BSD-3-Clause AND Apache-2.0 AND Zlib", "JPEG XL Project Authors",
               (("LICENSE", "BSD-3-Clause"), ("PATENTS", "BSD-3-Clause"))),
    "brotli": ("db24a98aed38a16de78cc05ca15cdd8dc2a0d92d", "113b9685dfc14dd2f3360d077283266eec2fbdfb0fe3e5600f11be151da0d68e",
               "MIT", "Brotli Authors", (("LICENSE", "MIT"),)),
    "highway": ("457c891775a7397bdb0376bb1031e6e027af1c48", "5124b0501c98d9930dbb065bfa1a5bbbd59ce0f12facb7e1e33aaef01a5f1f1a",
                "BSD-3-Clause AND Apache-2.0", "Highway Project Authors",
                (("LICENSE", "Apache-2.0"), ("LICENSE-BSD3", "BSD-3-Clause"))),
    "skcms": ("96d9171c94b937a1b5f0293de7309ac16311b722", "9bb4b5bba0b7c04f6c2bce9ff713d61e23c9a20c4945161ae16290498ad74627",
              "BSD-3-Clause", "Google Inc.", (("LICENSE", "BSD-3-Clause"),)),
}
AVIF_ARCHIVES = {
    "libavif": ("1.4.2", "00361372436719b3a926d521ca9f9061282ff01a50d2b6d5bc137ea5161d39f0",
                "BSD-2-Clause AND BSD-3-Clause", "libavif and bundled libyuv authors",
                (("LICENSE", "BSD-2-Clause AND BSD-3-Clause"),)),
    "libaom": ("3.14.1", "36895193b2893fdbe1bbd5194c48b0bb8034f7c1d594ace31a46a57035caf62c",
               "BSD-2-Clause", "Alliance for Open Media",
               (("LICENSE", "BSD-2-Clause"), ("PATENTS", "BSD-2-Clause"))),
}
SOURCE_ARCHIVES = {"jpegxl": JPEGXL_ARCHIVES, "avif": AVIF_ARCHIVES}
REUSED_CODEC_STAGES = {
    "apng": ("png", PNG_PRODUCT, ("licenses/libpng/LICENSE", "licenses/zlib/LICENSE")),
    "webp-animation": ("webp", WEBP_PRODUCT, tuple("licenses/libwebp/" + item
                        for item in ("COPYING", "PATENTS", "AUTHORS"))),
}
VTI_LZ4_MEMBERS = {
    "source/third_party/lz4-1.10.0-library.zip": "arrow-points-lz4-1.10.0-e9fb04130b7fc33727fa9e8e",
    "licenses/lz4/LICENSE": "arrow-points-lz4-1.10.0-8b58c446121a109ccf32edc0",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encode(value):
    return (json.dumps(value, indent=2) + "\n").encode("utf-8")


def stage(args, reader, extra_sources=(), dependencies=(), extra_product=(), source_archives=None):
    label = reader.upper()
    reader_key = reader.replace("-", "_")
    group_name = f"{reader_key}_reader_plugin"
    plugin = f"plugins/{reader}"
    driver = "animation_reader.cpp" if reader == "webp-animation" else f"{reader}_reader.cpp"
    product = (f"{plugin}/CMakeLists.txt", f"{plugin}/{driver}", *SHARED_PRODUCT, *extra_product)
    module = args.module.read_bytes()
    digest = sha(module)
    manifest = args.module.with_name("reader-plugins.json").read_bytes()
    entry, = json.loads(manifest)["plugins"]
    validation = json.loads(args.validation.read_bytes())
    if (not validation["passed"] or validation["module_sha256"] != digest or
            validation["manifest_sha256"] != sha(manifest) or entry["module"]["sha256"] != digest or
            entry["provider_id"] != f"org.qupathxx.{reader}" or entry["module"]["path"] != args.module.name
            or entry["dependencies"]):
        raise ValueError(f"Expected the exact validated {label} module and its manifest")

    inventory_path = ROOT / "tests/fixtures/manifest.json"
    original = inventory_path.read_text(encoding="utf-8")
    groups = json.loads(original)
    group = groups.get(group_name, {"members": [], "deliveries": {}})
    records = {item["material"]["id"]: item for item in group["members"]}
    files, membership = {}, {}

    def add(name, data, origin, kind="code", parents=None, license_id="AGPL-3.0-only",
            evidence="LICENSE", attribution="qupathxx contributors"):
        ident = f"{reader}-reader-{license_id.replace(' ', '-')}-" + sha(data)[:24]
        records.setdefault(ident, {
            "filename": Path(name).name, "source": origin, "bytes": len(data), "sha256": sha(data),
            "material": {"id": ident, "category": "admitted_open", "kind": kind,
                         "license_id": license_id, "license_evidence": evidence,
                         "attribution": attribution,
                         "source_ids": list(dict.fromkeys(parents)) if parents is not None else ["qupathxx-root-license"]},
        })
        files[name] = data
        membership[name] = ident
        return ident

    source_ids = []
    for path in product:
        source_ids.append(add("source/" + path, (ROOT / path).read_bytes(), path))
    for path in ("include/qupathxx/reader_plugin.h", "share/qupathxx/reader-plugin/profile.cmake",
                 "share/qupathxx/reader-plugin/LICENSE"):
        source_ids.append(add("source/reader-plugin-kit/" + path, (args.kit / path).read_bytes(),
                              "ReaderPluginKit " + path, "notice" if path.endswith("LICENSE") else "code"))
    guide = f"{plugin}/README.md"
    add("README.md", (ROOT / guide).read_bytes(), guide, "asset")
    validator = f"tools/ci/check_{reader_key}_reader_plugin.py"
    for path in (validator, *extra_sources):
        add("source/" + path, (ROOT / path).read_bytes(), path)
    add("licenses/qupathxx/LICENSE", (ROOT / "LICENSE").read_bytes(), "LICENSE", "notice")
    if reader in REUSED_CODEC_STAGES:
        provider, product_sources, notices = REUSED_CODEC_STAGES[reader]
        codec_stage = getattr(args, f"{provider}_stage")
        original_module = json.loads((codec_stage / "SOURCE.json").read_bytes())["module_sha256"]
        codec_group = groups[f"{provider}_reader_plugin"]
        admitted_members = codec_group["deliveries"][original_module]["members"]
        admitted_records = {item["material"]["id"]: item for item in codec_group["members"]}
        source_dir = f"source/plugins/{provider}/"
        selected = [source_dir + name for name in product_sources]
        selected += sorted(name for name in admitted_members if name.startswith("source/deps/"))
        selected += notices
        for name in selected:
            ident = admitted_members[name]
            expected = admitted_records[ident]
            data = (codec_stage / name).read_bytes()
            if len(data) != expected["bytes"] or sha(data) != expected["sha256"]:
                raise ValueError(f"Changed admitted codec input: {name}")
            if name.startswith(source_dir) and data != (ROOT / name.removeprefix("source/")).read_bytes():
                raise ValueError(f"Active codec build source differs from its admitted input: {name}")
            if name.startswith("licenses/") and data != (args.module.parent / name).read_bytes():
                raise ValueError(f"Installed codec notice differs from its source: {name}")
            files[name] = data
            membership[name] = ident
            source_ids.append(ident)
    for component, path in dependencies:
        version, license_file, author, dependency_sha = DEPENDENCIES[component]
        source = path.read_bytes()
        if sha(source) != dependency_sha:
            raise ValueError(f"Expected the admitted {component} {version} archive")
        source_root = f"{component}-{version}"
        name = f"source/third_party/{source_root}.tar.gz"
        files[name] = source
        membership[name] = f"release-src-{component}"
        notice = f"licenses/{component}/{license_file}"
        with tarfile.open(fileobj=io.BytesIO(source), mode="r:gz") as archive:
            with archive.extractfile(f"{source_root}/{license_file}") as member:
                original_notice = member.read()
        if (args.module.parent / notice).read_bytes() != original_notice:
            raise ValueError(f"Installed {component} notice differs from source")
        source_ids += [membership[name], add(notice, original_notice, f"Unmodified {source_root} {license_file}",
                      "notice", [membership[name]], "Zlib", notice, author)]
    if reader == "vti":
        admitted = read_inventory([inventory_path])
        for name, ident in VTI_LZ4_MEMBERS.items():
            expected = admitted[ident]
            value = (args.vti_lz4_stage / name).read_bytes()
            if len(value) != expected["bytes"] or sha(value) != expected["sha256"]:
                raise ValueError(f"Changed admitted LZ4 material: {name}")
            if name.startswith("licenses/") and value != (args.module.parent / name).read_bytes():
                raise ValueError("Installed LZ4 notice differs from the admitted source notice")
            files[name] = value
            membership[name] = ident
            source_ids.append(ident)
        decoder = args.vti_xz_archive.read_bytes()
        if sha(decoder) != vti_xz.ARCHIVE_SHA256:
            raise ValueError("Expected the exact XZ Embedded decoder source subset")
        evidence = "licenses/xz-embedded/COPYING"
        authors = "Lasse Collin and the XZ Embedded contributors"
        ident = add("source/third_party/" + vti_xz.ARCHIVE_NAME, decoder, vti_xz.UPSTREAM,
                    "source_archive", [], "0BSD", evidence, authors)
        source_ids.append(ident)
        with zipfile.ZipFile(io.BytesIO(decoder)) as archive:
            for name in ("COPYING", "AUTHORS"):
                location = f"licenses/xz-embedded/{name}"
                original_notice = archive.read(name)
                if (args.module.parent / location).read_bytes() != original_notice:
                    raise ValueError(f"Installed XZ Embedded notice differs from upstream: {name}")
                source_ids.append(add(location, original_notice, vti_xz.UPSTREAM, "notice", [ident],
                                      "0BSD", evidence, authors))
    if source_archives:
        for component, (version, expected, license_id, author, notices) in SOURCE_ARCHIVES[reader].items():
            stem = f"{component}-{version}"
            filename = stem + ("-decoder-sources" if reader == "avif" else "") + ".tar.gz"
            package = (source_archives / filename).read_bytes()
            if sha(package) != expected:
                raise ValueError(f"Unexpected source archive identity for {stem}")
            parents = ["licenses/" + component + "/" + name for name, _ in notices]
            evidence = ", ".join(parents)
            if component == "libjxl":
                evidence += ", archived third_party/CMakeLists.txt and lib/extras/LICENSE.apngdis, licenses/highway/LICENSE"
            archive_id = add(f"source/third_party/{filename}", package, filename,
                             "source_archive", [], license_id, evidence, author)
            source_ids.append(archive_id)
            with tarfile.open(fileobj=io.BytesIO(package), mode="r:gz") as archive:
                for filename, notice_license in notices:
                    location = f"licenses/{component}/{filename}"
                    with archive.extractfile(f"{stem}/{filename}") as member:
                        notice = member.read()
                    if (args.module.parent / location).read_bytes() != notice:
                        raise ValueError(f"Installed {location} differs from source")
                    source_ids.append(add(location, notice, f"Unmodified {stem} {filename}",
                                          "notice", [archive_id], notice_license, location, author))
    binary = add(args.module.name, module, f"Native qupathxx {label} module", parents=source_ids)
    add("reader-plugins.json", manifest, "Hash-bound native module manifest", "asset", [binary])
    source_hashes = {name: sha(data) for name, data in files.items() if name.startswith("source/")}
    add("SOURCE.json", encode({"module_sha256": digest, "source_files": source_hashes,
                              "validation_sha256": sha(args.validation.read_bytes()),
                              "profile": entry["profile"], "license": "AGPL-3.0-only"}),
        f"{label} package provenance", "asset", [binary])
    group["members"] = list(records.values())
    group["deliveries"][digest] = {"members": membership}
    value = encode(group).decode("utf-8").strip().replace("\n", "\n  ")
    match = re.search(r'^  "' + group_name + '": ', original, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(original[start:])
        updated = original[:start] + value + original[start + length:]
    else:
        updated = original[:original.rfind("}")].rstrip() + f',\n  "{group_name}": ' + value + "\n}\n"
    json.loads(updated)
    args.output.mkdir(parents=True)
    for name, data in files.items():
        destination = args.output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    inventory_path.write_text(updated, encoding="utf-8", newline="")
    admitted = read_inventory([inventory_path])
    check_stage(args.output, admitted)
    with zipfile.ZipFile(args.archive, "x", zipfile.ZIP_DEFLATED) as archive:
        for name in sorted(files):
            archive.write(args.output / name, name)
    check_archive(args.archive, admitted, expected=membership)
    print(f"Admitted {len(files)} {label} package and archive members")


def main(reader=None):
    parser = argparse.ArgumentParser(description=__doc__)
    if reader is None:
        parser.add_argument("reader", choices=("apng", "avif", "envi", "gif", "jpegxl", "metaimage", "openexr", "pcx",
                                              "rgbe", "tga", "vti", "webp-animation"))
    for name in ("module", "validation", "kit", "output", "archive"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--zlib-archive", type=Path)
    parser.add_argument("--tinyxml2-archive", type=Path)
    parser.add_argument("--jpegxl-archives", type=Path)
    parser.add_argument("--avif-archives", type=Path)
    parser.add_argument("--png-stage", type=Path)
    parser.add_argument("--webp-stage", type=Path)
    parser.add_argument("--vti-lz4-stage", type=Path)
    parser.add_argument("--vti-xz-archive", type=Path)
    args = parser.parse_args()
    selected = reader or args.reader
    if (selected == "apng") != bool(args.png_stage):
        parser.error("Only APNG uses and requires --png-stage")
    if (selected == "webp-animation") != bool(args.webp_stage):
        parser.error("Only animated WebP uses and requires --webp-stage")
    if (selected == "vti") != bool(args.vti_lz4_stage):
        parser.error("Only VTI uses and requires --vti-lz4-stage")
    if (selected == "vti") != bool(args.vti_xz_archive):
        parser.error("Only VTI uses and requires --vti-xz-archive")
    extra_sources = ()
    extra_product = ()
    dependencies = READER_DEPENDENCIES.get(selected, ())
    archives = {"zlib": args.zlib_archive, "tinyxml2": args.tinyxml2_archive}
    for component, path in archives.items():
        if (component in dependencies) != bool(path):
            parser.error(f"{selected} {'requires' if not path else 'does not use'} --{component}-archive")
    source_archives = {"jpegxl": args.jpegxl_archives, "avif": args.avif_archives}
    for provider, path in source_archives.items():
        if (selected == provider) != bool(path):
            parser.error(f"{selected} {'requires' if not path else 'does not use'} --{provider}-archives")
    if selected == "metaimage":
        extra_sources = ("plugins/metaimage/tests/CMakeLists.txt", "plugins/metaimage/tests/project_probe.cpp",
                         "plugins/metaimage/tests/write_simpleitk_fixtures.py")
    elif selected == "vti":
        extra_sources = ("plugins/vti/tests/CMakeLists.txt", "plugins/vti/tests/project_probe.cpp",
                         "plugins/vti/tests/write_vtk_fixtures.py", "tools/ci/prepare_vti_xz_sources.py")
        extra_product = ("plugins/image-common/gzip_member.hpp", "plugins/format-common/json_tree.hpp",
                         "plugins/format-common/reader_input.hpp")
    elif selected in ("apng", "envi", "gif", "rgbe", "webp-animation"):
        extra_sources = (f"plugins/{selected}/tests/CMakeLists.txt", f"plugins/{selected}/tests/project_probe.cpp")
        if selected == "envi":
            extra_sources += ("plugins/envi/tests/write_spectral_fixtures.py",)
            extra_product = ("plugins/format-common/json_tree.hpp", "plugins/format-common/reader_input.hpp")
    elif selected == "openexr":
        extra_sources = ("plugins/openexr/tests/CMakeLists.txt", "plugins/openexr/tests/project_probe.cpp")
        extra_product = ("plugins/image-common/gzip_member.hpp",)
    elif selected == "jpegxl":
        extra_sources = ("plugins/jpegxl/tests/CMakeLists.txt", "plugins/jpegxl/tests/project_probe.cpp",
                         "plugins/jpegxl/tests/fragment_reference.cpp")
    elif selected == "avif":
        extra_sources = ("plugins/avif/tests/CMakeLists.txt", "plugins/avif/tests/project_probe.cpp",
                         "tools/ci/prepare_avif_reader_sources.py")
    stage(args, selected, extra_sources, ((c, archives[c]) for c in dependencies), extra_product,
          source_archives.get(selected))


if __name__ == "__main__":
    main()
