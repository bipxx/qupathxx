"""Check native saved-count export against the exact stored sparse rows."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
from zipfile import ZIP_STORED, ZipFile


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def stored_rows(path):
    connection = sqlite3.connect(path.as_uri() + "?mode=ro&immutable=1", uri=True)
    try:
        features = connection.execute("SELECT idx,id,name,type FROM features ORDER BY idx").fetchall()
        barcodes = connection.execute("SELECT idx,id FROM observations ORDER BY idx").fetchall()
        counts = [(feature + 1, observation + 1, int.from_bytes(value, "little"))
                  for feature, observation, value in connection.execute(
                      "SELECT feature,observation,value FROM counts ORDER BY feature,observation")]
    finally:
        connection.close()
    assert [idx for idx, *_ in features] == list(range(len(features)))
    assert [idx for idx, _ in barcodes] == list(range(len(barcodes)))
    return [row[1:] for row in features], [identifier for _, identifier in barcodes], counts


def member_lines(archive, name):
    payload = archive.read(name).decode("utf-8")
    assert payload.endswith("\n"), name
    return payload[:-1].split("\n")


def inspect_archive(path, expected):
    features, barcodes, counts = expected
    with ZipFile(path) as archive:
        assert sorted(archive.namelist()) == ["barcodes.tsv", "features.tsv", "matrix.mtx"]
        assert archive.testzip() is None
        assert all(member.compress_type == ZIP_STORED for member in archive.infolist())
        assert [tuple(line.split("\t")) for line in member_lines(archive, "features.tsv")] == features
        assert member_lines(archive, "barcodes.tsv") == barcodes
        header, *rows = member_lines(archive, "matrix.mtx")
        assert header == "%%MatrixMarket matrix coordinate integer general"
        dimensions, *rows = [line for line in rows if not line.startswith("%")]
        assert tuple(map(int, dimensions.split())) == (len(features), len(barcodes), len(counts))
        assert [tuple(map(int, line.split())) for line in rows] == counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "project", "output"):
        parser.add_argument("--" + key, type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--image", default="1")
    parser.add_argument("--association")
    parser.add_argument("--reader-manifest", action="append", default=[],
                        type=lambda value: Path(value).resolve())
    parser.add_argument("--continuous-project", type=lambda value: Path(value).resolve())
    parser.add_argument("--reimport", type=lambda value: Path(value).resolve(),
                        help="Optional native SDK probe accepting MATRIX FEATURES BARCODES NEW_QMX")
    parser.add_argument("--require-edges", action="store_true",
                        help="Require an explicit zero, 2^53+1 and UINT64_MAX in the fixture")
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    env = {key: value for key, value in os.environ.items()
           if not key.upper().startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    if os.name == "nt":
        windows = Path(env["SYSTEMROOT"])
        env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    flags = [value for path in args.reader_manifest for value in ("--reader-plugins", path)]
    checks = {}

    def run(label, *command, status=0):
        result = subprocess.run([args.host, *flags, *command], env=env, cwd=args.output,
                                capture_output=True, encoding="utf-8", timeout=120)
        assert result.returncode == status, (label, result.stdout, result.stderr)
        receipt = json.loads(result.stdout if status == 0 else result.stderr)
        checks[label] = receipt
        return receipt

    listed = run("catalogue", "project-matrices", args.project, args.image)["matrices"]
    matrix, = [record for record in listed
               if args.association is None or record["association_id"] == args.association]
    association = matrix["association_id"]
    sidecar = Path(matrix["path"]).resolve()
    original = (sha(args.project), sha(sidecar))
    assert original[1] == matrix["file_sha256"]
    expected = stored_rows(sidecar)
    if args.require_edges:
        counts = {value for _, _, value in expected[2]}
        assert {0, 2**53 + 1, 2**64 - 1} <= counts

    archive = args.output / "counts.zip"
    export_args = ("project-matrix-export-mtx", args.project, args.image, association)
    receipt = run("export", *export_args, archive)
    assert receipt["state"] == "exported" and receipt["bytes"] == archive.stat().st_size
    assert receipt["matrix_file_sha256"] == original[1] and receipt["association_id"] == association
    inspect_archive(archive, expected)
    archive_sha = sha(archive)

    repeat = args.output / "repeat.zip"
    run("deterministic-export", *export_args, repeat)
    assert sha(repeat) == archive_sha
    refusal = run("existing-destination", *export_args, archive, status=2)
    assert "already exists" in refusal["error"] and sha(archive) == archive_sha
    missing_wal = sidecar.with_name(sidecar.name + "-wal")
    for label, destination in (("project-input", args.project), ("matrix-input", sidecar),
                               ("matrix-wal", missing_wal)):
        refusal = run(label, *export_args, destination, status=2)
        assert "must not replace its input" in refusal["error"], refusal
    assert not missing_wal.exists()
    if args.output.stat().st_dev == sidecar.stat().st_dev:
        alias = args.output / "matrix-inode.zip"
        os.link(sidecar, alias)
        refusal = run("hardlink-input", *export_args, alias, status=2)
        assert "aliases its input" in refusal["error"] and sha(alias) == original[1]
        alias.unlink()
    assert (sha(args.project), sha(sidecar)) == original

    if args.continuous_project:
        catalogue = run("continuous-catalogue", "project-matrices", args.continuous_project,
                        args.image)["matrices"]
        item, = [record for index, record in enumerate(catalogue)
                 if run(f"continuous-domain-{index}", "matrix-info", record["path"])["domain"] == "continuous"]
        destination = args.output / "continuous.zip"
        refusal = run("continuous-refused", "project-matrix-export-mtx", args.continuous_project, args.image,
                      item["association_id"], destination, status=2)
        assert "counts" in refusal["error"].lower(), refusal
        assert not destination.exists()

    if args.reimport:
        with ZipFile(archive) as source:
            for name in ("matrix.mtx", "features.tsv", "barcodes.tsv"):
                (args.output / name).write_bytes(source.read(name))
        imported = args.output / "reimported.qmx"
        result = subprocess.run(
            [args.reimport, "matrix.mtx", "features.tsv", "barcodes.tsv", imported],
            env=env, cwd=args.output, capture_output=True, encoding="utf-8", timeout=120)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert stored_rows(imported) == expected
        checks["native-reimport"] = {"file_sha256": sha(imported)}

    assert not list(args.output.glob("*.qpx-tmp-*"))
    report = {"passed": True, "host_sha256": sha(args.host), "archive_sha256": archive_sha,
              "checks": checks}
    (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} exact Matrix Market export checks passed")


if __name__ == "__main__":
    main()
