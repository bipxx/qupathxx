"""Exercise exact, new-only backups and recovery of a saved project's managed QMX."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "reader-manifest", "template", "csv-validation", "external-project", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def record(label, result, expected=0, native=True):
        assert result.returncode == expected, (label, result.returncode, result.stdout, result.stderr)
        value = json.loads(result.stdout if expected == 0 else result.stderr) if native else result.stdout.strip()
        checks[label] = value
        return value

    def run(label, argv, expected=0, native=True):
        result = subprocess.run([args.host, *argv] if native else argv, env=env, cwd=args.output,
                                encoding="utf-8", capture_output=True, timeout=45)
        return record(label, result, expected, native)

    validation = json.loads(args.csv_validation.read_bytes())
    prior_sidecars = args.csv_validation.parent / ".qpx-expression"
    first = validation["reopens"]["counts"]
    digest = first["file_sha256"]
    exact = prior_sidecars / (digest + ".qmx")
    wrong = prior_sidecars / (validation["reopens"]["continuous"]["file_sha256"] + ".qmx")
    reader = ["--reader-plugins", args.reader_manifest]
    project = args.output / "saved.qpxproj"
    initial = run("seed", [*reader, "matrix-attach", args.template, "1", exact, digest, project])
    association_id = initial["association_id"]
    assert association_id == first["association_id"]
    project_sha = sha(project)
    catalog = run("list", ["project-matrices", project, "1"])
    stored, = catalog["matrices"]
    target = Path(stored["path"])
    backup = args.output / "exact-backup.qmx"
    backup_argv = ["project-matrix-backup", project, "1", association_id]
    for label, protected in (("project", project), ("matrix", target),
                             ("source-wal", target.with_name(target.name + "-wal"))):
        refused = run("backup-protect-" + label, [*backup_argv, protected], 2)
        assert "must not replace its input" in refused["error"], refused
    companion = backup.with_name(backup.name + "-journal")
    companion.write_bytes(b"backup companion sentinel")
    refused = run("backup-refuse-companion", [*backup_argv, backup], 2)
    assert "backup destination has a SQLite companion" in refused["error"] and not backup.exists()
    assert companion.read_bytes() == b"backup companion sentinel"
    companion.rename(args.output / "backup-companion-retained")
    created = run("create-exact-backup", [*backup_argv, backup])
    assert created["state"] == "exported" and created["matrix_file_sha256"] == sha(backup) == digest
    assert created["bytes"] == target.stat().st_size == backup.stat().st_size and sha(target) == digest
    refused = run("backup-new-only", [*backup_argv, backup], 2)
    assert "already exists" in refused["error"] and sha(backup) == digest
    target.rename(args.output / "original-offline.qmx")
    target.parent.rmdir()

    argv = ["project-matrix-restore", project, "1", association_id, backup]
    run("inspect-missing", ["project-matrix-info", project, "1", association_id], 2)
    missing_output = args.output / "missing-source-output.qmx"
    run("backup-unavailable-source", [*backup_argv, missing_output], 2)
    assert not missing_output.exists()
    failed = run("wrong-backup", [*argv[:-1], wrong], 2)
    assert "exact file digest differs" in failed["error"] and not target.parent.exists()
    failed = run("unknown-association", [*argv[:3], "0" * 64, backup], 2)
    assert "association not found" in failed["error"].lower()

    restored = run("restore-missing-folder", argv)
    assert restored["state"] == "restored" and sha(target) == digest
    assert Path(restored["path"]) == target and restored["file_sha256"] == digest
    stamp = target.stat().st_mtime_ns
    retry = run("idempotent-retry", argv)
    assert retry["state"] == "already-present" and target.stat().st_mtime_ns == stamp

    preserved = args.output / "first-restored.qmx"
    target.rename(preserved)
    target.write_bytes(b"unrelated destination bytes")
    failure = run("existing-conflict", argv, 2)
    assert "digest differs" in failure["error"] and target.read_bytes() == b"unrelated destination bytes"
    refused = run("backup-conflicting-source", [*backup_argv, missing_output], 2)
    assert "digest differs" in refused["error"] and not missing_output.exists()
    target.rename(args.output / "conflict-retained.qmx")
    wal = target.with_name(target.name + "-wal")
    wal.write_bytes(b"active writer sentinel")
    failure = run("existing-sqlite-sidecar", argv, 2)
    assert "target has SQLite sidecars" in failure["error"] and not target.exists()
    assert wal.read_bytes() == b"active writer sentinel"
    wal.rename(args.output / "wal-retained")

    with subprocess.Popen([args.host, *argv], env=env, cwd=args.output, encoding="utf-8",
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE) as first_attempt:
        alongside = run("concurrent-second", argv)
        stdout, stderr = first_attempt.communicate(timeout=45)
        concurrent = record("concurrent-first", subprocess.CompletedProcess(argv, first_attempt.returncode, stdout, stderr))
    assert {concurrent["state"], alongside["state"]} == {"restored", "already-present"}
    assert sha(target) == sha(backup) == sha(preserved) == digest

    run("offline-native-reopen", [args.probe, project, association_id, digest, args.reader_manifest,
        "counts", "org.qupathxx.csv-matrix", 'Gene "B"', "Gene,A"], native=False)
    again = run("post-restore-list", ["project-matrices", project, "1"])
    assert again == catalog and sha(project) == project_sha
    external_sha = sha(args.external_project)
    external, = run("external-list", ["project-matrices", args.external_project, "1"])["matrices"]
    assert not external["owned"]
    failure = run("external-refusal", ["project-matrix-restore", args.external_project, "1",
                                      external["association_id"], backup], 2)
    assert "requires an owned project association" in failure["error"]
    external_backup = args.output / "external-backup.qmx"
    saved_external = run("backup-external", ["project-matrix-backup", args.external_project, "1",
                                            external["association_id"], external_backup])
    assert sha(external_backup) == sha(Path(external["path"])) == saved_external["matrix_file_sha256"] == external["file_sha256"]
    standalone = run("external-backup-reopen", ["matrix-info", external_backup])
    assert standalone["source_sha256"] == external["association_id"]
    assert sha(args.external_project) == external_sha and sha(exact) == digest
    assert not list(args.output.rglob("*.qpx-tmp-*"))

    report = {"passed": True, "host_sha256": sha(args.host), "project_sha256": project_sha, "checks": checks}
    (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} saved matrix recovery checks passed")


if __name__ == "__main__":
    main()
