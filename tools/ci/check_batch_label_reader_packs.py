"""Run mixed-reader batch and PNG label workflows on an existing two-ROI project."""
import argparse
from contextlib import closing
import csv
from hashlib import sha256
import io
import json
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import zlib


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def query(path, sql):
    with closing(sqlite3.connect(Path(path).as_uri() + "?mode=ro", uri=True)) as db:
        return db.execute(sql).fetchall()


def write_mask(path):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    header = struct.pack(">IIBBBBB", 2, 2, 8, 0, 0, 0, 0)
    data = zlib.compress(bytes((0, 7, 7, 0, 7, 7)))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
                     chunk(b"IDAT", data) + chunk(b"IEND", b""))


def measurements(path):
    [(raw,)] = query(path, "SELECT csv FROM qpx_batch_checkpoint")
    rows = csv.DictReader(io.StringIO(raw.decode("utf-8"), newline=""))
    # Separate executions capture the same file at different wall-clock times.
    return [{key: value for key, value in row.items() if key != "source_observed_unix_ms"}
            for row in rows]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("prior", "host", "envi", "fits", "png", "project", "output"):
        parser.add_argument(f"--{key}", type=lambda value: Path(value).resolve(), required=True)
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True)
    known_files = (args.envi, args.fits, args.png, args.project)
    input_hashes = {str(path): digest(path) for path in known_files}
    system = Path(os.environ["SYSTEMROOT"])
    env = {k: v for k, v in os.environ.items() if not k.startswith(("QPX_", "QUPATHXX_", "PYTHON"))}
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    batch = args.host / "qupathxx_batch.exe"
    labels = args.host / "qupathxx_labels.exe"

    def flags(*manifests):
        return [arg for path in manifests for arg in ("--reader-plugins", path)]

    def run(executable, *arguments, status=0):
        command = [executable, *arguments]
        result = subprocess.run(command, cwd=root, env=env, encoding="utf-8",
                                capture_output=True, timeout=60)
        assert result.returncode == status, (command, result.returncode, result.stdout, result.stderr)
        return result

    def batch_report(executable, manifests, folder, selection=()):
        result = run(executable, *flags(*manifests), args.project, root / folder,
                     "--kind", "raw", "--raw-channels", "1", "--raw-explicit-axes", *selection)
        return json.loads(result.stdout.splitlines()[-1])

    batch_refs = {}
    label_refs = {}
    links_sql = "SELECT image_id,payload FROM plugin_links ORDER BY image_id"
    saved_links = dict(query(args.project, links_sql))
    raw_sql = "SELECT image_id,id FROM raw_analysis_records ORDER BY image_id,id"
    for image, manifest in (("1", args.envi), ("2", args.fits)):
        reference = batch_report(args.prior / batch.name, (manifest,), f"baseline-{image}", ("--image", image))
        assert reference["state"] == "succeeded" and reference["saved"] == 1
        batch_refs[int(image)] = reference["images"][0]
        capture = ("--label-input", args.project, "--image", image, "--roi", "1", "--channels", "1")
        old = run(args.prior / labels.name, *flags(manifest), *capture)
        label_refs[image] = json.loads(old.stdout)
        assert label_refs[image]["state"] == "input"
        for executable in (batch, labels):
            result = run(executable, *flags(args.fits, args.envi), *capture)
            assert json.loads(result.stdout) == label_refs[image]

    combined = batch_report(batch, (args.envi, args.fits), "combined")
    assert combined["state"] == "succeeded" and combined["saved"] == 2 and combined["failed"] == 0
    for image in combined["images"]:
        expected = batch_refs[image["image_id"]]
        assert image["state"] == "saved" and image["records"] == 1
        assert all(image[key] == expected[key] for key in ("input_sha256", "pixel_sha256", "records"))
        assert query(image["path"], raw_sql) == query(expected["path"], raw_sql)
        assert measurements(image["path"]) == measurements(expected["path"])
        assert query(image["path"], links_sql) == [(str(image["image_id"]), saved_links[str(image["image_id"])])]
    checkpoints = {item["path"]: digest(Path(item["path"])) for item in combined["images"]}
    resumed = batch_report(batch, (args.fits, args.envi), "combined")
    assert resumed["reused"] == 2 and resumed["failed"] == 0
    assert [item["path"] for item in resumed["images"]] == [item["path"] for item in combined["images"]]
    assert checkpoints == {path: digest(Path(path)) for path in checkpoints}

    mask = root / "mask.png"
    write_mask(mask)
    prior = args.project
    imports = {}
    for executable, image in ((labels, "1"), (batch, "2")):
        marked = root / f"labels-{image}.qpx"
        result = run(executable, *flags(args.envi, args.png, args.fits),
                     "--import-label-mask", prior, marked, "--image", image, "--roi", "1",
                     "--channels", "1", "--mask", mask, "--mask-format", "png",
                     "--producer", "synthetic-test-mask", "--producer-version", "1",
                     "--model-sha256", digest(mask), "--input-sha256", label_refs[image]["input_sha256"])
        saved = json.loads(result.stdout)
        assert saved["state"] == "saved" and saved["regions"] == saved["published_objects"] == 1
        imports[image] = saved
        prior = marked
    for image, saved in imports.items():
        result = run(labels, *flags(args.fits, args.envi, args.png), "--verify-labels",
                     prior, "--image", image, "--record", saved["record"])
        verified = json.loads(result.stdout)
        assert verified["state"] == "verified"
        assert all(saved[key] == verified[key] for key in ("record", "input_sha256", "raster_sha256"))
    assert dict(query(prior, links_sql)) == saved_links

    for executable, code in ((batch, 2), (labels, 1)):
        malformed = run(executable, *flags(args.envi), "--reader-plugins", status=code)
        assert "Missing absolute manifest" in malformed.stderr
        duplicate = run(executable, *flags(args.envi, args.envi), "--help", status=code)
        assert "Duplicate reader plugin manifest path" in duplicate.stderr
    assert input_hashes == {str(path): digest(path) for path in known_files}
    receipt = {"passed": True, "batch": combined, "resume": resumed, "label_input": label_refs,
               "label_import": imports, "checkpoint_sha256": checkpoints, "bootstrap_errors_checked": True,
               "sha256": {**input_hashes, str(batch): digest(batch), str(labels): digest(labels)}}
    (root / "validation.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print("Mixed-reader batch, resume, label captures, PNG import and replay passed")


if __name__ == "__main__":
    main()
