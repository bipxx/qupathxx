"""Verify that project export paths protect retained QMX and QPR assets."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("batch", "format-cli", "fixtures", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    system = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join((str(system / "System32"), str(system)))
    checks = {}

    def run(label, host, *argv, expected=0):
        result = subprocess.run([host, *argv], cwd=args.output, env=env, capture_output=True,
                                encoding="utf-8", timeout=60)
        assert result.returncode == expected, (label, result.returncode, result.stdout, result.stderr)
        response = result.stdout if expected == 0 else result.stderr
        checks[label] = json.loads(response) if response else {}
        return checks[label]

    project = args.output / "source.qpx"
    shutil.copyfile(args.fixtures / "cli.qpx", project)
    source_bytes = project.read_bytes()
    retained = {}
    for kind, folder in (("qmx", ".qpx-expression"), ("qpr", ".qpx-expression-reductions")):
        source, = [path for path in (args.fixtures / folder).glob("*." + kind) if path.name.encode() in source_bytes]
        assets = args.output / folder
        assets.mkdir()
        retained[kind] = assets / source.name
        shutil.copyfile(source, retained[kind])
    matrix, archive = retained["qmx"], retained["qpr"]
    hashes = {p: sha(p) for p in (project, matrix, archive)}
    ref = (args.fixtures / "cli-scatter.svg").read_text(encoding="utf-8")
    reduction_id = re.search(r"<desc>Result ([0-9a-f]{64});", ref).group(1)
    association, = run("list-source", args.format_cli, "project-matrices", project, "1")["matrices"]

    def replace_refused(label, path, reason="must not replace its input"):
        result = run(label, args.batch, "--export-cell-table", project, path, expected=2)
        assert reason in result["error"], result

    replace_refused("existing-qmx", matrix)
    replace_refused("existing-qpr", archive)
    alias = args.output / "matrix-alias.csv"
    os.link(matrix, alias)
    replace_refused("physical-qmx-alias", alias, "aliases its input")
    assert sha(alias) == hashes[matrix]
    for suffix in ("-journal", "-wal", "-shm"):
        reserved = matrix.with_name(matrix.name + suffix)
        replace_refused("qmx" + suffix, reserved)
        assert not reserved.exists()

    saved_archive = args.output / "archive.offline"
    archive.rename(saved_archive)
    replace_refused("missing-qpr", archive)
    refused = run("retained-reduction-before-load", args.batch, "--export-expression-reduction", project, archive,
                  "--image", "1", "--record", reduction_id, "--plot", expected=2)
    assert "must not replace its input" in refused["error"] and not archive.exists()
    refused = run("matrix-csv-to-offline-qpr", args.format_cli, "project-matrix-export-csv", project, "1",
                  association["association_id"], archive, "--feature", "G0", expected=2)
    assert "must not replace its input" in refused["error"] and not archive.exists()
    saved_archive.rename(archive)

    saved_matrix = args.output / "matrix.offline"
    matrix.rename(saved_matrix)
    replace_refused("missing-qmx", matrix)
    refused = run("geojson-to-offline-qmx", args.format_cli, "geojson-export", project, "1", matrix, expected=2)
    assert "must not replace its input" in refused["error"] and not matrix.exists()
    geojson = args.output / "annotations.geojson"
    run("geojson-with-matrix-offline", args.format_cli, "geojson-export", project, "1", geojson)
    assert json.loads(geojson.read_bytes())["type"] == "FeatureCollection"
    saved_matrix.rename(matrix)

    cell_table = args.output / "cells.csv"
    cell_table.write_bytes(b"replace this independent export")
    run("valid-cell-export", args.batch, "--export-cell-table", project, cell_table)
    assert cell_table.read_bytes().startswith(b"project_identity,")
    plot = args.output / "reduction.svg"
    run("valid-reduction-export", args.batch, "--export-expression-reduction", project, plot,
        "--image", "1", "--record", reduction_id, "--plot")
    assert sha(plot) == sha(args.fixtures / "cli-scatter.svg")
    assert all(sha(path) == value for path, value in hashes.items())
    assert not list(args.output.rglob("*.qpx-tmp-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "batch_sha256": sha(args.batch), "format_cli_sha256": sha(args.format_cli),
        "protected_sha256": {str(path): digest for path, digest in hashes.items()}, "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} retained-asset protection and export checks passed")


if __name__ == "__main__":
    main()
