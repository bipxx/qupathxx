"""Verify project region exports preserve retained analysis paths and payloads."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("region", "fixture", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    if args.output.parent != args.fixture.parent:
        parser.error("--output must be a sibling of --fixture to preserve relative image links")
    args.output.mkdir()
    env = {key: value for key, value in os.environ.items() if not key.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, command, expected=0):
        result = subprocess.run([args.region, *command], cwd=args.output, env=env,
                                capture_output=True, encoding="utf-8", timeout=45)
        assert result.returncode == expected, (label, result.stdout, result.stderr)
        if expected:
            assert not result.stdout and result.stderr, label
            checks[label] = result.stderr.strip()
        else:
            checks[label] = json.loads(result.stdout)
        return checks[label]

    project = shutil.copyfile(args.fixture / "source.qpxproj", args.output / "source.qpxproj")

    def copy_asset(folder, pattern):
        source, = (args.fixture / folder).glob(pattern)
        target = args.output / source.relative_to(args.fixture)
        target.parent.mkdir()
        return shutil.copyfile(source, target)

    matrix = copy_asset(".qpx-expression", "*.qmx")
    archive = copy_asset(".qpx-expression-reductions", "*.qpr")
    before = {path: sha256(path) for path in (project, matrix, archive)}
    raw_args = ["--width", "2", "--height", "2"]
    commands = {
        "npy": lambda target: ["--raw-project", project, "2", target, *raw_args],
        "zip": lambda target: ["--raw-project", project, "2", target, *raw_args, "--bundle"],
        "pam": lambda target: ["--project-region", project, "2", "0", "0", "0", "2", "2", target, "--receipt"],
    }
    with zipfile.ZipFile(args.fixture / "baseline.zip") as baseline:
        baseline_report = json.loads(baseline.read("receipt.json"))["region"]
        baseline_array = baseline.read("raw.npy")
    assert baseline_array == (args.fixture / "baseline.npy").read_bytes()
    assert baseline_report["project"]["project_sha256"] == before[project]

    def normal_outputs(label):
        for kind, command in commands.items():
            target = args.output / f"{label}.{kind}"
            report = run(f"{label}-{kind}", command(target))
            assert report["project"] == baseline_report["project"]
            if kind == "npy":
                assert report == baseline_report and target.read_bytes() == baseline_array
            elif kind == "zip":
                assert report == baseline_report
                verified = run(label + "-verify-bundle", ["--verify-raw-bundle", target])
                with zipfile.ZipFile(target) as bundle:
                    assert bundle.read("raw.npy") == baseline_array
                    assert json.loads(bundle.read("receipt.json")) == verified
                assert verified["region"] == report
            else:
                assert report["view"]["series"] == baseline_report["series"]
                assert report["sha256"] == sha256(target) == sha256(args.fixture / "baseline.pam")

    protected = {"qmx": matrix, "qpr": archive}
    for suffix in ("journal", "wal", "shm"):
        protected["qmx-" + suffix] = matrix.with_name(matrix.name + "-" + suffix)
        protected["project-" + suffix] = project.with_name(project.name + "-" + suffix)
    offline = {path: path.with_suffix(".offline") for path in (matrix, archive)}
    for original, saved in offline.items():
        original.rename(saved)
    try:
        for kind, command in commands.items():
            for name, path in protected.items():
                error = run(f"{kind}-{name}", command(path), expected=1 if kind == "pam" else 2)
                assert "must not replace its input" in error and not path.exists(), error
        normal_outputs("offline")
    finally:
        for original, saved in offline.items():
            saved.rename(original)

    alias = args.output / "matrix-alias.npy"
    os.link(matrix, alias)
    error = run("matrix-alias", commands["npy"](alias), expected=2)
    assert "aliases its input" in error and sha256(alias) == before[matrix]
    normal_outputs("online")
    assert all(sha256(path) == digest for path, digest in before.items())
    assert not list(args.output.rglob("*.qpx-tmp-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "region_sha256": sha256(args.region), "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} project region path protection and replay checks passed")


if __name__ == "__main__":
    main()
