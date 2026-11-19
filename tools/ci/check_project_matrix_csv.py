"""Verify saved-matrix CSV values and re-import through the native CSV provider."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "manifest", "reader-manifest", "template",
                "csv-validation", "sparse-project", "nonspatial-project", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, argv, expected=0, *, native=True):
        proc = subprocess.run([args.host, *argv] if native else argv, cwd=args.output, env=env,
                              capture_output=True, encoding="utf-8", timeout=45)
        assert proc.returncode == expected, (label, proc.stdout, proc.stderr)
        result = json.loads(proc.stdout if expected == 0 else proc.stderr) if native else proc.stdout.strip()
        checks[label] = result
        return result

    def export(label, project, association, output, features, expected=0):
        flags = [arg for identifier in features for arg in ("--feature", identifier)]
        return run(label, ["project-matrix-export-csv", project, "1", association, output, *flags], expected)

    def export_positions(label, project, association, output, expected=0):
        return run(label, ["project-matrix-export-positions", project, "1", association, output], expected)

    def table(path, delimiter=","):
        with path.open("r", encoding="utf-8", newline="") as stream:
            return list(csv.reader(stream, delimiter=delimiter))

    verified = json.loads(args.csv_validation.read_bytes())
    files = args.csv_validation.parent
    reader = ["--reader-plugins", args.reader_manifest]
    template_sha = sha(args.template)
    provider = "org.qupathxx.csv-matrix"
    features = ["Gene,A", 'Gene "B"']
    for domain in ("counts", "continuous"):
        project = files / (domain + ".qpxproj")
        initial = verified["reopens"][domain]
        association = initial["association_id"]
        managed = files / ".qpx-expression" / (initial["file_sha256"] + ".qmx")
        original = sha(project), sha(managed)
        output = args.output / (domain + ".csv")
        receipt = export(domain, project, association, output, features)
        assert receipt["state"] == "exported" and receipt["matrix_file_sha256"] == original[1]
        assert receipt["feature_ids"] == features and receipt["bytes"] == output.stat().st_size
        header, *rows = table(output)
        assert header == ["observation_id", *features]
        assert [row[0] for row in rows] == ["0001", "1", "μ"]
        if domain == "counts":
            assert [(int(a), int(b)) for _, a, b in rows] == [(0, 5), (2**53 + 1, 0), (2**64 - 1, 7)]
        else:
            expected = [(-0.0, 2**-149), (1.0 + 2**-52, -7.5), (2**-1074, 6.5)]
            assert [struct.pack("<dd", float(a), float(b)) for _, a, b in rows] == [
                struct.pack("<dd", *pair) for pair in expected]

        original_csv = sha(output)
        error = export("existing-" + domain, project, association, output, features, 2)
        assert "already exists" in error["error"] and sha(output) == original_csv
        if domain == "counts":
            for label, target, selected, diagnostic in (
                ("alias-source", managed, features, "must not replace its input"),
                ("alias-project", project, features, "must not replace its input"),
                ("unknown-feature", args.output / "unknown.csv", ["missing"], "Unknown expression feature ID"),
                ("duplicate-feature", args.output / "duplicate.csv", [features[0]] * 2, "duplicate"),
                ("empty-selection", args.output / "empty.csv", [], "requires"),
            ):
                error = export(label, project, association, target, selected, 2)
                assert diagnostic in error["error"], error
                if target not in (project, managed):
                    assert not target.exists()

        qmx = args.output / (domain + ".qmx")
        new_project = args.output / (domain + ".qpxproj")
        positions = args.output / (domain + ".tsv")
        coordinate_report = export_positions("export-positions-" + domain, project, association, positions)
        assert coordinate_report["frame_sha256"] == initial["frame_sha256"]
        assert coordinate_report["matrix_file_sha256"] == original[1]
        assert coordinate_report["plane"] == {"channel": -1, "z": 0, "time": 0}
        assert coordinate_report["bytes"] == positions.stat().st_size
        header, *coordinate_rows = table(positions, "\t")
        assert header == ["observation_id", "x_px", "y_px"]
        assert [(key, float(x), float(y)) for key, x, y in coordinate_rows] == [
            ("0001", .5, .75), ("1", 1.25, 1.25), ("μ", 3.5, 2.5)]
        position_sha = sha(positions)
        again = export_positions("positions-exists-" + domain, project, association, positions, 2)
        assert "already exists" in again["error"] and sha(positions) == position_sha
        if domain == "counts":
            qmx_wal = managed.with_name(managed.name + "-wal")
            for label, target in (("positions-alias-qmx", managed), ("positions-alias-project", project),
                                  ("positions-create-wal", qmx_wal)):
                refused = export_positions(label, project, association, target, 2)
                assert "must not replace its input" in refused["error"], refused
            assert not qmx_wal.exists()
        options = {"observation_id": "observation_id", "feature_columns": features[::-1],
                   "value_domain": receipt["value_domain"]}
        imported = run("reimport-" + domain, [*reader, "--format-plugins", args.manifest, "matrix-project-index",
                       provider, output, positions, args.template, "1", qmx,
                       "--plugin-options-json", json.dumps(options)])
        assert imported["receipt"]["source_sha256"] == original_csv
        assert imported["frame_sha256"] == initial["frame_sha256"]
        assert imported["positions_sha256"] == position_sha
        digest = imported["file_sha256"]
        saved = run("reattach-" + domain, [*reader, "matrix-attach", args.template, "1", qmx, digest, new_project])
        qmx.rename(qmx.with_suffix(".offline"))
        output.rename(output.with_suffix(".offline-csv"))
        positions.rename(positions.with_suffix(".offline-tsv"))
        run("offline-reopen-" + domain,
            [args.probe, new_project, saved["association_id"], digest, args.reader_manifest,
             domain, provider, *features[::-1]], native=False)
        assert (sha(project), sha(managed)) == original

    sparse = run("sparse-association", ["project-matrices", args.sparse_project, "1"])["matrices"]
    assoc, = sparse
    assert (assoc["entries"], assoc["observations"], assoc["features"]) == (1024, 1024, 1024)
    sparse_path = args.output / "sparse.csv"
    originals = sha(args.sparse_project), sha(Path(assoc["path"]))
    export("sparse-export", args.sparse_project, assoc["association_id"], sparse_path, ["feature-0512", "feature-0000"])
    header, *rows = table(sparse_path)
    assert header == ["observation_id", "feature-0512", "feature-0000"]
    assert rows == [[f"obs-{i:04}", "513" if i == 512 else "0", "1" if i == 0 else "0"] for i in range(1024)]
    sparse_positions = args.output / "sparse.tsv"
    export_positions("sparse-positions", args.sparse_project, assoc["association_id"], sparse_positions)
    header, *rows = table(sparse_positions, "\t")
    assert header == ["observation_id", "x_px", "y_px"]
    assert rows == [[f"obs-{i:04}", str(i % 32), str(i // 32)] for i in range(1024)]
    assert (sha(args.sparse_project), sha(Path(assoc["path"]))) == originals

    no_frame, = run("nonspatial-association", ["project-matrices", args.nonspatial_project, "1"])["matrices"]
    no_output = args.output / "nonspatial.tsv"
    guarded = export_positions("nonspatial-refusal", args.nonspatial_project, no_frame["association_id"], no_output, 2)
    assert "requires a stored level-zero image pixel frame" in guarded["error"] and not no_output.exists()

    edge_source = args.output / "edge-source.tsv"
    edge_source.write_text("observation_id\tx_px\ty_px\nμ\t3.5\t2.5\n1\t1.0000000000000002\t1.25\n"
                           "0001\t-0\t5e-324\n", encoding="utf-8")
    edge_qmx = args.output / "edge.qmx"
    opts = {"observation_id": "observation_id", "feature_columns": features[::-1], "value_domain": "raw-counts"}
    edge = run("edge-import", [*reader, "--format-plugins", args.manifest, "matrix-project-index", provider,
                               args.output / "counts.offline-csv", edge_source, args.template, "1", edge_qmx,
                               "--plugin-options-json", json.dumps(opts)])
    edge_project = args.output / "edge.qpxproj"
    attached = run("edge-attach", [*reader, "matrix-attach", args.template, "1", edge_qmx,
                                  edge["file_sha256"], edge_project])
    edge_source.rename(edge_source.with_suffix(".offline-tsv"))
    edge_qmx.rename(edge_qmx.with_suffix(".offline"))
    edge_output = args.output / "edge-export.tsv"
    export_positions("edge-export", edge_project, attached["association_id"], edge_output)
    _, *rows = table(edge_output, "\t")
    assert [row[0] for row in rows] == ["0001", "1", "μ"]
    assert [struct.pack("<dd", float(x), float(y)) for _, x, y in rows] == [
        struct.pack("<dd", x, y) for x, y in ((-0., 2**-1074), (1.0 + 2**-52, 1.25), (3.5, 2.5))]
    assert sha(args.template) == template_sha and not list(args.output.glob("*.qpx-tmp-*"))
    report = {"passed": True, "host_sha256": sha(args.host), "checks": checks}
    (args.output / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} exact saved-matrix CSV workflow checks passed")


if __name__ == "__main__":
    main()
