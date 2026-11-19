"""Independent authored geometry fixtures and native interoperability checks.

Development control/oracles only. Native services own all product interpretation,
identity, validation and publication. No third-party assets or Python fallback.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import os
import shutil
import subprocess
from check_format_family_plugins import quantitative_tiff

HEADER = "id\tkind\tgeometry\tc\tz\tt\tclass\n"


def qro(kind, value):
    """Literal QRO1 oracle, independent of the production TSV scanner/encoder."""
    prefix = b"QRO\x01" + bytes([kind])
    ring = lambda points: struct.pack("<I", len(points)) + b"".join(struct.pack("<dd", *p) for p in points)
    if kind < 2:
        return prefix + struct.pack("<dddd", *value)
    if kind >= 4:
        return prefix + ring(value)
    return prefix + struct.pack("<I", len(value)) + b"".join(
        ring(shell) + struct.pack("<I", len(holes)) + b"".join(ring(hole) for hole in holes)
        for shell, holes in value)


MAIN = [
    ("0001", "rectangle", "1.25,2.5,4,3", -1, 0, 0, "T", qro(0, (1.25, 2.5, 4, 3))),
    ("1", "ellipse", "8,1,4,5", 0, 0, 0, "S", qro(1, (8, 1, 4, 5))),
    ("18446744073709551615", "polyline", "1,10;4,12;7,11", -1, 0, 0, "", qro(4, [(1, 10), (4, 12), (7, 11)])),
    ("μ 图", "points", "10,10;12,12;15,15", 0, 0, 0, "T", qro(5, [(10, 10), (12, 12), (15, 15)])),
    ("hole|shape", "polygon", "1,1;7,1;7,7;1,7|2,2;2,3;3,3;3,2", -1, 0, 0, "S",
     qro(2, [([(1, 1), (7, 1), (7, 7), (1, 7)], [[(2, 2), (2, 3), (3, 3), (3, 2)]])])),
    ("multi/shape", "multipolygon", "9,8;11,8;11,10;9,10/12,8;14,8;14,10;12,10", -1, 0, 0, "",
     qro(3, [([(9, 8), (11, 8), (11, 10), (9, 10)], []), ([(12, 8), (14, 8), (14, 10), (12, 10)], [])])),
]


def row_text(row):
    return "\t".join(map(str, row[:7])) + "\n"


def fixtures(root):
    root.mkdir(parents=True, exist_ok=False)
    report = {"schema": "qpx.annotation-geometry-fixtures/1", "positive": [], "negative": [], "native_negative": []}
    image = quantitative_tiff()
    (root / "image.tif").write_bytes(image)
    report["target"] = {"file": "image.tif", "sha256": hashlib.sha256(image).hexdigest(),
                        "authority": "existing repository-authored 16x16 UInt8 RGB TIFF, all raw codes80"}

    def positive(name, rows, transform=lambda text: text):
        text = transform(HEADER + "".join(map(row_text, rows)))
        (root / name).write_bytes(text.encode("utf8"))
        expected = []
        for i, row in enumerate(rows):
            wire = name + f".{i}.qro"
            (root / wire).write_bytes(row[7])
            expected.append(dict(id=row[0], c=row[3], z=row[4], t=row[5], classification=row[6], geometry=wire))
        report["positive"].append(dict(file=name, objects=expected))

    positive("main.tsv", MAIN)
    positive("main-crlf.tsv", MAIN, lambda text: text.replace("\n", "\r\n"))
    positive("main-no-final-newline.tsv", MAIN, lambda text: text.rstrip("\n"))
    for row in MAIN:
        positive(row[1] + ".tsv", [row])
    positive("closed-ring.tsv", [("closed", "polygon", "1,1;5,1;5,5;1,5;1,1", -1, 0, 0, "",
                                  qro(2, [([(1, 1), (5, 1), (5, 5), (1, 5), (1, 1)], [])]))])
    positive("signed-zero.tsv", [("zero", "points", "-0,0;1e-100,1e-100", -1, 0, 0, "", qro(5, [(-0.0, 0), (1e-100, 1e-100)]))])
    positive("exponents.tsv", [("exponent", "rectangle", "1.25e0,2.5e0,4e0,3e0", -1, 0, 0, "", MAIN[0][7])])
    positive("explicit-planes.tsv", [("plane", "points", "1,2", 2147483647, 2147483647, 2147483647, "", qro(5, [(1, 2)]))])
    for length in [256, 257, 4096]:
        positive(f"id-{length}.tsv", [("P" * length, "points", "1,2", -1, 0, 0, "", qro(5, [(1, 2)]))])
    positive("unicode.tsv", [("\"μ 图\"", "points", "1.25,2.5", -1, 0, 0, "样本", qro(5, [(1.25, 2.5)]))])
    positive("geometry-token-bound.tsv", [("limit", "points", "1,2;" * 1023 + "1,2", -1, 0, 0, "", qro(5, [(1, 2)] * 1024))])

    def negative(name, content, status=4):
        if isinstance(content, str):
            content = content.encode("utf8")
        (root / name).write_bytes(content)
        report["negative"].append(dict(file=name, status=status))

    one = row_text(MAIN[0])
    negative("empty.tsv", "", 2)
    negative("header.tsv", HEADER.replace("geometry", "coordinates"), 2)
    negative("header-only.tsv", HEADER)
    negative("bom.tsv", b"\xef\xbb\xbf" + HEADER.encode() + one.encode(), 2)
    negative("columns-short.tsv", HEADER + "a\tpoints\t1,2\t-1\t0\t0\n")
    negative("columns-extra.tsv", HEADER + one.rstrip("\n") + "\textra\n")
    negative("duplicate-id.tsv", HEADER + one + one)
    negative("blank-row.tsv", HEADER + "\n")
    negative("bare-cr.tsv", (HEADER + one).replace("\n", "\r"))
    negative("nul.tsv", (HEADER + one).replace("0001", "a\0b"))
    negative("invalid-utf8.tsv", (HEADER + one).encode().replace(b"0001", b"\xff"))
    negative("empty-id.tsv", HEADER + one.replace("0001", ""))
    negative("long-id.tsv", HEADER + one.replace("0001", "P" * 4097))
    for value in ["nan", "inf", "-1", " 1", "1 ", "1junk", "1e999", "0x1p0", "+1"]:
        negative("number-" + str(len(report["negative"])) + ".tsv", HEADER + f"a\tpoints\t{value},2\t-1\t0\t0\t\n")
    for kind, value in [("unknown", "1,2"), ("rectangle", "1,2,0,3"), ("ellipse", "1,2,3,0"),
                        ("rectangle", "1,2,3"), ("ellipse", "1,2,3,4,5"), ("points", "1,2,3"),
                        ("points", "1"), ("points", "1,2;"), ("points", ";1,2"), ("polyline", "1,2"),
                        ("polygon", "1,1;2,2"), ("polygon", "1,1;5,1;5,5|"),
                        ("polygon", "1,1;5,1;5,5/1,1;5,1;5,5"), ("multipolygon", "1,1;5,1;5,5")]:
        negative("geometry-" + str(len(report["negative"])) + ".tsv", HEADER + f"a\t{kind}\t{value}\t-1\t0\t0\t\n")
    for c, z, t in [(-2, 0, 0), (-1, -1, 0), (-1, 0, -1), (2147483648, 0, 0)]:
        negative("plane-" + str(len(report["negative"])) + ".tsv", HEADER + f"a\tpoints\t1,2\t{c}\t{z}\t{t}\t\n")
    negative("long-geometry.tsv", HEADER + "a\tpoints\t" + "1,2;" * 1024 + "1,2\t-1\t0\t0\t\n")
    for name, kind, geometry in [
            ("self-crossing", "polygon", "1,1;5,5;1,5;5,1"),
            ("hole-outside", "polygon", "1,1;5,1;5,5;1,5|8,8;9,8;9,9;8,9"),
            ("overlapping-parts", "multipolygon", "1,1;5,1;5,5;1,5/3,3;7,3;7,7;3,7"),
            ("zero-line", "polyline", "1,2;1,2"),
            ("outside", "rectangle", "15,15,4,4")]:
        file = "native-" + name + ".tsv"
        (root / file).write_text(HEADER + f"a\t{kind}\t{geometry}\t-1\t0\t0\t\n", encoding="utf8", newline="")
        report["native_negative"].append(dict(file=file, apply=name == "outside"))
    (root / "geometry-total.txt").write_text(str(sum(len(row[7]) for row in MAIN)), encoding="ascii")
    (root / "fixtures.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--validate", action="store_true")
    parser.add_argument("--reuse", type=Path, help="Reuse unchanged passing pre-fault native evidence")
    for name in ["module", "manifest", "bad-module", "test", "cli", "overlay", "c-header-test", "old-import-test", "old-runtime-test", "output"]:
        parser.add_argument("--" + name, type=Path)
    args = parser.parse_args()
    if args.validate:
        if any(getattr(args, name.replace("-", "_")) is None for name in ["module", "manifest", "bad-module", "test", "cli", "overlay", "c-header-test", "old-import-test", "old-runtime-test", "output"]):
            parser.error("Validation needs all module/consumer/manifest/output arguments")
        validate(args)
        return
    report = fixtures(args.fixtures.resolve())
    print(json.dumps({key: len(report[key]) for key in ["positive", "negative", "native_negative"]}))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(args):
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    fixture_dir = args.fixtures.resolve()
    artifacts = {name: getattr(args, name).resolve() for name in ["module", "manifest", "bad_module", "test", "cli", "overlay", "c_header_test", "old_import_test", "old_runtime_test"]}
    descriptor = json.loads(args.manifest.read_text(encoding="utf8"))
    entry = descriptor["plugins"][0]
    assert len(descriptor["plugins"]) == 1 and descriptor["schema"] == "qpx.format-plugins/1"
    assert entry["abi"] == 2 and entry["provider_id"] == "org.qupathxx.geometry-tsv" and entry["module"]["sha256"] == sha(args.module)
    bad_dir = output / "bad-provider"
    bad_dir.mkdir()
    shutil.copyfile(args.bad_module, bad_dir / args.bad_module.name)
    bad_descriptor = json.loads(json.dumps(descriptor))
    bad_descriptor["plugins"][0]["module"] = {"path": args.bad_module.name, "sha256": sha(args.bad_module)}
    bad_manifest = bad_dir / "format-plugins.json"
    bad_manifest.write_text(json.dumps(bad_descriptor, indent=2) + "\n", encoding="utf8")
    artifacts["bad_manifest"] = bad_manifest
    old_root = root / "build/format-family-plugins"
    qualified = json.loads((old_root / "validation-settled/validation.json").read_text(encoding="utf8"))
    for key in ["matrix_module", "annotation_module", "manifest", "bad_manifest"]:
        record = qualified["artifacts_before"][key]
        path = Path(record["path"])
        assert sha(path) == record["sha256"], "Retained qualified input changed: " + key
        artifacts["old_" + key] = path
    before = {name: {"path": str(path), "sha256": sha(path)} for name, path in artifacts.items()}
    fixture_hashes = {p.relative_to(fixture_dir).as_posix(): sha(p) for p in fixture_dir.rglob("*") if p.is_file()}
    result = {"schema": "qpx.annotation-geometry-validation/1", "artifacts_before": before,
              "fixtures": fixture_hashes, "commands": [], "passed": False}
    reusable = {}
    if args.reuse:
        previous_path = args.reuse.resolve()
        previous = json.loads(previous_path.read_text(encoding="utf8"))
        assert previous["fixtures"] == fixture_hashes, "Reuse needs identical fixtures"
        for name, artifact in before.items():
            if name not in {"bad_module", "bad_manifest"}:
                assert previous["artifacts_before"][name]["sha256"] == artifact["sha256"], "Changed accepted input cannot reuse evidence: " + name
        for row in previous["commands"]:
            if row["returncode"] == row["expected"] and row["name"] in {"c-layout", "direct-abi", "create", "reopen", "offline", "refusals", "changed-source"}:
                log = previous_path.parent / row["log"]
                assert sha(log) == row["log_sha256"], "Changed accepted log"
                reusable[row["name"]] = (row, log)
        result["reused_validation"] = {"path": str(previous_path), "sha256": sha(previous_path),
                                       "scope": "Unchanged primary host/module/native tests; only the controlled bad-module event writer changed"}
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("PYTHON")}
    env["PATH"] = os.pathsep.join([os.environ["SystemRoot"] + "/System32", os.environ["SystemRoot"]])

    def save():
        (output / "validation.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf8")

    def run(name, command, expected=0, report=False):
        command = list(map(str, command))
        if name in reusable:
            assert not report
            row, log = reusable[name]
            if name in {"create", "refusals", "changed-source"}:
                source, destination = Path(row["argv"][-1]), Path(command[-1])
                destination.relative_to(output)
                shutil.copytree(source, destination, dirs_exist_ok=True)
                assert {p.relative_to(source).as_posix(): sha(p) for p in source.rglob("*") if p.is_file()} == {p.relative_to(destination).as_posix(): sha(p) for p in destination.rglob("*") if p.is_file()}, "Copied accepted work differs"
            copied = dict(row, log=str(log), reused=True, requested_argv=command)
            result["commands"].append(copied)
            save()
            return None
        process = subprocess.run(command, env=env, capture_output=True)
        log = output / (name + ".log")
        log.write_bytes(process.stdout + process.stderr)
        result["commands"].append(dict(name=name, argv=command, returncode=process.returncode, expected=expected, log=log.name, log_sha256=sha(log)))
        save()
        assert process.returncode == expected, log.read_text(encoding="utf8", errors="replace")
        return json.loads(process.stdout) if report else None

    def work(name):
        path = output / name
        path.mkdir()
        return path

    def native(mode, manifest, destination):
        run(mode, [args.test, mode, manifest, fixture_dir, destination])

    run("c-layout", [args.c_header_test])
    run("direct-abi", [args.test, "abi", args.module.resolve(), fixture_dir, output])
    native("create", args.manifest.resolve(), work("project"))
    native("reopen", args.manifest.resolve(), output / "project")
    native("offline", "-", output / "project")
    native("refusals", args.manifest.resolve(), work("native-refusals"))
    native("changed-source", args.manifest.resolve(), work("source-change"))
    fault_modes = ["metadata-encoding", "metadata-reserved", "metadata-zero", "metadata-total", "metadata-huge",
                   "probe-geometry-huge", "probe-text-huge", "probe-count", "probe-pointer", "probe-request",
                   "fill-pointer", "fill-count", "fill-size", "fill-offset", "fill-overrun", "fill-text-offset", "fill-utf8",
                   "fill-plane", "fill-reserved", "fill-qro", "fill-kind", "fill-qro-count", "fill-nan", "fill-negative",
                   "fill-truncated", "wait-objects"]
    for mode in fault_modes:
        native(mode, bad_manifest, work("fault-" + mode))
    changed_dir = work("changed-provider")
    changed_module = changed_dir / args.module.name
    changed_module.write_bytes(args.module.read_bytes() + b"QPX-geometry-identity-control\n")
    changed_descriptor = json.loads(json.dumps(descriptor))
    changed_descriptor["plugins"][0]["module"]["sha256"] = sha(changed_module)
    changed_manifest = changed_dir / "format-plugins.json"
    changed_manifest.write_text(json.dumps(changed_descriptor, indent=2) + "\n", encoding="utf8")
    native("changed-implementation", changed_manifest, output / "project")
    result["changed_module_sha256"] = sha(changed_module)
    # These tests exercise changed host/session/preview code with exactly the
    # accepted earlier providers and fixture bytes, without recompiling codecs.
    old_fixture = old_root / "validation-settled/fixtures-μ-图"
    for case in ["annotation", "annotation-stale", "annotation-cancel", "limits", "native-id-limits", "matrix", "legacy"]:
        run("v1-import-" + case, [args.old_import_test, case, artifacts["old_manifest"], old_fixture, work("v1-import-" + case)])
    for case in ["catalog", "matrix-control", "annotation-control", "receipt", "ranges", "cancel"]:
        run("v1-host-" + case, [args.old_runtime_test, "--host-case", case, artifacts["old_manifest"], old_fixture])
    baseline = json.loads((output.parent / "legacy-points-baseline.json").read_text(encoding="utf8"))
    command = baseline["argv"].copy()
    command[0] = str(args.cli.resolve())
    command[command.index("--class-map-json") - 1] = str(output / "legacy-points-current.qpproj")
    legacy_report = run("legacy-point-cli", command, report=True)
    expected_report = baseline["report"].copy()
    expected_report["output"] = legacy_report["output"]
    assert legacy_report == expected_report, "Original ABI1 preview/receipt/mapping changed"
    run("legacy-point-project", [args.test, "compare-projects", baseline["report"]["output"], legacy_report["output"], output])
    bootstrap = [args.cli, "--format-plugins", args.manifest.resolve()]
    catalog = run("geometry-catalog", [*bootstrap, "catalog"], report=True)
    assert catalog["diagnostics"] == [] and len(catalog["providers"]) == 1 and catalog["providers"][0]["module_sha256"] == sha(args.module)
    project = output / "project"
    base = project / "base.qpproj"
    class_map = json.dumps({"T": "Native Tumor", "S": "Native Stroma"})
    command = [*bootstrap, "annotations-import", entry["provider_id"], base, 1, fixture_dir / "main.tsv", output / "cli-geometry.qpproj", "--class-map-json", class_map]
    cli_report = run("geometry-cli", command, report=True)
    assert cli_report["command"] == "annotations-import" and cli_report["preview_sha256"] == (project / "preview.sha256").read_text(encoding="utf8") and cli_report["receipt"] == json.loads((project / "receipt.json").read_text(encoding="utf8"))
    run("geometry-cli-project", [args.test, "compare-projects", project / "geometry.qpproj", output / "cli-geometry.qpproj", output])
    source_digests = {str(p): sha(p) for p in [base, fixture_dir / "main.tsv", fixture_dir / "image.tif"]}
    for name, input_file, extra in [("missing-class", "main.tsv", []), ("invalid-geometry", "native-self-crossing.tsv", ["--class-map-json", class_map]),
                                    ("long-target-id", "id-257.tsv", [])]:
        destination = output / (name + ".qpproj")
        run("cli-" + name, [*bootstrap, "annotations-import", entry["provider_id"], base, 1, fixture_dir / input_file, destination, *extra], expected=2)
        assert not destination.exists(), "Refused CLI published a project"
    run("cli-existing-output", [*bootstrap, "annotations-import", entry["provider_id"], base, 1, fixture_dir / "main.tsv", base, "--class-map-json", class_map], expected=2)
    assert all(sha(Path(path)) == digest for path, digest in source_digests.items()), "CLI refusal changed input"
    coexist_dir = work("coexist")
    coexist = {"schema": "qpx.format-plugins/1", "plugins": json.loads(artifacts["old_manifest"].read_text(encoding="utf8"))["plugins"] + descriptor["plugins"]}
    for manifest, directory in [(json.loads(artifacts["old_manifest"].read_text(encoding="utf8")), artifacts["old_manifest"].parent), (descriptor, args.module.parent)]:
        for item in manifest["plugins"]:
            shutil.copyfile(directory / item["module"]["path"], coexist_dir / item["module"]["path"])
    coexist_manifest = coexist_dir / "format-plugins.json"
    coexist_manifest.write_text(json.dumps(coexist, indent=2) + "\n", encoding="utf8")
    mixed = run("mixed-catalog", [args.cli, "--format-plugins", coexist_manifest, "catalog"], report=True)
    assert mixed["diagnostics"] == [] and {p["provider_id"]: p["module_sha256"] for p in mixed["providers"]} == {p["provider_id"]: p["module"]["sha256"] for p in coexist["plugins"]}
    result["artifacts_after"] = {name: {"path": str(path), "sha256": sha(path)} for name, path in artifacts.items()}
    result["artifacts_unchanged"] = result["artifacts_before"] == result["artifacts_after"]
    result["fixtures_unchanged"] = fixture_hashes == {p.relative_to(fixture_dir).as_posix(): sha(p) for p in fixture_dir.rglob("*") if p.is_file()}
    assert result["artifacts_unchanged"] and result["fixtures_unchanged"]
    result["passed"] = True
    save()
    print(json.dumps(dict(passed=True, native_checks=len(result["commands"]), artifacts_unchanged=True, original_point_identity=legacy_report["preview_sha256"])))


if __name__ == "__main__":
    main()
