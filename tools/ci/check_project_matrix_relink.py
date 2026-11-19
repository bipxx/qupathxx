"""Exercise exact external QMX relink and ownership with new-only project publication."""
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
    for key in ("host", "template", "exact-qmx", "wrong-qmx", "owned-project", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, *argv, expected=0):
        result = subprocess.run([args.host, *argv], cwd=args.output, env=env, encoding="utf-8",
                                capture_output=True, timeout=45)
        assert result.returncode == expected, (label, result.stdout, result.stderr)
        payload = json.loads(result.stdout if expected == 0 else result.stderr)
        checks[label] = payload
        return payload

    inbound = args.output / "input"
    inbound.mkdir()
    project = inbound / "original.qpx"
    shutil.copyfile(args.template, project)
    project_sha = sha(project)
    original, = run("original-list", "project-matrices", project, "1")["matrices"]
    old = Path(original["path"])
    assert old.is_relative_to(inbound) and not old.exists() and not original["owned"]
    association = original["association_id"]
    digest = original["file_sha256"]
    assert sha(args.exact_qmx) == digest and sha(args.wrong_qmx) != digest
    old.parent.mkdir(exist_ok=True)
    run("original-offline", "project-matrix-info", project, "1", association, expected=2)

    directory = args.output / "linked"
    directory.mkdir()
    assets = directory / "assets"
    assets.mkdir()
    backup = assets / "moved.qmx"
    shutil.copyfile(args.exact_qmx, backup)
    target = directory / "relinked.qpx"
    command = ("project-matrix-relink", project, "1", association)
    error = run("wrong-qmx", *command, args.wrong_qmx, target, expected=2)
    assert "exact file digest differs" in error["error"] and not target.exists()
    for label, destination in (("protect-project", project), ("protect-replacement", backup), ("protect-original-locator", old)):
        error = run(label, *command, backup, destination, expected=2)
        assert "must not replace its input" in error["error"], error
    assert not old.exists()
    companion = target.with_name(target.name + "-journal")
    companion.write_bytes(b"destination companion sentinel")
    error = run("existing-companion", *command, backup, target, expected=2)
    assert "destination has a SQLite companion" in error["error"] and not target.exists()
    assert companion.read_bytes() == b"destination companion sentinel"
    companion.rename(directory / "retained-journal")

    saved = run("save-relinked", *command, backup, target)
    assert saved["state"] == "saved" and not saved["owned"]
    assert saved["association_id"] == association and saved["file_sha256"] == digest
    assert Path(saved["path"]) == backup and Path(saved["output"]) == target
    saved_sha = sha(target)
    repeated = run("existing-destination", *command, backup, target, expected=2)
    assert "destination already exists" in repeated["error"] and sha(target) == saved_sha
    listed, = run("relinked-list", "project-matrices", target, "1")["matrices"]
    assert listed == {**original, "path": str(backup)}
    assert run("original-still-offline", "project-matrices", project, "1")["matrices"] == [original]

    relocated = args.output / "relocated"
    directory.rename(relocated)
    target = relocated / target.name
    saved_backup = relocated / "assets" / backup.name
    changed = run("relocated-info", "project-matrix-info", target, "1", association)
    assert Path(changed["input"]) == saved_backup and changed["file_sha256"] == digest
    assert changed["association_id"] == association and not changed["owned"]
    table = args.output / "values.csv"
    run("exact-replay", "project-matrix-export-csv", target, "1", association, table, "--feature", "B", "--feature", "A")
    with table.open(encoding="utf-8", newline="") as stream:
        assert list(csv.reader(stream)) == [
            ["observation_id", "B", "A"], ["X", "0", "9007199254740993"], ["Y", "7", "0"], ["Z", "0", "5"],
        ]

    owned, = run("owned-list", "project-matrices", args.owned_project, "1")["matrices"]
    refusal = run("owned-refusal", "project-matrix-relink", args.owned_project, "1", owned["association_id"],
                  saved_backup, args.output / "owned.qpx", expected=2)
    assert "requires an external project association" in refusal["error"]

    collection = args.output / "owned-projects"
    collection.mkdir()
    owned_project = collection / "new.qpx"
    managed = collection / ".qpx-expression" / (digest + ".qmx")
    managed.parent.mkdir()
    command = ("project-matrix-own", project, "1", association)
    refusal = run("own-wrong-copy", *command, args.wrong_qmx, owned_project, expected=2)
    assert "exact file digest differs" in refusal["error"] and not owned_project.exists()
    refusal = run("own-original-target", *command, saved_backup, old, expected=2)
    assert "must not replace its input" in refusal["error"] and not old.exists()
    managed.write_bytes(b"conflicting managed asset")
    refusal = run("own-managed-conflict", *command, saved_backup, owned_project, expected=2)
    assert "exact file digest differs" in refusal["error"] and not owned_project.exists()
    assert managed.read_bytes() == b"conflicting managed asset"
    managed.rename(collection / "retained-conflict.qmx")
    journal = managed.with_name(managed.name + "-journal")
    journal.write_bytes(b"managed companion sentinel")
    refusal = run("own-managed-companion", *command, saved_backup, owned_project, expected=2)
    assert "Managed expression destination has a SQLite companion" in refusal["error"]
    assert not owned_project.exists() and not managed.exists()
    journal.rename(collection / "retained-companion")
    converted = run("own-save-new", *command, saved_backup, owned_project)
    assert converted["owned"] and Path(converted["path"]) == managed and sha(managed) == digest
    assert converted["association_id"] == association and converted["file_sha256"] == digest
    stamp = managed.stat().st_mtime_ns
    shared_project = collection / "shared.qpx"
    second = run("own-reuse-exact", *command, saved_backup, shared_project)
    assert second["owned"] and Path(second["path"]) == managed and managed.stat().st_mtime_ns == stamp

    offline = args.output / "external-offline.qmx"
    saved_backup.rename(offline)
    assert not saved_backup.exists() and not old.exists()
    for label, reopened in (("owned", owned_project), ("shared-owned", shared_project)):
        inspected = run(label + "-offline-info", "project-matrix-info", reopened, "1", association)
        assert inspected["owned"] and Path(inspected["input"]) == managed and inspected["file_sha256"] == digest
    owned_table = args.output / "owned-values.csv"
    run("owned-offline-exact", "project-matrix-export-csv", owned_project, "1", association,
        owned_table, "--feature", "B", "--feature", "A")
    assert owned_table.read_bytes() == table.read_bytes()
    assert not (inbound / ".qpx-expression").exists()
    assert sha(project) == sha(args.template) == project_sha and sha(target) == saved_sha and sha(offline) == digest
    assert not list(args.output.rglob("*.qpx-tmp-*")) and not list(args.output.rglob(".qpx-save-new-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "host_sha256": sha(args.host), "input_project_sha256": project_sha, "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} exact external relink and ownership checks passed")


if __name__ == "__main__":
    main()
