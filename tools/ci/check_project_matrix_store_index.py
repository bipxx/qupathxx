"""Verify sparse AnnData Zarr indexing and attachment with a saved ENVI reader."""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess

from check_project_matrix_index import sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "previous-host", "probe", "template", "reader-manifest", "packet", "baseline", "output"):
        parser.add_argument("--" + key, required=True, type=lambda path: Path(path).resolve())
    args = parser.parse_args()
    root = args.output
    inputs = root / "input data"
    inputs.mkdir(parents=True)
    project = args.template
    header = project.with_name("study.hdr")
    companion = project.with_name("study.img")
    manifest = args.packet / "package-sparse-final/bin/anndata-zarr-format-plugins.json"
    baseline = json.loads(args.baseline.read_bytes())
    for path, field in ((project, "template_sha"), (args.reader_manifest, "reader_manifest_sha"),
                        (manifest, "matrix_manifest_sha"), (args.previous_host, "host_sha")):
        assert sha(path) == baseline[field].lower()
    fixture = json.loads((args.packet / "integration-sparse-02/results.json").read_bytes())
    case = fixture["checks"]["upstream-wide-csc"]
    expected = json.loads(case["stdout"])
    source = Path(case["argv"][5])
    store = inputs / source.name
    shutil.copytree(source, store)
    positions = inputs / "positions.tsv"
    positions.write_bytes(("observation_id\tx_px\ty_px\n" + "".join(
        f"axis-{i:05}\t{i % 4}\t{i // 4 % 3}\n" for i in reversed(range(10240))
    )).encode())
    original = {path: sha(path) for path in (project, header, companion, positions)}
    store_hashes = {path.relative_to(store): sha(path) for path in store.rglob("*") if path.is_file()}
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(os.environ["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, *cmd, status=0, message=None):
        p = subprocess.run(cmd, cwd=root, env=env, capture_output=True, encoding="utf-8", timeout=60)
        assert p.returncode == status, (label, p.returncode, p.stdout, p.stderr)
        if message:
            assert message in p.stderr, (label, p.stderr)
        checks[label] = {"exit_code": p.returncode, "stdout": p.stdout.strip(), "stderr": p.stderr.strip()}
        return p.stdout.strip()

    reader = [args.host, "--reader-plugins", args.reader_manifest]
    def index(target, key=".zattrs"):
        return [*reader, "--format-plugins", manifest, "matrix-project-store-index", "org.qupathxx.anndata-zarr",
                store, key, positions, project, "1", target, "--plugin-options-json", case["argv"][-1]]

    # The original image-path route reuses the exact same admitted directory and
    # position files under both hosts. The report has only a different save path.
    control = case["argv"][3:]
    control_label = []
    for host, label in ((args.previous_host, "image-path-old"), (args.host, "image-path-new")):
        argv = [*control]
        path = root / (label + ".qmx")
        argv[6] = path
        report = json.loads(run(label, host, "--format-plugins", manifest, *argv))
        assert report.pop("output") == str(path)
        assert report["file_sha256"] == sha(path)
        control_label.append(report)
    assert control_label[0] == control_label[1]

    target = root / "indexed.qmx"
    old_mode = project.stat().st_mode
    try:
        project.chmod(stat.S_IREAD)
        inside = store / "invalid.qmx"
        run("protect-store", *index(inside), status=2, message="protected input directory")
        assert not inside.exists()
        run("protect-companion", *index(companion), status=2, message="protected")
        run("protect-template", *index(project), status=2, message="protected")
        run("protect-positions", *index(positions), status=2)
        run("absent-primary-key", *index(target, "absent"), status=2)
        assert not target.exists()
        indexed = json.loads(run("index-readonly-project", *index(target)))
        assert indexed["command"] == "matrix-project-store-index"
        assert indexed["frame_sha256"] != expected["frame_sha256"]
        for field in ("entries", "features", "observations", "domain", "input_profile", "value_domain",
                      "source_scope", "primary_key", "store_content_sha256", "store_files", "store_bytes"):
            assert indexed[field] == expected[field], (field, indexed[field], expected[field])
        assert indexed["positions_sha256"] == original[positions]
        digest = indexed["file_sha256"]
        assert digest == sha(target)
        run("protect-existing-qmx", *index(target), status=2, message="already exists")
        assert sha(target) == digest
        assert {p.relative_to(store): sha(p) for p in store.rglob("*") if p.is_file()} == store_hashes
        assert sha(positions) == original[positions]
        offline = root / "unavailable"
        inputs.rename(offline)
        saved = root / "owned.qpxproj"
        attached = json.loads(run("attach-offline", *reader, "matrix-attach", project, "1", target, digest, saved))
        assert attached["owned"] and attached["file_sha256"] == digest
        assert attached["frame_sha256"] == indexed["frame_sha256"] and attached["source_sha256"] == indexed["source_sha256"]
        managed = root / ".qpx-expression" / (digest + ".qmx")
        assert sha(managed) == digest
        target.rename(offline / target.name)
        reopened = json.loads(run("offline-store-receipt", args.host, "matrix-info", managed))
        for field in ("receipt", "positions_sha256", "store_content_sha256", "store_observed_sha256",
                      "primary_key", "store_files", "store_bytes", "file_sha256", "source_sha256"):
            assert reopened[field] == indexed[field], field
        run("fresh-owned-reopen", args.probe, "--owned-anndata-wide", saved, "1", attached["association_id"],
            digest, args.reader_manifest)
    finally:
        project.chmod(old_mode)
    assert all(sha(p) == original[p] for p in (project, header, companion))
    (root / "results.json").write_text(json.dumps({
        "passed": True, "checks": checks, "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
        "qmx_sha256": digest, "retained_project_sha256": sha(project), "store_members": len(store_hashes),
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} project store-index checks passed")


if __name__ == "__main__":
    main()
