"""Check read-only CLI discovery and inspection of exact saved matrix associations."""
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
    for name in ("host", "reader-manifest", "template", "csv-validation", "output"):
        parser.add_argument("--" + name, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    checks = {}
    env = {key: value for key, value in os.environ.items() if not key.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))

    def run(label, *argv, expected=0):
        result = subprocess.run([args.host, *argv], cwd=args.output, env=env, encoding="utf-8",
                                capture_output=True, timeout=45)
        assert result.returncode == expected, (label, result.returncode, result.stdout, result.stderr)
        payload = json.loads(result.stdout if expected == 0 else result.stderr)
        checks[label] = payload
        return payload

    template_sha = sha(args.template)
    empty = run("empty-project-image", "project-matrices", args.template, "1")
    assert empty["state"] == "listed" and empty["matrices"] == []
    absent = run("missing-image", "project-matrices", args.template, "2", expected=2)
    assert "image" in absent["error"].lower()
    invalid = run("invalid-image-id", "project-matrices", args.template, "0", expected=2)
    assert "positive native image ID" in invalid["error"]

    csv_fixture = json.loads(args.csv_validation.read_bytes())
    prior_sidecars = args.csv_validation.parent / ".qpx-expression"
    expected_rows = {}
    project = args.template
    for domain in ("counts", "continuous"):
        previous = csv_fixture["reopens"][domain]
        file_hash = previous["file_sha256"]
        stored_file = prior_sidecars / (file_hash + ".qmx")
        assert sha(stored_file) == file_hash
        saved_project = args.output / (domain + ".qpxproj")
        attached = run("attach-" + domain, "--reader-plugins", args.reader_manifest, "matrix-attach",
                       project, "1", stored_file, file_hash, saved_project)
        assert attached["association_id"] == previous["association_id"]
        assert attached["file_sha256"] == file_hash and attached["owned"]
        expected_rows[attached["association_id"]] = (domain, file_hash)
        project = saved_project
    project_sha = sha(project)

    listing = run("two-owned-matrices", "project-matrices", project, "1")
    rows = listing["matrices"]
    assert listing["image_id"] == 1 and Path(listing["input_project"]) == project
    assert [row["association_id"] for row in rows] == sorted(expected_rows)
    for row in rows:
        association = row["association_id"]
        domain, file_hash = expected_rows[association]
        path = args.output / ".qpx-expression" / (file_hash + ".qmx")
        assert row["owned"] and Path(row["path"]) == path and row["file_sha256"] == sha(path)
        assert row["source_sha256"] == association
        assert (row["observations"], row["features"], row["entries"]) == (3, 2, 6)
        standalone = run("standalone-" + domain, "matrix-info", path)
        inspected = run("saved-" + domain, "project-matrix-info", project, "1", association)
        assert inspected["owned"] and inspected["association_id"] == association
        assert Path(inspected["input_project"]) == project and inspected["image_id"] == 1
        assert {key: value for key, value in inspected.items() if key in standalone and key != "command"} == {
            key: value for key, value in standalone.items() if key != "command"}
        assert inspected["receipt"] == csv_fixture["indexed"][domain]["receipt"]
        assert inspected["frame_sha256"] == csv_fixture["indexed"][domain]["frame_sha256"]

    association = rows[0]["association_id"]
    path = Path(rows[0]["path"])
    retained = args.output / "retained.qmx"
    missing = run("unknown-association", "project-matrix-info", project, "1", "0" * 64, expected=2)
    assert "association not found" in missing["error"].lower()
    path.rename(retained)
    try:
        assert run("list-with-missing-qmx", "project-matrices", project, "1") == listing
        run("inspect-missing-qmx", "project-matrix-info", project, "1", association, expected=2)
        data = retained.read_bytes()
        path.write_bytes(data + b"\0")
        assert run("list-with-altered-qmx", "project-matrices", project, "1") == listing
        altered = run("inspect-altered-qmx", "project-matrix-info", project, "1", association, expected=2)
        assert "digest" in altered["error"].lower(), altered
    finally:
        shutil.copyfile(retained, path)
    assert sha(path) == rows[0]["file_sha256"]
    assert sha(project) == project_sha and sha(args.template) == template_sha
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "host_sha256": sha(args.host), "project_sha256": project_sha, "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} saved matrix catalog and exact inspection checks passed")


if __name__ == "__main__":
    main()
