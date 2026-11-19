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
    parser.add_argument("--spatial", action="store_true",
                        help="Restore positions and the re-indexed matrix on an empty saved image")
    parser.add_argument("--position-reexport", action="store_true",
                        help="Verify exact re-export from the restored point frame")
    args = parser.parse_args()
    if args.position_reexport and not args.spatial:
        parser.error("--position-reexport requires --spatial")
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

    with ZipFile(archive) as source:
        for name in ("matrix.mtx", "features.tsv", "barcodes.tsv"):
            (args.output / name).write_bytes(source.read(name))
    imported = args.output / "reimported.qmx"
    if args.reimport:
        result = subprocess.run(
            [args.reimport, "matrix.mtx", "features.tsv", "barcodes.tsv", imported],
            env=env, cwd=args.output, capture_output=True, encoding="utf-8", timeout=120)
        assert result.returncode == 0, (result.stdout, result.stderr)
        checks["native-reimport"] = {"file_sha256": sha(imported)}
    else:
        index = ("matrix-market-index", "matrix.mtx", "features.tsv", "barcodes.tsv")
        receipt = run("native-reimport", *index, imported, "--catalogue", "tenx")
        assert receipt["state"] == "indexed" and receipt["file_sha256"] == sha(imported)
        assert receipt["input_profile"] == "tenx-tsv" and receipt["domain"] == "counts"
        reopened = run("reopened-index", "matrix-info", imported)
        assert all(receipt[key] == reopened[key] for key in
                   ("source_sha256", "file_sha256", "features", "observations", "entries", "input_profile"))

        destination = args.output / "refused.qmx"
        run("missing-catalogue", *index, destination, status=2)
        run("unknown-catalogue", *index, destination, "--catalogue", "auto", status=2)
        run("wrong-catalogue", *index, destination, "--catalogue", "qpx", status=2)
        assert not destination.exists()
        old_digest = sha(imported)
        run("index-existing-destination", *index, imported, "--catalogue", "tenx", status=2)
        assert sha(imported) == old_digest

        wal = destination.with_name(destination.name + "-wal")
        wal.write_bytes(b"Reserved SQLite companion")
        refusal = run("index-existing-companion", *index, destination, "--catalogue", "tenx", status=2)
        assert "SQLite companion" in refusal["error"] and not destination.exists()
        assert wal.read_bytes() == b"Reserved SQLite companion"

        features, barcodes, counts = expected
        (args.output / "qpx-features.tsv").write_bytes(
            ("feature_id\n" + "".join(row[0] + "\n" for row in features)).encode("utf-8"))
        (args.output / "qpx-observations.tsv").write_bytes(
            ("observation_id\n" + "".join(name + "\n" for name in barcodes)).encode("utf-8"))
        qpx_output = args.output / "qpx-catalogue.qmx"
        qpx = run("native-qpx-index", "matrix-market-index", "matrix.mtx", "qpx-features.tsv",
                  "qpx-observations.tsv", qpx_output, "--catalogue", "qpx")
        assert qpx["input_profile"] == "qpx-tsv"
        assert stored_rows(qpx_output) == ([(row[0], row[0], "Gene Expression") for row in features], barcodes, counts)

        duplicate = args.output / "duplicate.mtx"
        duplicate.write_bytes(("%%MatrixMarket matrix coordinate integer general\n" +
                               f"{len(features)} {len(barcodes)} 2\n1 1 2\n1 1 3\n").encode("ascii"))
        duplicate_output = args.output / "duplicate.qmx"
        refusal = run("duplicate-coordinate", "matrix-market-index", duplicate, "features.tsv",
                      "barcodes.tsv", duplicate_output, "--catalogue", "tenx", status=2)
        assert "duplicate" in refusal["error"].lower() and not duplicate_output.exists()

    assert stored_rows(imported) == expected

    if args.spatial:
        positions = args.output / "positions.tsv"
        position_receipt = run("export-positions", "project-matrix-export-positions", args.project,
                               args.image, association, positions)
        plane = position_receipt["plane"]
        assert plane["channel"] == -1
        frame = ("--frame-z", str(plane["z"]), "--frame-time", str(plane["time"]))
        _, *rows = positions.read_text(encoding="utf-8").splitlines()
        coordinates = {row[0]: [float(row[1]), float(row[2])] for row in (line.split("\t") for line in rows)}
        assert len(coordinates) == len(expected[1]) and set(coordinates) == set(expected[1])

        restore = ("project-matrix-import-positions", args.project, args.image, imported)
        mismatch = args.output / "wrong-positions.tsv"
        mismatch.write_bytes(("observation_id\tx_px\ty_px\n" + "absent-from-matrix\t" + rows[0].split("\t", 1)[1] +
                              "\n" + "".join(line + "\n" for line in rows[1:])).encode("utf-8"))
        refused = args.output / "wrong-positions.qpx"
        run("wrong-position-id", *restore, mismatch, refused, *frame, status=2)
        assert not refused.exists()

        received = args.output / "received.qpx"
        imported_digest = sha(imported)
        saved = run("restore-spatial-project", *restore, positions, received, *frame)
        assert saved["state"] == "saved" and saved["owned"] and saved["observations"] == len(coordinates)
        received_digest = sha(received)
        run("restore-preserves-existing-project", *restore, positions, received, *frame, status=2)
        assert sha(received) == received_digest

        # Move only the authored outputs of this test to simulate losing the handoff inputs.
        offline = args.output / "offline-inputs"
        offline.mkdir()
        imported.rename(offline / imported.name)
        positions.rename(offline / positions.name)
        received_id = saved["matrix_source_sha256"]
        owned, = [item for item in run("restored-catalogue", "project-matrices", received, args.image)["matrices"]
                  if item["association_id"] == received_id]
        assert owned["owned"] and owned["file_sha256"] == imported_digest
        assert stored_rows(Path(owned["path"]).resolve()) == expected
        reexport = args.output / "reexport.zip"
        run("reexport-offline-original", "project-matrix-export-mtx", received, args.image, received_id, reexport)
        assert sha(reexport) == archive_sha
        geometry = args.output / "received.geojson"
        run("export-restored-points", "geojson-export", received, args.image, geometry)
        points = json.loads(geometry.read_bytes())["features"]
        assert len(points) == len(coordinates) and all(point["geometry"]["type"] == "Point" for point in points)
        assert {point["properties"]["name"]:point["geometry"]["coordinates"] for point in points} == coordinates
        if args.position_reexport:
            replay = args.output / "replayed-positions.tsv"
            export_positions = ("project-matrix-export-positions", received, args.image, received_id)
            selection = ("--frame-id", saved["coordinate_frame"], *frame)
            receipt = run("reexport-backed-positions", *export_positions, replay, *selection)
            assert receipt["coordinate_frame"] == saved["coordinate_frame"] and receipt["plane"] == plane
            assert receipt["matrix_file_sha256"] == imported_digest
            assert replay.read_bytes() == (offline / positions.name).read_bytes()
            run("backed-positions-existing-output", *export_positions, replay, *selection, status=2)
            assert replay.read_bytes() == (offline / positions.name).read_bytes()
            bad_frame = args.output / "refused-frame.tsv"
            run("backed-positions-wrong-frame", *export_positions, bad_frame,
                "--frame-id", "unrelated", *frame, status=2)
            assert not bad_frame.exists()
            assert sha(Path(owned["path"])) == imported_digest
        assert sha(received) == received_digest and (sha(args.project), sha(sidecar)) == original

    assert not list(args.output.glob("*.qpx-tmp-*"))
    assert not list(args.output.glob(".qpx-expression-*"))
    report = {"passed": True, "host_sha256": sha(args.host), "archive_sha256": archive_sha,
              "checks": checks}
    (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} exact Matrix Market export checks passed")


if __name__ == "__main__":
    main()
