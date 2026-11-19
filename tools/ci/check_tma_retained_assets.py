"""Verify TMA publication protects the source project's retained analysis assets."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("batch", "fixture", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    if args.output.parent != args.fixture.parent:
        parser.error("--output must be a sibling of --fixture to preserve relative image links")
    args.output.mkdir(parents=True)
    env = {key: value for key, value in os.environ.items() if not key.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, argv, destination, expected=0):
        command = [*argv, destination, "--image", "2"]
        if argv[0] == "--detect-tma":
            command += ["--rows", "3", "--columns", "4", "--core-width", "32", "--core-height", "32"]
        result = subprocess.run([args.batch, *command], cwd=args.output, env=env,
                                capture_output=True, encoding="utf-8", timeout=45)
        assert result.returncode == expected, (label, result.stdout, result.stderr)
        payload = json.loads(result.stdout if expected == 0 else result.stderr)
        checks[label] = payload
        return payload

    project = args.output / "source.qpxproj"
    shutil.copyfile(args.fixture / project.name, project)
    seed = args.output / "seed.qpxproj"
    shutil.copyfile(args.fixture / seed.name, seed)
    def copy_asset(folder, pattern):
        source, = (args.fixture / folder).glob(pattern)
        target = args.output / source.relative_to(args.fixture)
        target.parent.mkdir()
        return shutil.copyfile(source, target)

    matrix = copy_asset(".qpx-expression", "*.qmx")
    archive = copy_asset(".qpx-expression-reductions", "*.qpr")
    before = {path: sha256(path) for path in (project, seed, matrix, archive)}
    proposal = args.output / "proposal.qptma"
    destinations = {
        "detect": proposal,
        "qptma": args.output / "existing.qptma",
        "cases": args.output / "cases.csv",
        "cohort": args.output / "cohort.csv",
        "import": args.output / "applied.qpxproj",
    }
    routes = {
        "detect": ["--detect-tma", project],
        "qptma": ["--export-qptma", project],
        "cases": ["--export-tma-cases", project],
        "cohort": ["--export-tma-cohort", project],
        "import": ["--import-qptma", seed, proposal],
    }
    retained_expected = {
        "detect": args.fixture / "normal-proposal.qptma",
        "qptma": args.fixture / (archive.name + ".unrelated-output"),
        "cases": args.fixture / (matrix.name + ".unrelated-output"),
        "cohort": args.fixture / "qmx-journal.unrelated-output",
    }

    # Refuse missing retained locators even though publication is new-only.
    companions = {suffix: matrix.with_name(matrix.name + "-" + suffix) for suffix in ("journal", "wal", "shm")}
    offline = {path: path.with_suffix(".offline") for path in (matrix, archive)}
    for original, saved in offline.items():
        original.rename(saved)
    shutil.copyfile(retained_expected["detect"], proposal)
    try:
        for route, argv in routes.items():
            for name, path in {"qmx": matrix, "qpr": archive, **companions}.items():
                refused = run(f"{route}-{name}", argv, path, expected=1)
                assert "must not replace its input" in refused["error"], (route, name, refused)
                assert not path.exists()
        for route, baseline in retained_expected.items():
            target = args.output / ("offline-" + destinations[route].name)
            assert run(f"offline-{route}", routes[route], target)["state"] == "saved"
            assert sha256(target) == sha256(baseline)
    finally:
        for original, saved in offline.items():
            saved.rename(original)
    proposal.unlink()

    for route, argv in routes.items():
        target = destinations[route]
        assert run(f"normal-{route}", argv, target)["state"] == "saved"
        if route in retained_expected:
            assert sha256(target) == sha256(retained_expected[route])
    reopened = args.output / "reopened.qptma"
    run("reopen-import", ["--export-qptma", destinations["import"]], reopened)
    header = b"Core name\t"
    assert reopened.read_bytes().split(header, 1)[1] == proposal.read_bytes().split(header, 1)[1]
    assert all(sha256(path) == digest for path, digest in before.items())
    assert not list(args.output.rglob("*.qpx-tmp-*"))
    assert not list(args.output.rglob(".qpx-save-new-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "batch_sha256": sha256(args.batch), "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} TMA retained-asset publication checks passed")


if __name__ == "__main__":
    main()
