"""Test project-based matrix indexing with the saved ENVI reader."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess


def sha(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "envi-probe", "reader-manifest", "matrix-manifest", "baseline", "scale", "output"):
        parser.add_argument("--" + key, required=True, type=lambda path: Path(path).resolve())
    args = parser.parse_args()
    root = args.output
    root.mkdir()
    source = root / "original inputs"
    source.mkdir()
    counts = source / "scale.h5mu"
    positions = source / "positions.tsv"
    shutil.copyfile(args.scale / counts.name, counts)
    shutil.copyfile(args.scale / positions.name, positions)
    project = args.baseline.with_name("template") / "study.qpxproj"
    header = project.with_name("study.hdr")
    companion = project.with_name("study.img")
    baseline = json.loads(args.baseline.read_bytes())
    assert baseline["exit_code"] == 2 and sha(project) == baseline["template_sha"].lower()
    assert not (args.host.parent / "qpx_mudata_plugin.dll").exists()
    before = {p: sha(p) for p in (project, header, companion, counts, positions)}
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(os.environ["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, *cmd, status=0, message=None):
        p = subprocess.run(cmd, cwd=root, env=env, capture_output=True, encoding="utf-8", timeout=45)
        assert p.returncode == status, (label, p.returncode, p.stdout, p.stderr)
        if message:
            assert message in p.stderr, (label, p.stderr)
        checks[label] = {"exit_code": p.returncode, "stdout": p.stdout.strip(), "stderr": p.stderr.strip()}
        return p.stdout.strip()

    opts = '{"modality":"rna","matrix":"X","value_domain":"raw-counts"}'
    reader = [args.host, "--reader-plugins", args.reader_manifest]
    index = [*reader, "--format-plugins", args.matrix_manifest, "matrix-project-index", "org.qupathxx.mudata"]

    def import_args(target, image="1", template=project):
        return [*index, counts, positions, template, image, target, "--plugin-options-json", opts]

    qmx = root / "indexed.qmx"
    old_mode = project.stat().st_mode
    try:
        project.chmod(stat.S_IREAD)
        run("unknown-image", *import_args(qmx, image="999"), status=2)
        assert not qmx.exists()
        for label, path in (("template", project), ("header", header), ("companion", companion),
                            ("matrix", counts), ("positions", positions)):
            run("protect-" + label, *import_args(path), status=2, message="protected")
            assert sha(path) == before[path]
        managed_input = project.parent / ".qpx-expression" / "invalid.qmx"
        run("protect-managed-namespace", *import_args(managed_input), status=2, message="protected input directory")
        assert not managed_input.exists()
        report = json.loads(run("index-readonly-project", *import_args(qmx)))
        assert report["input_profile"] == "external-sparse-file/1" and report["entries"] == 3
        assert report["positions_sha256"] == before[positions]
        assert report["receipt"]["source_sha256"] == before[counts]
        digest = report["file_sha256"]
        assert digest == sha(qmx)
        run("protect-existing-qmx", *import_args(qmx), status=2, message="already exists")
        assert sha(qmx) == digest

        stale = root / "stale project"
        stale.mkdir()
        shutil.copyfile(header, stale / header.name)
        shutil.copyfile(companion, stale / companion.name)
        run("create-stale-control", args.envi_probe, "create", args.reader_manifest, stale)
        changed = stale / companion.name
        with changed.open("r+b") as stream:
            original = stream.read(1)
            stream.seek(0)
            stream.write(bytes([original[0] ^ 1]))
        rejected = root / "stale.qmx"
        run("reject-changed-companion", *import_args(rejected, template=stale / project.name), status=2, message="changed")
        assert not rejected.exists()
        assert all(sha(path) == expected for path, expected in before.items())

        unavailable = root / "unavailable"
        source.rename(unavailable)
        saved = root / "owned.qpxproj"
        attached = json.loads(run("attach-without-matrix-provider", *reader, "matrix-attach",
                                  project, "1", qmx, digest, saved))
        assert attached["owned"] and attached["file_sha256"] == digest
        assert attached["source_sha256"] == report["source_sha256"] and attached["frame_sha256"] == report["frame_sha256"]
        managed = root / ".qpx-expression" / (digest + ".qmx")
        assert sha(managed) == digest
        qmx.rename(unavailable / qmx.name)
        run("fresh-owned-reopen", args.probe, "--owned-scale", saved, "1", attached["association_id"],
            digest, args.reader_manifest)
    finally:
        project.chmod(old_mode)
    assert all(sha(path) == before[path] for path in (project, header, companion))
    assert not any(project.with_name(project.name + s).exists() for s in ("-journal", "-wal", "-shm"))
    result = {"passed": True, "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
              "template_sha256": sha(project), "qmx_sha256": digest, "checks": checks}
    (root / "results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} project-index checks passed")


if __name__ == "__main__":
    main()
