"""Verify independent installed format manifests through the native CLI."""
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    keys = ("host", "old-host", "annotation-manifest", "matrix-manifest", "reader-manifest",
            "annotation-source", "matrix-source", "positions", "template", "annotation-probe",
            "matrix-probe", "baseline", "output")
    for key in keys:
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    root = args.output
    inputs = root / "inputs"
    inputs.mkdir(parents=True)
    baseline = json.loads(args.baseline.read_bytes())
    assert baseline["exit_code"] == 2 and sha(args.old_host) == baseline["host_sha"].lower()
    assert sha(args.annotation_manifest) == baseline["annotation_manifest_sha"].lower()
    assert sha(args.matrix_manifest) == baseline["matrix_manifest_sha"].lower()
    template_sha = sha(args.template)
    sources = []
    for src in (args.annotation_source, args.matrix_source, args.positions):
        copy = inputs / src.name
        shutil.copyfile(src, copy)
        sources.append(copy)
    annotation, matrix, positions = sources
    inputs_sha = {path: sha(path) for path in sources}
    anno_id = "org.qupathxx.geoparquet"
    matrix_id = "org.qupathxx.parquet-matrix"
    flags = ["--format-plugins", args.annotation_manifest, "--format-plugins", args.matrix_manifest]
    raw = ["--reader-plugins", args.reader_manifest]
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    system = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    checks = {}

    def run(argv, status=0):
        result = subprocess.run(argv, cwd=root, env=env, capture_output=True, encoding="utf-8", timeout=40)
        assert result.returncode == status, (argv, result.returncode, result.stdout, result.stderr)
        return result

    def catalog(host, *options):
        return json.loads(run([host, *options, "catalog"]).stdout)

    def providers(payload):
        return {item["provider_id"]: item for item in payload["providers"]}

    singles = {}
    for manifest in (args.annotation_manifest, args.matrix_manifest):
        previous = catalog(args.old_host, "--format-plugins", manifest)
        current = catalog(args.host, "--format-plugins", manifest)
        assert current == previous and not current["diagnostics"]
        singles.update(providers(current))
    assert set(singles) == {anno_id, matrix_id}
    checks["single-manifest-catalogs"] = sorted(singles)
    for label, paths in (("combined-catalog", flags), ("reverse-catalog", [*flags[2:], *flags[:2]])):
        combined = catalog(args.host, *paths)
        assert providers(combined) == singles and not combined["diagnostics"]
        checks[label] = combined

    def authored(path, contents):
        target = root / path / "format-plugins.json"
        target.parent.mkdir()
        target.write_text(contents, encoding="utf-8")
        return target

    malformed = authored("malformed manifest", "{")
    for label, invalid in (("malformed", malformed), ("missing", root / "missing.json")):
        result = catalog(args.host, *flags, "--format-plugins", invalid)
        assert providers(result) == singles and len(result["diagnostics"]) == 1
        assert str(invalid) in result["diagnostics"][0]
        checks[label] = result["diagnostics"]

    original = args.annotation_manifest.read_text(encoding="utf-8")
    duplicate = authored("same provider elsewhere", original)
    changed = json.loads(original)
    changed["plugins"][0]["provider_id"] = "org.qupathxx.conflicting-format"
    same_format = authored("same format elsewhere", json.dumps(changed))
    for label, manifest, message in (
        ("duplicate-provider", duplicate, "provider ID is claimed by multiple manifests"),
        ("duplicate-format", same_format, "domain/format ID is claimed by multiple manifests"),
    ):
        for options in ([*flags, "--format-plugins", manifest], ["--format-plugins", manifest, *flags]):
            report = catalog(args.host, *options)
            assert providers(report) == {matrix_id: singles[matrix_id]} and len(report["diagnostics"]) == 2
            assert all(message in item for item in report["diagnostics"]), report
        checks[label] = report["diagnostics"]

    changed = json.loads(original)
    plugin = changed["plugins"][0]
    module_name = plugin["module"]["path"]
    plugin["module"]["sha256"] = "0" * 64
    bad_hash = authored("bad hash", json.dumps(changed))
    shutil.copyfile(args.annotation_manifest.with_name(module_name), bad_hash.with_name(module_name))
    result = catalog(args.host, "--format-plugins", bad_hash, *flags[2:])
    assert providers(result) == {matrix_id: singles[matrix_id]} and len(result["diagnostics"]) == 1
    assert str(bad_hash) in result["diagnostics"][0] and "hash" in result["diagnostics"][0].lower(), result
    checks["bad-hash"] = result["diagnostics"]

    rejected = run([args.host, *flags[:2], *flags[:2], "catalog"], 2)
    assert "Duplicate format plugin manifest path" in rejected.stderr
    rejected = run([args.host, "--format-plugins", "relative.json", "catalog"], 2)
    assert "requires an absolute" in rejected.stderr
    rejected = run([args.host, *raw, *raw, "catalog"], 2)
    assert "Duplicate bootstrap option: --reader-plugins" in rejected.stderr
    checks["bootstrap-refusals"] = 3

    anno_options = {"coordinate_space": "projected-xy", "source_crs": "EPSG:3857",
                    "source_to_pixels": [0.5, 0, 500, 0, -0.25, 500],
                    "geometry": "geometry", "id": "source_id", "name": "title"}
    matrix_options = {"observation_id": "spot", "feature_columns": ["Gene.B", "Gene.A"], "value_domain": "raw-counts"}

    def invoke(host, boot, command, target, opts):
        p = run([host, *boot, *raw, *command, target, "--plugin-options-json", json.dumps(opts)])
        return json.loads(p.stdout)

    command = ["annotations-import", anno_id, args.template, "1", annotation]
    annotated = root / "annotated.qpxproj"
    old = invoke(args.old_host, flags[:2], command, root / "reference-annotations.qpxproj", anno_options)
    new = invoke(args.host, flags, command, annotated, anno_options)
    assert new["preview_sha256"] == old["preview_sha256"]
    assert new["receipt"] == old["receipt"] and new["objects"] == old["objects"]
    checks["annotation"] = new

    command = ["matrix-project-index", matrix_id, matrix, positions, args.template, "1"]
    qmx = root / "counts.qmx"
    old_qmx = root / "reference.qmx"
    old = invoke(args.old_host, flags[2:], command, old_qmx, matrix_options)
    new = invoke(args.host, flags, command, qmx, matrix_options)
    assert new["receipt"] == old["receipt"] and new["file_sha256"] == old["file_sha256"] == sha(qmx) == sha(old_qmx)
    checks["matrix"] = new

    assert {path: sha(path) for path in sources} == inputs_sha
    inputs.rename(root / "inputs-unavailable")
    final = root / "image-with-annotations-and-matrix.qpxproj"
    attached = json.loads(run([args.host, *raw, "matrix-attach", annotated, "1", qmx, new["file_sha256"], final]).stdout)
    assert attached["owned"] and attached["file_sha256"] == new["file_sha256"]
    qmx.rename(root / "inputs-unavailable" / qmx.name)
    result = run([args.annotation_probe, final, args.reader_manifest, "registered"])
    matrix_result = run([args.matrix_probe, final, attached["association_id"], attached["file_sha256"],
                         args.reader_manifest, "counts", matrix_id])
    checks["offline-project-reopen"] = [result.stdout.strip(), matrix_result.stdout.strip()]
    assert sha(args.template) == template_sha
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "host_sha256": sha(args.host), "baseline_host_sha256": sha(args.old_host), "checks": checks,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} combined catalog, import and offline project checks")


if __name__ == "__main__":
    main()
