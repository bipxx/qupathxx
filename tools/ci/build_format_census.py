"""Build the documentation-only FP00 planning census from retained evidence.

This does not register product readers, admit materials, choose runtime parsers
or implement an import authority. Native catalogs and source-scoped receipts
remain authoritative for actual build/support. All upstream rows are retained;
names/extensions are metadata, not promises of native or commercial eligibility.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
STATES = {"delivered", "reuse_first", "candidate", "contract_first", "unreviewed", "excluded"}
sha = lambda data: hashlib.sha256(data).hexdigest()


def key(name):
    return re.sub(r"[^a-z0-9]", "", name.casefold())


def classify(row):
    """Explicit name rules; extension collisions never select an implementation."""
    name = key(row["name"])
    if row["family"] == "openslide":
        return "W02" if name == "dicom" else "W01", "reuse_first", "openslide", (
            "Upstream backend listing only. Reconcile the actual SDK and a named brightfield input; "
            "no per-vendor/codec specimen qualification or multidimensional raw support inferred.")
    delivered = {
        "icsimagecytometrystandard": ("I15", "org.qupathxx.ics", "ics-raw-channel-window/1; selected ICS1/IDS and inlineICS2 uncompressed scalar/channel-first XY/XYZ UInt8/UInt16/Int16/Float32, guarded companions, exact bits/C/Z and explicit display options; no general acquisition/compression claim"),
        "nrrdnearlyrawrasterdata": ("I16", "org.qupathxx.nrrd", "nrrd5-raw-scalar-window/1; selected attached raw NRRD0005 scalar XY/XYZ UInt8/UInt16/Int16/Float32, exact stored bits/Z and explicit display window; no general affine/detached/compressed claim"),
        "windowsbitmap": ("I04", "org.qupathxx.bmp", "bmp-infoheader-uint8-samples/1; selected BM/40-byte BI_RGB1/4/8/24/32, literal palette indices/RGB and per-axis ppm; X is unused, other DIB/compression/alpha profiles refuse"),
        "portableanymap": ("I03", "org.qupathxx.pnm", "pnm-pam-uint-samples/1; selected single-image P1–P7 UInt8/UInt16 tuples and explicit transfer options; built-in P6 behavior stays separate"),
        "jpeg": ("I02", "org.qupathxx.jpeg", "jpeg8-oriented-decoded-samples/1; 8-bit Huffman Gray/RGB/YCbCr, strict metadata profile"),
        "pngportablenetworkgraphics": ("I01", "org.qupathxx.png", "png-static-native-samples/1; declared static color/depth/Adam7 profile"),
        "ometiff": ("I13", "tiff", "Existing quantitative TIFF/OME-TIFF profile only; no arbitrary OME-XML or TIFF encoding claim"),
        "tifftaggedimagefileformat": ("I13", "tiff", "Existing declared quantitative layouts/dtypes/axes/codecs only; not every TIFF dialect"),
    }
    if name in delivered:
        plan, provider, note = delivered[name]
        return plan, "delivered", provider, note
    scanners = {"aperiosvstiff", "hamamatsundpi", "hamamatsuvms", "leicascn", "trestle", "ventanabif"}
    if name in scanners:
        return "W01", "reuse_first", "openslide", "Existing SDK route first; exact scanner variant, codec, companions and source rights still need input-specific evidence."
    candidates = {
        "jpeg2000": ("I06", "Standalone JP2/J2K candidate, not qualified by an embedded OpenSlide codec; resolve native dependency maintenance first."),
        "fitsflexibleimagetransportsystem": ("I18", "Native FITS subset candidate; HDU, stored/physical values and coordinate meaning must be explicit."),
    }
    if name in candidates:
        plan, note = candidates[name]
        return plan, "candidate", None, note
    contracts = {
        "bigdataviewer": ("I24", "Store/large-file inventory and spatial transform contracts precede independent native provider."),
        "nifti": ("I17", "General affine/voxel interpretation needs a native contract; an explicit representable slice subset may use v1."),
        "analyze75": ("I17", "Exact dtype, paired source identity and spatial interpretation need a selected native profile."),
        "dicom": ("W02", "Existing WSI route is distinct from general medical series, SEG and SR; no blanket DICOM support."),
        "omeropyramid": ("X04", "Remote/source authority required; not a local image-decoder claim."),
    }
    if name in contracts:
        plan, note = contracts[name]
        return plan, "contract_first", "openslide" if name == "dicom" else None, note
    if name in {"animatedpng", "gifgraphicsinterchangeformat", "mngmultipleimagenetworkgraphics"}:
        return "I07", "unreviewed", None, "Static versus animated semantics, native decoder/terms and view/timing contract are unresolved; static PNG does not admit APNG."
    if name in {"pcxpcpaintbrush", "targa"}:
        return "I08", "unreviewed", None, "Exact native raster/schema/codec/fixture profile has not been admitted."
    if name == "micro-manager" or name in {"micromanager", "cellvoyager", "cv7000"}:
        return "I26", "unreviewed", None, "Collection schema, metadata/registration and complete file-set identity are not established by decoding individual TIFF files."
    if name == "cellh5":
        return "I25", "unreviewed", None, "Native HDF5 availability does not admit this inner image schema, filters, axes or fixtures."
    if name == "mrcmedicalresearchcouncil":
        return "I19", "unreviewed", None, "MRC axis/origin/type/version and native/fixture terms need a selected profile."
    vendors = {"bitplaneimaris", "cellsensvsi", "zeissczi", "nikonniselementsnd2", "leicalasaflifleicaimagefileformat", "leicalof", "leicaxlef", "olympusoir", "olympusfluoviewfv1000"}
    if name in vendors:
        return "W04", "unreviewed", None, "New fluorescence/omics requires reusable open inner-format terms in addition to an open native reader. Exclude a proposed closed/restricted workflow; no assumption that all vendor schemas are closed or admitted."
    if "tiff" in name or name in {"metamorphstackstk", "zeisslsmlaserscanningmicroscope510710"}:
        return "I14", "unreviewed", "tiff", "Pixel storage may reuse TIFF only within its existing generic profile; this dialect's channels/planes/calibration/schema and complete source set are unreviewed."
    other_domains = {"aviaudiovideointerleave", "quicktimemovie", "epsencapsulatedpostscript", "canondng", "nefnikonelectronicformat", "minoltamrw", "adobephotoshoppsd", "pictmacintoshpicture", "text", "jpx"}
    if name in other_domains:
        return "X05", "unreviewed", None, "Native decoder/schema/terms and a representable research projection are unresolved; no runtime fallback or support inferred from a suffix."
    return "W05", "unreviewed", None, "Complete census retention only: native implementation, reusable schema, required codec/fixture rights and host representation need individual investigation. Bio-Formats Java availability does not establish a native port."


def roadmap_rows(path):
    result = []
    for line in path.read_text(encoding="utf8").splitlines():
        if not re.match(r"^\| [IWMAX]\d{2} \|", line):
            continue
        parts = [part.strip() for part in line.strip("|").split("|")]
        result.append({"plan_id": parts[0], "profile_group": parts[1], "planning_disposition": parts[2],
                       "contract_notes": parts[3] if len(parts) > 3 else "See scoped roadmap packet and per-format card"})
    assert len(result) == len({row["plan_id"] for row in result}) == 57
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True)
    parser.add_argument("--runtime", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    upstream_bytes = args.upstream.read_bytes()
    upstream = json.loads(upstream_bytes)
    assert upstream["schema"] == "qpx.upstream-format-census/1" and upstream["checks"]["all_source_rows_retained"]
    assert not upstream["checks"]["duplicate_format_rows"]
    runtime_bytes = args.runtime.read_bytes()
    runtime = json.loads(runtime_bytes)
    assert runtime["schema"] == "qpx.format-runtime-census/1"
    groups = roadmap_rows(ROOT / "docs/format-plugin-coverage-plan.md")
    group_ids = {row["plan_id"] for row in groups}
    linked_units = {row["backend"]: row for row in runtime["openslide"]["linked_source_units"]}
    entries, collisions = [], defaultdict(list)
    for original in upstream["formats"]:
        plan, state, provider, note = classify(original)
        assert plan in group_ids and state in STATES
        ident = original["family"] + ":" + key(original["name"])
        row = {"id": ident, **original, "plan_id": plan, "disposition": state,
               "existing_provider_route": provider, "scope_and_missing_evidence": note,
               "rights": {"census_grants_implementation_admission": False, "census_grants_fixture_admission": False,
                          "schema_native_codec_fixture_review": "Existing source-scoped profile only" if state == "delivered" else "Unreviewed for any new implementation/workflow"},
               "upstream_listing_is_support_evidence": False}
        if original["family"] == "openslide":
            backend = original["upstream_backend"]
            unit = linked_units.get({"philips": "philips-tiff"}.get(backend, backend))
            row["runtime_backend_evidence"] = {
                "actual_api_version": runtime["openslide"]["actual_api_version"],
                "source_unit": unit["source_unit"] if unit else None,
                "listed_in_matching_source_build": bool(unit and unit["in_actual_source_build_list"]),
                "linked_in_crc_bound_debug_names": bool(unit and unit["in_crc_bound_debug_names"]),
                "qualified_by_census": False,
                "existing_fixture_scope": unit.get("existing_fixture_evidence") if unit else None}
        entries.append(row)
        for extension in original["extensions"]:
            collisions[extension.casefold()].append(ident)
    assert len(entries) == len({row["id"] for row in entries}) == upstream["checks"]["format_count"]
    groups = [{**row, "upstream_entries": [item["id"] for item in entries if item["plan_id"] == row["plan_id"]]} for row in groups]
    result = {"schema": "qpx.format-planning-census/1", "census_date": max(row["fetched_at"][:10] for row in upstream["sources"]),
              "authority": "Documentation-only planning metadata; native runtime catalogs and exact receipts own actual support and existing material inventories own admission.",
              "upstream_sources": upstream["sources"], "upstream_inventory_sha256": sha(upstream_bytes),
              "runtime_evidence": {"path": args.runtime.as_posix(), "sha256": sha(runtime_bytes), "schema": runtime.get("schema"),
                                   "claim_limit": "See exact runtime evidence; dependency/backend tokens are not per-format specimen qualification."},
              "native_runtime": {"openslide_api_version": runtime["openslide"]["actual_api_version"],
                                 "openslide_dll_sha256": runtime["openslide"]["dll_sha256"],
                                 "linked_source_units": len(linked_units), "all_vendors_qualified": False},
              "entries": entries, "roadmap_groups": groups,
              "policy_exclusions": [
                  "Java/Groovy runtime and QuPath Java-extension compatibility",
                  "Closed/proprietary/noncommercial/research-only required implementation, codec or material",
                  "New fluorescence/spatial-omics workflow whose inner format fails the reusable open-format rule"],
              "checks": {"all_upstream_entries_classified": True, "count": len(entries), "roadmap_group_count": len(groups),
                         "counts_by_upstream": dict(sorted(Counter(row["family"] for row in entries).items())),
                         "counts_by_disposition": dict(sorted(Counter(row["disposition"] for row in entries).items())),
                         "extension_alias_count": sum(len(row["extensions"]) for row in entries),
                         "unique_casefolded_extensions": len(collisions),
                         "shared_extensions": {name: values for name, values in sorted(collisions.items()) if len(values) > 1},
                         "unexpanded_extension_notations": upstream["checks"]["unexpanded_extension_notations"],
                         "classification_uses_extensions": False, "material_admission_changed": False}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True) + "\n", encoding="utf8", newline="")
    print(json.dumps({name: result["checks"][name] for name in ("count", "roadmap_group_count", "counts_by_upstream", "counts_by_disposition", "extension_alias_count", "unique_casefolded_extensions")}))


if __name__ == "__main__":
    main()
