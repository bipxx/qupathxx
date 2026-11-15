"""Development-only authored fixtures and native typed-family validation control.

No product parser, importer, geometry, publication or identity logic lives here.
Literal integer/coordinate arrays are the independent expected results. All
interpretation and persistence checks execute C++ providers/services/consumers.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

FEATURES = ["0002", "2", "zero-only"]
OBSERVATIONS = ["0001", "1", "18446744073709551615"]
VALUES = [18446744073709551615, 9007199254740993, 0, 0, 7, 0, 11, 0, 0]
POINTS = [
    {"id": "0001", "x": 1.25, "y": 2.5, "c": -1, "z": 0, "t": 0, "class": "T"},
    {"id": "1", "x": 2.25, "y": 3.5, "c": 0, "z": 0, "t": 0, "class": ""},
    {"id": "18446744073709551615", "x": 3.25, "y": 4.5, "c": -1, "z": 0, "t": 0, "class": "S"},
]
MATRIX_BAD = ["matrix-type", "matrix-zero", "matrix-huge", "matrix-reserved", "matrix-mutate",
              "axis-short", "axis-long", "axis-index", "axis-offset", "axis-empty-id", "axis-utf8", "axis-c1",
              "axis-nul", "axis-text-long", "axis-mutate", "values-short", "values-long", "values-mutate", "values-io"]
POINT_BAD = ["point-frame", "point-meta-reserved", "point-meta-mutate", "points-short",
             "points-long", "points-mutate", "points-text-long", "points-offset", "points-empty-id",
             "points-utf8", "points-c1", "points-nul", "points-nan", "points-inf", "points-c", "points-z", "points-t",
             "points-reserved", "points-io"]


def sha(path: Path) -> str:
    return hashlib.file_digest(path.open("rb"), "sha256").hexdigest()


def save(path: Path, value) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def quantitative_tiff() -> bytes:
    """Authored classic TIFF, uncompressed UInt8 RGB3, one contiguous strip.

    This fixture encoder uses no codec/library; the C++ native TIFF authority
    must prove raw metadata and all literal samples before annotation capture.
    """
    count = 12
    bits = 8 + 2 + count * 12 + 4
    formats = bits + 6
    pixels = formats + 6
    fields = [(256, 4, 1, 16), (257, 4, 1, 16), (258, 3, 3, bits),
              (259, 3, 1, 1), (262, 3, 1, 2), (273, 4, 1, pixels),
              (274, 3, 1, 1), (277, 3, 1, 3), (278, 4, 1, 16),
              (279, 4, 1, 768), (284, 3, 1, 1), (339, 3, 3, formats)]
    return (b"II" + struct.pack("<HIH", 42, 8, count)
            + b"".join(struct.pack("<HHII", *field) for field in fields)
            + struct.pack("<IHHHHHH", 0, 8, 8, 8, 1, 1, 1) + bytes([80]) * 768)


def fixtures(root: Path) -> dict:
    root.mkdir(parents=True, exist_ok=False)
    report = {"schema": "qpx.format-family-fixtures/1", "authority": "repository-authored literal UInt64 and point arrays", "positive": [], "negative": []}

    def add(domain, name, data, expected=None, status=None):
        path = root / name
        path.write_bytes(data.encode("utf-8") if isinstance(data, str) else data)
        record = {"domain": domain, "file": name, "sha256": sha(path)}
        if status is None:
            record.update(expected)
            report["positive"].append(record)
        else:
            record["status"] = status
            report["negative"].append(record)

    counts = "observation_id\t0002\t2\tzero-only\n0001\t18446744073709551615\t9007199254740993\t0\n1\t0\t7\t0\n18446744073709551615\t11\t0\t0\n"
    ce = {"features": FEATURES, "observations": OBSERVATIONS, "values": VALUES}
    for name, text in [("counts-main.tsv", counts), ("counts-crlf.tsv", counts.replace("\n", "\r\n")),
                       ("counts-no-lf.tsv", counts.rstrip("\n")), ("counts-leading-digits.tsv", counts.replace("\t7\t", "\t0007\t")),
                       ("counts-μ-图.tsv", counts), ("counts-mixed.tsv", counts.replace("\n", "\r\n", 1))]:
        add("matrix", name, text, ce)
    add("matrix", "counts-literal-ids.tsv", 'observation_id\t"gene"\t gene \n"0001"\t0\t18446744073709551615',
        {"features": ['"gene"', ' gene '], "observations": ['"0001"'], "values": [0, 18446744073709551615]})
    add("matrix", "counts-utf8.tsv", "observation_id\tμ\t图\n样本\t9007199254740993\t0\n",
        {"features": ["μ", "图"], "observations": ["样本"], "values": [9007199254740993, 0]})
    add("matrix", "counts-token-bound.tsv", "observation_id\t" + "f" * 4096 + "\n" + "o" * 4096 + "\t1\n",
        {"features": ["f" * 4096], "observations": ["o" * 4096], "values": [1]})
    points = "id\tx\ty\tc\tz\tt\tclass\n0001\t1.25\t2.5\t-1\t0\t0\tT\n1\t2.25\t3.5\t0\t0\t0\t\n18446744073709551615\t3.25\t4.5\t-1\t0\t0\tS\n"
    for name, text in [("points-main.tsv", points), ("points-crlf.tsv", points.replace("\n", "\r\n")),
                       ("points-no-lf.tsv", points.rstrip("\n")), ("points-μ-图.tsv", points),
                       ("points-exponent.tsv", points.replace("1.25\t", "1.25e0\t", 1))]:
        add("annotation", name, text, {"points": POINTS})
    add("annotation", "points-empty-final-class.tsv", "id\tx\ty\tc\tz\tt\tclass\n0001\t1.25\t2.5\t-1\t0\t0\t",
        {"points": [{**POINTS[0], "class": ""}]})
    add("annotation", "points-outside.tsv", "id\tx\ty\tc\tz\tt\tclass\noutside\t1000.25\t2000.5\t-1\t0\t0\t\n",
        {"points": [{"id": "outside", "x": 1000.25, "y": 2000.5, "c": -1, "z": 0, "t": 0, "class": ""}]})
    add("annotation", "points-utf8.tsv", 'id\tx\ty\tc\tz\tt\tclass\n"μ 图"\t0\t0\t-1\t0\t0\t样本',
        {"points": [{"id": '"μ 图"', "x": 0, "y": 0, "c": -1, "z": 0, "t": 0, "class": "样本"}]})
    add("annotation", "points-token-bound.tsv", "id\tx\ty\tc\tz\tt\tclass\n" + "P" * 4096 + "\t1\t2\t-1\t0\t0\t\n",
        {"points": [{"id": "P" * 4096, "x": 1, "y": 2, "c": -1, "z": 0, "t": 0, "class": ""}]})
    matrix_negative = {
        "wrong-header": ("Observation_id\tG\nA\t1\n", 2), "empty": ("", 2),
        "no-features": ("observation_id\nA\n", 4), "empty-feature": ("observation_id\t\nA\t1\n", 4),
        "duplicate-feature": ("observation_id\tG\tG\nA\t1\t2\n", 4), "no-observations": ("observation_id\tG\n", 4),
        "duplicate-observation": ("observation_id\tG\nA\t1\nA\t2\n", 4), "empty-observation": ("observation_id\tG\n\t1\n", 4),
        "missing-cell": ("observation_id\tG\tH\nA\t1\n", 4), "extra-cell": ("observation_id\tG\nA\t1\t2\n", 4),
        "blank-row": (counts + "\n", 4), "bare-cr": (counts.replace("\n", "\r"), 4),
        "nul": (counts.replace("0001", "000\0", 1), 4), "control": (counts.replace("0001", "000\x01", 1), 4),
        "c1": (counts.replace("0001", "000\x85", 1), 4), "bad-utf8": (counts.encode().replace(b"0001", b"\xc0\xaf01", 1), 4),
        "token-over": ("observation_id\tG\n" + "o" * 4097 + "\t1\n", 4),
        "header-over": ("observation_id\t" + "\t".join(f"{i:04}" + "x" * 4092 for i in range(17))
                        + "\nA\t" + "\t".join(["1"] * 17) + "\n", 4),
    }
    for label, cell in [("negative", "-1"), ("plus", "+1"), ("float", "1.0"), ("exponent", "1e0"),
                        ("overflow", "18446744073709551616"), ("empty-cell", ""), ("space", " 1")]:
        matrix_negative[label] = ("observation_id\tG\nA\t" + cell + "\n", 4)
    point_negative = {"wrong-header": (points.replace("class\n", "Class\n", 1), 2), "empty": ("", 2),
                      "no-points": ("id\tx\ty\tc\tz\tt\tclass\n", 4), "blank-row": (points + "\n", 4),
                      "duplicate-id": (points.replace("\n1\t", "\n0001\t"), 4),
                      "bad-utf8": (points.encode().replace(b"0001", b"\xc0\xaf01", 1), 4),
                      "nul": (points.replace("0001", "000\0", 1), 4), "control": (points.replace("0001", "000\x01", 1), 4),
                      "c1": (points.replace("0001", "000\x85", 1), 4), "bare-cr": (points.replace("\n", "\r"), 4),
                      "token-over": (points.replace("0001", "x" * 4097, 1), 4),
                      "missing-cell": ("id\tx\ty\tc\tz\tt\tclass\nA\t1\t2\t-1\t0\t0\n", 4),
                      "extra-cell": (points.replace("\tT\n", "\tT\tmore\n", 1), 4)}
    for label, field, value in [("negative-x", 1, "-1"), ("nan", 1, "nan"), ("inf", 2, "inf"), ("hex", 1, "0x1p0"),
                                ("plus", 1, "+1"), ("space", 1, " 1"), ("bad-c", 3, "-2"),
                                ("large-c", 3, "2147483648"), ("wild-z", 4, "-1"), ("wild-t", 5, "-1"),
                                ("fraction-z", 4, "0.5"), ("empty-id", 0, "")]:
        row = ["A", "1", "2", "-1", "0", "0", "T"]
        row[field] = value
        point_negative[label] = ("id\tx\ty\tc\tz\tt\tclass\n" + "\t".join(row) + "\n", 4)
    for domain, negatives in [("matrix", matrix_negative), ("annotation", point_negative)]:
        for label, (data, status) in negatives.items():
            add(domain, f"bad-{domain}-{label}.tsv", data, status=status)
    (root / "positions.tsv").write_text("observation_id\tx_px\ty_px\n18446744073709551615\t30.25\t40.5\n0001\t10.25\t20.5\n1\t20.25\t30.5\n", encoding="utf-8")
    for name, text in {
        "positions-missing.tsv": "observation_id\tx_px\ty_px\n0001\t10.25\t20.5\n1\t20.25\t30.5\n",
        "positions-extra.tsv": "observation_id\tx_px\ty_px\n0001\t10.25\t20.5\n1\t20.25\t30.5\n18446744073709551615\t30.25\t40.5\nextra\t0\t0\n",
        "positions-duplicate.tsv": "observation_id\tx_px\ty_px\n0001\t10.25\t20.5\n1\t20.25\t30.5\n1\t30.25\t40.5\n",
    }.items():
        (root / name).write_text(text, encoding="utf-8")
    # A normally valid source whose typed expansion, not a file-format error,
    # must exceed a deliberately small caller allowance.
    (root / "budget-counts.tsv").write_text("observation_id\tG\n" + "".join(f"O{i}\t1\n" for i in range(512)), encoding="utf-8")
    (root / "budget-points.tsv").write_text("id\tx\ty\tc\tz\tt\tclass\n" + "".join(f"P{i}\t1\t2\t-1\t0\t0\t\n" for i in range(512)), encoding="utf-8")
    (root / "positions-one.tsv").write_text("observation_id\tx_px\ty_px\nA\t1\t2\n", encoding="utf-8")
    for size in [256, 257]:
        (root / f"counts-native-id{size}.tsv").write_text("observation_id\t" + "F" * size + "\nA\t1\n", encoding="utf-8")
        (root / f"points-native-id{size}.tsv").write_text("id\tx\ty\tc\tz\tt\tclass\n" + "P" * size + "\t1\t2\t-1\t0\t0\t\n", encoding="utf-8")
    (root / "image.ppm").write_bytes(b"P6\n16 16\n255\n" + bytes([80] * (16 * 16 * 3)))
    (root / "image.tif").write_bytes(quantitative_tiff())
    report["annotation_target"] = {"file": "image.tif", "encoding": "authored-classic-tiff-uncompressed-contiguous",
                                   "width": 16, "height": 16, "channels": 3, "dtype": "UInt8", "literal_sample": 80,
                                   "role": "Native quantitative target; C=-1/C0 planes retained; not a TSV/provider dependency"}
    report["counts"] = {"positive": len(report["positive"]), "negative": len(report["negative"])}
    report["files"] = {p.name: sha(p) for p in sorted(root.iterdir()) if p.is_file()}
    save(root / "fixtures.json", report)
    return report


def run(command, out: Path) -> dict:
    result = subprocess.run([str(x) for x in command], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, encoding="utf-8", errors="strict", check=False)
    out.write_text(result.stdout, encoding="utf-8")
    return {"argv": [str(x) for x in command], "exit": result.returncode, "log": str(out), "log_sha256": sha(out)}


def changed_module_manifest(original: Path, output: Path, name="changed-modules", mutate=True, bad_hash=False) -> Path:
    """Transport-only changed-PE control; native code checks identity semantics."""
    directory = output / name
    directory.mkdir()
    document = json.loads(original.read_text(encoding="utf-8"))
    for provider in document["plugins"]:
        for entry in [provider["module"], *provider["dependencies"]]:
            source = Path(entry["path"])
            if not source.is_absolute():
                source = original.parent / source
            destination = directory / source.name
            if not destination.exists():
                shutil.copyfile(source, destination)
                if mutate and entry is provider["module"]:
                    with destination.open("ab") as stream:
                        stream.write(b"\nRepository-authored same-version implementation identity control\n")
            entry["path"] = destination.name
            entry["sha256"] = sha(destination)
        if bad_hash and provider["domain"] == "matrix":
            provider["module"]["sha256"] = "0" * 64
    target = directory / "plugins.json"
    save(target, document)
    return target


def validate(args) -> None:
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    fixture_dir = output / "fixtures-μ-图"
    authored = fixtures(fixture_dir)
    artifacts = {name: getattr(args, name).resolve() for name in
                 ["matrix_module", "annotation_module", "abi_test", "import_test", "manifest", "bad_manifest"]}
    if args.consumer:
        artifacts["consumer"] = args.consumer.resolve()
    if args.cli:
        artifacts["cli"] = args.cli.resolve()
    for label in ["manifest", "bad_manifest"]:
        manifest = artifacts[label]
        for i, provider in enumerate(json.loads(manifest.read_text(encoding="utf-8"))["plugins"]):
            for entry in [provider["module"], *provider["dependencies"]]:
                path = Path(entry["path"])
                if not path.is_absolute():
                    path = manifest.parent / path
                artifacts[f"{label}_{i}_{path.name}"] = path.resolve()
    before = {name: {"path": str(path), "sha256": sha(path)} for name, path in artifacts.items()}
    result = {"schema": "qpx.format-family-validation/1", "artifacts_before": before,
              "fixtures_sha256": sha(fixture_dir / "fixtures.json"), "counts": authored["counts"], "commands": []}
    def execute(label, command, expected=0):
        record = run(command, output / (label + ".log"))
        record["expected_exit"] = expected
        result["commands"].append(record)
        if record["exit"] != expected:
            raise RuntimeError(f"Native validation failed ({record['exit']}, expected {expected}); retained {record['log']}:\n"
                               + Path(record["log"]).read_text(encoding="utf-8")[-4000:])
        return record
    def report(label, command, expected=0):
        record = execute(label, command, expected)
        return json.loads(Path(record["log"]).read_text(encoding="utf-8"))
    def capture_generated(manifest, prefix):
        for path in [manifest, *sorted(manifest.parent.glob("*.dll"))]:
            name = prefix + "_" + path.name
            artifacts[name] = path
            before[name] = {"path": str(path), "sha256": sha(path)}
    try:
        execute("direct-abi", [args.abi_test, "--abi", args.matrix_module, args.annotation_module, fixture_dir])
        execute("host-catalog-default", [args.abi_test, "--host-case", "catalog-default", "-", fixture_dir])
        for case in ["catalog", "matrix-control", "annotation-control", "receipt", "source-change", "cancel", "ranges"]:
            execute("host-" + case, [args.abi_test, "--host-case", case, args.manifest, fixture_dir])
        for case in MATRIX_BAD + POINT_BAD:
            execute("host-" + case, [args.abi_test, "--host-case", case, args.bad_manifest, fixture_dir])
        for case in ["matrix", "annotation", "annotation-stale", "annotation-cancel", "positions", "limits", "legacy", "native-id-limits"]:
            work = output / case
            work.mkdir()
            execute("import-" + case, [args.import_test, case, args.manifest, fixture_dir, work])
        for case in ["axis-duplicate", "observation-name", "points-duplicate", "point-huge", "mutate-source", "wait-values", "wait-points"]:
            work = output / case
            work.mkdir()
            execute("boundary-" + case, [args.import_test, case, args.bad_manifest, fixture_dir, work])
        if args.consumer:
            work = output / "sdk"
            work.mkdir()
            execute("sdk-create", [args.consumer, "create", args.manifest, fixture_dir, work])
            execute("sdk-reopen", [args.consumer, "reopen", args.manifest, fixture_dir, work])
            execute("sdk-offline", [args.consumer, "offline", "-", fixture_dir, work])
            changed = changed_module_manifest(args.manifest.resolve(), output)
            result["changed_module_manifest"] = {"path": str(changed), "sha256": sha(changed)}
            capture_generated(changed, "changed")
            execute("sdk-changed-implementation", [args.consumer, "changed-implementation", changed, fixture_dir, work])
            execute("sdk-changed-source", [args.consumer, "changed-source", args.manifest, fixture_dir, work])
            if args.cli:
                default = report("cli-default-catalog", [args.cli, "catalog"])
                assert default == {"schema": "qpx.format-plugin-catalog/1", "providers": [], "diagnostics": []}, default
                bootstrap = [args.cli, "--format-plugins", args.manifest.resolve()]
                catalog = report("cli-configured-catalog", [*bootstrap, "catalog"])
                assert catalog["schema"] == "qpx.format-plugin-catalog/1" and catalog["diagnostics"] == [], catalog
                providers = {p["provider_id"]: p for p in catalog["providers"]}
                assert set(providers) == {"org.qupathxx.counts-tsv", "org.qupathxx.point-tsv"}, catalog
                for provider, domain, format_id, display, contract, module in [
                    ("org.qupathxx.counts-tsv", "matrix", "counts-tsv", "Dense counts TSV", "dense-observation-feature-uint64-tsv/1", args.matrix_module),
                    ("org.qupathxx.point-tsv", "annotation", "point-tsv", "Pixel point TSV", "pixel-point-annotation-tsv/1", args.annotation_module),
                ]:
                    p = providers[provider]
                    assert (p["domain"], p["format_id"], p["display_name"], p["contract"], p["version"], p["abi"], p["profile"], p["default_options"]) == (
                        domain, format_id, display, contract, "1.0.0", 1, "windows-x64-msvc1944-release-md", ""), p
                    assert p["module_sha256"] == sha(module) and len(p["implementation_sha256"]) == 64, p
                bad_hash = changed_module_manifest(args.manifest.resolve(), output, "bad-hash", mutate=False, bad_hash=True)
                capture_generated(bad_hash, "bad_hash")
                bad_catalog = report("cli-bad-hash-catalog", [args.cli, "--format-plugins", bad_hash, "catalog"])
                assert [p["provider_id"] for p in bad_catalog["providers"]] == ["org.qupathxx.point-tsv"], bad_catalog
                assert any("module/dependency SHA-256 mismatch" in x for x in bad_catalog["diagnostics"]), bad_catalog
                counts, positions, image, points = [fixture_dir / name for name in ["counts-main.tsv", "positions.tsv", "image.ppm", "points-main.tsv"]]
                indexed = report("cli-matrix-index", [*bootstrap, "matrix-index", "org.qupathxx.counts-tsv", counts, positions, image, work / "cli-counts.qmx"])
                assert (indexed["schema"], indexed["state"], indexed["features"], indexed["observations"], indexed["entries"]) == ("qpx.format-import-cli/1", "indexed", 3, 3, 9), indexed
                assert indexed["file_sha256"] == sha(work / "cli-counts.qmx") and indexed["positions_sha256"] == sha(positions), indexed
                assert indexed["position_columns"] == ["observation_id", "x_px", "y_px"] and indexed["receipt"]["source_sha256"] == sha(counts), indexed
                original_project = sha(work / "points.qpproj")
                class_map = json.dumps({"T": "Native Tumor", "S": "Native Stroma"}, separators=(",", ":"))
                saved = report("cli-points-import", [*bootstrap, "points-import", "org.qupathxx.point-tsv", work / "points.qpproj", "1", points, work / "cli-points.qpproj", "--class-map-json", class_map])
                assert (saved["schema"], saved["state"], saved["image_id"]) == ("qpx.format-import-cli/1", "saved", 1), saved
                assert saved["objects"] == [{"source_id": source_id, "object_id": i + 5} for i, source_id in enumerate(OBSERVATIONS)], saved
                assert saved["receipt"]["source_sha256"] == sha(points) and sha(work / "points.qpproj") == original_project, saved
                execute("sdk-cli-reopen", [args.consumer, "cli-reopen", args.manifest, fixture_dir, work])
                for label, command, unavailable, diagnostic in [
                    ("bad-hash", [args.cli, "--format-plugins", bad_hash, "matrix-index", "org.qupathxx.counts-tsv", counts, positions, image, work / "bad-hash-never.qmx"], "org.qupathxx.counts-tsv", "module/dependency SHA-256 mismatch"),
                    ("matrix-wrong-domain", [*bootstrap, "matrix-index", "org.qupathxx.point-tsv", counts, positions, image, work / "matrix-wrong-domain-never.qmx"], "org.qupathxx.point-tsv", None),
                    ("points-wrong-domain", [*bootstrap, "points-import", "org.qupathxx.counts-tsv", work / "points.qpproj", "1", points, work / "points-wrong-domain-never.qpproj", "--class-map-json", class_map], "org.qupathxx.counts-tsv", None),
                ]:
                    error = report("cli-" + label, command, expected=2)
                    assert error["schema"] == "qpx.format-import-error/1" and ("Format plugin provider unavailable: " + unavailable) in error["error"], error
                    if diagnostic:
                        assert any(diagnostic in x for x in error["diagnostics"]), error
                assert not any(work.glob("*-never.*")), "Refused CLI command published output"
                assert sha(work / "points.qpproj") == original_project, "Refused CLI command changed input project"
        result["passed"] = True
    finally:
        after = {name: {"path": str(path), "sha256": sha(path)} for name, path in artifacts.items()}
        result["artifacts_after"] = after
        result["artifacts_unchanged"] = before == after
        result["fixtures_unchanged"] = (sha(fixture_dir / "fixtures.json") == result["fixtures_sha256"]
                                        and all(sha(fixture_dir / name) == digest for name, digest in authored["files"].items()))
        save(output / "validation.json", result)
        if not result["artifacts_unchanged"] or not result["fixtures_unchanged"]:
            raise RuntimeError("Validation inputs changed; see validation.json")
    print(json.dumps({"output": str(output), "passed": True, "commands": len(result["commands"]), **authored["counts"]}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    sub.add_parser("fixtures").add_argument("output", type=Path)
    check = sub.add_parser("validate")
    for flag in ["matrix-module", "annotation-module", "abi-test", "import-test", "manifest", "bad-manifest", "output"]:
        check.add_argument("--" + flag, type=Path, required=True)
    check.add_argument("--consumer", type=Path)
    check.add_argument("--cli", type=Path, help="Optional native format-import CLI; requires --consumer")
    args = parser.parse_args()
    # Windows LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR requires an absolute module
    # path. Bind both evidence and command arguments to the same resolved paths.
    for name, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, name, value.resolve())
    if args.mode == "fixtures":
        print(json.dumps(fixtures(args.output.resolve())["counts"]))
    else:
        if args.cli and not args.consumer:
            parser.error("--cli requires --consumer to check persisted CLI results through the installed SDK")
        validate(args)


if __name__ == "__main__":
    main()
