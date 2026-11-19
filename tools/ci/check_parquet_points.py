"""Verify native Parquet point imports and exact project reopens."""
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import shutil
import subprocess

import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq


def sha(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "manifest", "reader-manifest", "template", "baseline", "output"):
        parser.add_argument("--" + key, type=lambda value: Path(value).resolve(), required=True)
    args = parser.parse_args()
    root = args.output
    sources = root / "inputs"
    sources.mkdir(parents=True)
    baseline = json.loads(args.baseline.read_bytes())
    original = sources / "snappy.parquet"
    shutil.copyfile(args.baseline.with_name("points.parquet"), original)
    assert sha(original) == baseline["fixture_sha"].lower()
    assert sha(args.host) == baseline["host_sha"].lower()
    assert sha(args.template) == baseline["template_sha"].lower()
    assert baseline["exit_code"] == 2
    provider, = json.loads(args.manifest.read_bytes())["plugins"]
    assert provider["provider_id"] == "org.qupathxx.parquet-points" and provider["abi"] == 3
    assert provider["domain"] == "annotation" and not provider["dependencies"]
    module = args.manifest.with_name(provider["module"]["path"])
    identity = sha(module), sha(args.manifest)
    assert identity[0] == provider["module"]["sha256"]
    env = {key: val for key, val in os.environ.items() if not key.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    system = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    command = [args.host, "--reader-plugins", args.reader_manifest, "--format-plugins",
               args.manifest, "annotations-import", provider["provider_id"]]
    opts = {"coordinate_space": "image-pixels", "id": "source_id", "x": "x_px", "y": "y_px", "name": "title"}
    table = pq.read_table(original)
    assert pq.read_metadata(original).num_row_groups == 2
    expected_ids = ["0001", "1", "μ"]
    assert table["source_id"].to_pylist() == expected_ids
    imports, reopens, refusals = {}, {}, {}
    integer_cases = {}

    def run(argv, expected=0):
        result = subprocess.run(argv, cwd=root, env=env, encoding="utf-8", capture_output=True, timeout=30)
        assert result.returncode == expected, (argv, result.returncode, result.stdout, result.stderr)
        return result

    def write(label, data, codec="NONE"):
        path = sources / f"{label}.parquet"
        pq.write_table(data, path, row_group_size=2, compression=codec, write_page_checksum=True)
        return path

    def replace(index, values):
        return table.set_column(index, table.column_names[index], values)

    def invoke(path, dest, options, *extra, template=args.template):
        flags = [] if options is None else ["--plugin-options-json", json.dumps(options)]
        return [*command, template, "1", path, dest, *flags, *extra]

    def accept(label, path, options=opts, *extra, template=args.template, ids=expected_ids):
        dest = root / (label + ".qpxproj")
        report = json.loads(run(invoke(path, dest, options, *extra, template=template)).stdout)
        assert report["state"] == "saved" and report["receipt"]["source_sha256"] == sha(path)
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        assert [row["source_id"] for row in report["objects"]] == ids
        imports[label] = report

    def refuse(label, path, diagnostic, options=opts):
        output = root / (label + ".qpxproj")
        original_sha = sha(path)
        error = json.loads(run(invoke(path, output, options), 2).stderr)["error"]
        assert diagnostic.lower() in error.lower() and not output.exists() and sha(path) == original_sha, error
        refusals[label] = error

    accept("snappy", original)
    plain = write("plain", table)
    accept("uncompressed", plain)
    classes = replace(1, pa.array([0, 1, 3], type=pa.uint32()))
    classes = classes.set_column(2, "y_px", pa.array([1, 2, 0], type=pa.int16()))
    classes = classes.append_column("labels", pa.array(["tumor", None, "stroma"]))
    labeled = write("classified", classes, "zstd")
    class_opts = {**opts, "class": "labels"}
    refuse("implicit-class", labeled, "no explicit native class mapping", class_opts)
    class_template = root / "classified-template.qpxproj"
    run([args.probe, args.template, args.reader_manifest, "seed-classes", class_template])
    accept("integer", labeled, class_opts, "--class-map-json", '{"tumor":"Tumor","stroma":"Stroma"}',
           template=class_template)

    for dtype in ("int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64"):
        info = np.iinfo(dtype)
        identifiers = [int(info.min), 2**53+1 if info.bits == 64 else 1, int(info.max)]
        values = pa.array(identifiers, type=pa.from_numpy_dtype(np.dtype(dtype)))
        label = f"source-{dtype}"
        source = write(label, replace(0, values), "zstd")
        independent = pq.read_table(source)["source_id"]
        assert independent.type == values.type and independent.to_pylist() == identifiers
        ids = list(map(str, identifiers))
        accept(label, source, ids=ids)
        integer_cases[label] = (dtype, ids)

    refuse("duplicate-integer-id", write("repeat-integer-id", replace(0, pa.array([1, 1, 2], type=pa.uint64()))),
           "IDs are not unique")
    refuse("null-integer-id", write("missing-integer-id", replace(0, pa.array([1, None, 2], type=pa.int64()))),
           "missing or null")
    refuse("real-id", write("floating-id", replace(0, pa.array([.5, 1.5, 2.5]))),
           "ID column must be UTF-8 or an integer")

    refuse("missing-options", original, "require explicit", None)
    refuse("unsupported-space", original, "require image-pixels", {**opts, "coordinate_space": "meters"})
    refuse("missing-column", original, "column is missing", {**opts, "x": "missing"})
    refuse("duplicate-selection", original, "distinct columns", {**opts, "x": "y_px"})
    refuse("ambiguous-column", write("ambiguous", table.append_column("x_px", table["x_px"])), "ambiguous")
    refuse("duplicate-id", write("nonunique", replace(0, pa.array(["1", "1", "μ"]))), "IDs are not unique")
    refuse("null-id", write("missing-id", replace(0, pa.array(["0001", None, "μ"]))), "missing or null")
    refuse("null-coordinate", write("missing-x", replace(1, pa.array([.5, None, 3.5]))), "missing or null")
    refuse("negative-integer", write("negative-x", replace(1, pa.array([0, -1, 3]))), "cannot be negative")
    refuse("infinite-coordinate", write("infinite-x", replace(1, pa.array([0., float("inf"), 3.]))), "finite nonnegative")
    wide = replace(1, pa.array([0, 2**53 + 1, 3], type=pa.uint64()))
    refuse("wide-coordinate", write("wide-x", wide), "not exactly representable")
    refuse("nonlocal-profile", write("gzip", table, "gzip"), "uncompressed, Snappy or Zstandard")
    truncated = sources / "truncated.parquet"
    truncated.write_bytes(original.read_bytes()[:-1])
    refuse("missing-footer", truncated, "local unencrypted Parquet file")
    chunk = pq.read_metadata(plain).row_group(0).column(0)
    payload = bytearray(plain.read_bytes())
    at = payload.index(b"0001", chunk.dictionary_page_offset, chunk.data_page_offset)
    payload[at] = ord("9")
    damaged = sources / "damaged-page.parquet"
    damaged.write_bytes(payload)
    refuse("page-checksum", damaged, "page or metadata decoding failed")
    existing = root / "snappy.qpxproj"
    before = sha(existing)
    run(invoke(original, existing, opts), 2)
    assert sha(existing) == before
    refusals["overwrite"] = "existing project preserved"

    sources.rename(root / "inputs-unavailable")
    for label in imports:
        saved = root / (label + ".qpxproj")
        if label in integer_cases:
            kind, ids = integer_cases[label]
            exchange = root / (label + ".geojson")
            result = run([args.host, "geojson-export", saved, "1", exchange])
            features = json.loads(exchange.read_bytes())["features"]
            for feature, point, identifier in zip(features, table.to_pylist(), ids, strict=True):
                metadata = feature["properties"]["metadata"]
                assert metadata["qpx.annotation-plugin.source_id"] == identifier
                assert metadata["qpx.annotation-plugin.source.parquet.source_id_type"] == kind
                assert feature["geometry"] == {"type": "Point", "coordinates": [point["x_px"], point["y_px"]]}
                assert feature["properties"]["name"] == (point["title"] or identifier)
        else:
            result = run([args.probe, saved, args.reader_manifest, label, provider["provider_id"]])
        reopens[label] = {"sha256": sha(saved), "stdout": result.stdout.strip()}
    assert sha(args.template) == baseline["template_sha"].lower()
    assert identity == (sha(module), sha(args.manifest))
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "producer": pa.__version__, "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
        "module_sha256": identity[0], "manifest_sha256": identity[1],
        "imports": imports, "reopens": reopens, "refusals": refusals,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{len(imports)} imports, {len(reopens)} exact reopens, {len(refusals)} refusals")


if __name__ == "__main__":
    main()
