"""Verify TMA project routes preserve the source's reserved SQLite names."""
import argparse
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
    for key in ("batch", "fixture", "baseline", "output"):
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
        payload = json.loads(result.stdout if expected == 0 else result.stderr)
        checks[label] = payload
        return payload

    source = args.output / "source.qpxproj"
    applied = args.output / "applied.qpxproj"
    shutil.copyfile(args.fixture / "source.qpxproj", source)
    shutil.copyfile(args.fixture / "applied.qpxproj", applied)
    before = {item: sha(item) for item in (source, applied)}
    proposal = args.output / "preview.qptma"
    detect = ["--image", "2", "--rows", "3", "--columns", "4", "--core-width", "32", "--core-height", "32"]
    imported = args.output / "roundtrip.qpxproj"
    export = args.output / "export.qptma"
    cases = args.output / "cases.csv"
    cohort = args.output / "cohort.csv"

    routes = {
        "proposal": (source, proposal, ["--detect-tma", source, proposal, *detect]),
        "apply": (source, imported, ["--import-qptma", source, proposal, imported, "--image", "2"]),
        "qptma": (applied, export, ["--export-qptma", applied, export, "--image", "2"]),
        "cases": (applied, cases, ["--export-tma-cases", applied, cases, "--image", "2"]),
        "cohort": (applied, cohort, ["--export-tma-cohort", applied, cohort, "--image", "2"]),
    }

    # Reject all reserved paths before computing or publishing any output.
    for name, (project, normal_output, argv) in routes.items():
        if name == "apply":
            command = [args.fixture / "proposal.qptma" if arg == proposal else arg for arg in argv]
        else:
            command = argv
        for suffix in ("-journal", "-wal", "-shm"):
            prohibited = project.with_name(project.name + suffix)
            result = run(name + suffix, [prohibited if arg == normal_output else arg for arg in command], 1)
            assert "must not replace its input" in result["error"] and not prohibited.exists(), result

    for name, (_, _, argv) in routes.items():
        assert run("valid-" + name, argv)["state"] == "saved"
    retained_preview = args.baseline / "detect.qpxproj-wal"
    retained_export = args.baseline / "export.qpxproj-journal"
    assert sha(proposal) == sha(retained_preview)
    assert sha(export) == sha(retained_export)
    assert sha(cases) == sha(args.baseline / "cases-baseline.csv")
    assert sha(cohort) == sha(args.baseline / "cohort-baseline.csv")
    exported = args.output / "roundtrip.qptma"
    run("reopen-and-export", ["--export-qptma", imported, exported, "--image", "2"])
    assert sha(exported) == sha(export)
    assert {item: sha(item) for item in before} == before
    assert not list(args.output.rglob("*.qpx-tmp-*"))
    assert not list(args.output.rglob(".qpx-save-new-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "batch_sha256": sha(args.batch), "checks": checks,
        "valid_output_sha256": {p.name: sha(p) for p in (proposal, export, cases, cohort, exported)},
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} TMA output protection and byte-exact workflow checks passed")


if __name__ == "__main__":
    main()
