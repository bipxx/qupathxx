"""Verify installed native CSV and TSV matrix providers through owned-project replay."""
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
    for key in ("host", "probe", "manifest", "reader-manifest", "template", "fixture", "positions", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    inputs = args.output / "inputs"
    inputs.mkdir(parents=True)
    item, = json.loads(args.manifest.read_bytes())["plugins"]
    provider = item["provider_id"]
    assert provider in {"org.qupathxx.csv-matrix", "org.qupathxx.tsv-matrix"}
    tsv = provider == "org.qupathxx.tsv-matrix"
    dialect = {"delimiter": "\t", "quoting": csv.QUOTE_NONE, "quotechar": None} if tsv else {}
    separator = b"\t" if tsv else b","
    suffix = ".tsv" if tsv else ".csv"
    module = args.manifest.with_name(item["module"]["path"])
    identity = sha(module), sha(args.manifest), sha(args.template)
    assert item["abi"] == 2 and not item["dependencies"]
    assert item["module"]["sha256"] == identity[0]

    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    system = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    host = [args.host, "--reader-plugins", args.reader_manifest]
    positions = inputs / "positions.tsv"
    shutil.copyfile(args.positions, positions)
    indexed, reopens, refused = {}, {}, {}
    features = ('Gene "B"', "Gene,A")
    options = dict(observation_id="spot", feature_columns=features, value_domain="raw-counts")
    continuous = {**options, "value_domain": "normalized-expression"}

    def run(argv, code=0):
        result = subprocess.run(argv, cwd=args.output, env=env, encoding="utf-8", capture_output=True, timeout=40)
        assert result.returncode == code, (argv, result.stdout, result.stderr, result.returncode)
        return result.stdout or result.stderr

    def write(label, data):
        path = inputs / (label + suffix)
        path.write_bytes(data)
        return path

    def index(source, dest, selection):
        return [*host, "--format-plugins", args.manifest, "matrix-project-index", provider,
                source, positions, args.template, "1", dest, "--plugin-options-json", json.dumps(selection)]

    def accept(label, source, selection):
        qmx = args.output / (label + ".qmx")
        result = json.loads(run(index(source, qmx, selection)))
        receipt = result["receipt"]
        assert receipt["source_sha256"] == sha(source) and receipt["provider"]["module_sha256"] == identity[0]
        assert result["file_sha256"] == sha(qmx) and result["positions_sha256"] == sha(positions)
        assert result["value_domain"] == selection["value_domain"]
        assert (result["observations"], result["features"], result["entries"]) == (3, 2, 6)
        indexed[label] = result

    def refuse(label, source, message, selection=options):
        qmx = args.output / (label + ".qmx")
        before = sha(source)
        result = json.loads(run(index(source, qmx, selection), 2))
        assert message in result["error"] and not qmx.exists() and before == sha(source), result
        refused[label] = result["error"]

    normalized = inputs / ("normalized" + suffix)
    shutil.copyfile(args.fixture, normalized)
    with normalized.open(encoding="utf-8-sig", newline="") as stream:
        fields, *rows = csv.reader(stream, **dialect)
    assert fields == [features[1], "spot", "unused", features[0]]
    assert [row[1] for row in rows] == ["0001", "1", "μ"]
    accept("continuous", normalized, continuous)

    counts = inputs / ("counts" + suffix)
    with counts.open("w", encoding="utf-8", newline="") as stream:
        csv.writer(stream, lineterminator="\n", **dialect).writerows([
            fields, ["0", "0001", "label, A", "5"],
            [str(2**53+1), "1", '"quoted"', "0"], [str(2**64-1), "μ", "", "7"],
        ])
    accept("counts", counts, options)
    body = counts.read_bytes()
    normalized_bytes = normalized.read_bytes()
    accept("no-final-newline", write("unterminated-record", body.removesuffix(b"\n")), options)
    first = body.splitlines()[1]
    label_field = b"label, A" if tsv else b'"label, A"'

    def replace_count(token):
        return body.replace(b"0" + separator + b"0001", token + separator + b"0001")

    for label, data, message, selection in (
        ("empty", replace_count(b""), "feature is empty", options),
        ("overflow", body.replace(b'18446744073709551615', b'18446744073709551616'), "require a UInt64", options),
        ("negative", replace_count(b"-1"), "require a UInt64", options),
        ("decimal-count", replace_count(b"0.5"), "require a UInt64", options),
        ("infinite", normalized_bytes.replace(b"-0.0", b"inf"), "must be finite", continuous),
        ("junk-number", normalized_bytes.replace(b"-0.0", b"-0.0ms"), "Float64 decimal number", continuous),
        ("short-row", body.replace(first, first.rsplit(separator, 1)[0]), "row width differs", options),
        ("long-row", body.replace(first, first + separator + b"99"), "row width differs", options),
        ("unknown-id", body.replace(b"0001", b"00001"), "observation is missing from positions", options),
        ("duplicate-id", body.replace(b"0001", b"1"), "duplicate", options),
        ("duplicate-header", body.replace(separator + b"unused" + separator, separator + b"spot" + separator),
         "duplicate column name", options),
        ("invalid-utf8", body.replace(label_field, b"\xff"), "Invalid UTF-8", options),
        ("oversized-field", body.replace(label_field, b"x" * 4097), "exceeds 4096 bytes", options),
    ):
        assert data not in (body, normalized_bytes), label
        refuse(label, write(label, data), message, selection)
    if not tsv:
        refuse("broken-quote", write("broken-quote", body.replace(b'"label, A"', b'"label, A"x')), "Unexpected text")
        refuse("multiline", write("multiline", body.replace(b'"label, A"', b'"label\nA"')), "cannot contain line breaks")
    refuse("missing-feature", counts, "column is missing", {**options, "feature_columns": ["missing"]})
    control = args.output / "counts.qmx"
    run(index(counts, control, options), 2)
    assert sha(control) == indexed["counts"]["file_sha256"]
    refused["overwrite"] = "existing QMX preserved"

    unavailable = args.output / "unavailable"
    inputs.rename(unavailable)
    for label, receipt in indexed.items():
        qmx = args.output / (label + ".qmx")
        project = args.output / (label + ".qpxproj")
        digest = receipt["file_sha256"]
        attached = json.loads(run([*host, "matrix-attach", args.template, "1", qmx, digest, project]))
        assert attached["owned"] and attached["file_sha256"] == digest
        qmx.rename(unavailable / qmx.name)
        profile = "continuous" if label == "continuous" else "counts"
        run([args.probe, project, attached["association_id"], digest, args.reader_manifest,
             profile, provider, *features])
        reopens[label] = attached

    assert (sha(module), sha(args.manifest), sha(args.template)) == identity
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": identity[0], "manifest_sha256": identity[1], "template_sha256": identity[2],
        "fixture_sha256": sha(args.fixture), "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
        "indexed": indexed, "reopens": reopens, "refusals": refused,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(indexed)} {suffix[1:].upper()} imports, {len(reopens)} exact owned reopens, {len(refused)} refusals")


if __name__ == "__main__":
    main()
