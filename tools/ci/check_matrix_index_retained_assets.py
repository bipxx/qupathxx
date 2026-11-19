"""Verify saved-project matrix indexing protects retained destination paths."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("format-cli", "fixture", "csv-manifest", "csv-positions", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    windows = Path(os.environ["SYSTEMROOT"])
    env = {key: value for key, value in os.environ.items() if not key.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, command, expected=0):
        result = subprocess.run([args.format_cli, *command], cwd=args.output, env=env,
                                capture_output=True, encoding="utf-8", timeout=45)
        assert result.returncode == expected, (label, result.stdout, result.stderr)
        assert not (result.stdout if expected else result.stderr), label
        report = json.loads(result.stderr if expected else result.stdout)
        checks[label] = report
        return report

    def copy_asset(folder, pattern):
        original, = (args.fixture / folder).glob(pattern)
        target = args.output / folder / original.name
        target.parent.mkdir()
        return shutil.copyfile(original, target)

    project = shutil.copyfile(args.fixture / "source.qpxproj", args.output / "source.qpxproj")
    matrix = copy_asset(".qpx-expression", "*.qmx")
    reduction = copy_asset(".qpx-expression-reductions", "*.qpr")
    csv_input = args.fixture / "counts.csv"
    store = json.loads((args.fixture / "store-fixture.json").read_text(encoding="utf-8-sig"))
    preserved = {path: digest(path) for path in (
        project, matrix, reduction, csv_input, args.csv_positions, Path(store["positions"]),
    )}
    store_files = {path: digest(path) for path in Path(store["store"]).rglob("*") if path.is_file()}
    assert store_files
    csv_options = json.dumps(dict(observation_id="spot", feature_columns=["A", "B"], value_domain="raw-counts"))
    routes = {
        "file": (["--format-plugins", args.csv_manifest, "matrix-project-index", "org.qupathxx.csv-matrix",
                  csv_input, args.csv_positions], csv_options),
        "store": (["--format-plugins", store["manifest"], "matrix-project-store-index",
                   "org.qupathxx.anndata-zarr", store["store"], ".zattrs", store["positions"]], store["options"]),
    }

    def index(route, target):
        prefix, options = routes[route]
        return [*prefix, project, "1", target, "--plugin-options-json", options]

    protected = {"matrix": matrix, "archive": reduction}
    for suffix in ("journal", "wal", "shm"):
        for name, path in (("project", project), ("matrix", matrix)):
            protected[name + "-" + suffix] = path.with_name(path.name + "-" + suffix)
    offline = {path: path.with_suffix(".offline") for path in (matrix, reduction)}
    targets = {route: args.output / (route + ".qmx") for route in routes}
    for original, saved in offline.items():
        original.rename(saved)
    try:
        for route, target in targets.items():
            for name, forbidden in protected.items():
                refused = run(f"{route}-{name}", index(route, forbidden), expected=2)
                assert "must not replace its input" in refused["error"], (route, name, refused)
                assert not forbidden.exists()
            report = run("index-" + route, index(route, target))
            baseline = json.loads((args.fixture / (route + "-baseline.json")).read_bytes())
            assert report["state"] == "indexed" and report["file_sha256"] == digest(target)
            assert digest(target) == digest(args.fixture / ("baseline-" + route + ".qmx"))
            assert {k: v for k, v in report.items() if k != "output"} == {
                k: v for k, v in baseline.items() if k != "output"
            }
    finally:
        for original, saved in offline.items():
            saved.rename(original)

    aliased = args.output / "alias.qmx"
    os.link(reduction, aliased)
    refused = run("archive-hardlink", index("file", aliased), expected=2)
    assert "aliases its input" in refused["error"] and digest(aliased) == preserved[reduction]
    for route, source in targets.items():
        attached = args.output / (route + "-owned.qpxproj")
        report = run("attach-" + route, ["matrix-attach", project, "1", source, digest(source), attached])
        assert report["owned"] and report["file_sha256"] == digest(source)
        source.rename(source.with_suffix(".offline"))
        listed = run("reopen-" + route, ["project-matrices", attached, "1"])
        new_matrix, = [item for item in listed["matrices"] if item["association_id"] == report["association_id"]]
        assert new_matrix["file_sha256"] == report["file_sha256"]
    assert all(digest(path) == before for path, before in {**preserved, **store_files}.items())
    assert not list(args.output.rglob("*.qpx-tmp-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "format_cli_sha256": digest(args.format_cli), "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} saved-project matrix indexing and attachment checks passed")


if __name__ == "__main__":
    main()
