"""Check reserved project destinations in cell and expression analysis output."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("batch", "cell-project", "aggregation", "differential", "baseline", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, argv, expected=0):
        result = subprocess.run([args.batch, *argv], cwd=args.output, env=env,
                                capture_output=True, encoding="utf-8", timeout=45)
        assert result.returncode == expected, (label, result.stdout, result.stderr)
        payload = result.stdout if expected == 0 else result.stderr
        checks[label] = json.loads(payload) if payload else {}
        return checks[label]

    def row(path):
        with path.open("r", encoding="utf-8", newline="") as stream:
            return next(csv.DictReader(stream))

    source = args.output / "cell.qpxproj"
    shutil.copyfile(args.cell_project, source)
    source_hash = sha(source)
    protected_assets = {source: source_hash}
    cell_output = args.output / "cell.csv"

    def reject_companions(label, command, project, *params):
        for suffix in ("-journal", "-wal", "-shm"):
            candidate = project.with_name(project.name + suffix)
            report = run(label + suffix, [command, project, candidate, *params], expected=2)
            assert "must not replace its input" in report["error"] and not candidate.exists(), report

    reject_companions("cell", "--export-cell-table", source)
    run("cell-normal", ["--export-cell-table", source, cell_output])
    assert sha(cell_output) == sha(args.baseline / "cell-baseline.qpxproj-wal")

    aggregation = row(args.aggregation / "bulk.csv")
    differential = row(args.differential / "replicates.csv")
    workflows = (
        ("aggregation", "--aggregate-expression", args.aggregation, "study.qpx", "bulk.csv",
         ["--matrix", aggregation["source_sha256"], "--spec", args.aggregation / "bulk.json"]),
        ("differential", "--differential-expression", args.differential, "input.qpx", "cli.csv",
         ["--pseudobulk", differential["pseudobulk_id"], "--group-a", "A", "--group-b", "B"]),
    )
    for name, command, fixtures, filename, expected_csv, params in workflows:
        directory = args.output / name
        directory.mkdir()
        project = directory / filename
        shutil.copyfile(fixtures / filename, project)
        managed = directory / ".qpx-expression"
        shutil.copytree(fixtures / managed.name, managed)
        protected_assets[project] = sha(project)
        protected_assets.update((p, sha(p)) for p in managed.glob("*.qmx"))
        reject_companions(name, command, project, "--image", "1", *params)

        saved = directory / "saved.qpxproj"
        report = run("valid-" + name, [command, project, saved, "--image", "1", *params])
        retained = json.loads((args.baseline / ("agg-baseline.log" if name == "aggregation" else "de-baseline.log")).read_bytes())
        assert report["state"] == "saved" and report["record"] == retained["record"]
        assert report["result_sha256"] == retained["result_sha256"]
        verify = run("replay-" + name, ["--verify-expression-" + name, saved, "--image", "1", "--record", report["record"]])
        assert verify["state"] == "verified" and verify["result_sha256"] == report["result_sha256"]
        exported = directory / "values.csv"
        run("export-" + name, ["--export-expression-" + name, saved, exported, "--image", "1",
                              "--record", report["record"]])
        assert sha(exported) == sha(fixtures / expected_csv)

    assert all(sha(path) == digest for path, digest in protected_assets.items())
    assert not list(args.output.rglob("*.qpx-tmp-*")) and not list(args.output.rglob(".qpx-save-new-*"))
    report = {"passed": True, "batch_sha256": sha(args.batch), "checks": checks}
    (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} saved analysis, protected output and replay checks passed")


if __name__ == "__main__":
    main()
