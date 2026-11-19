"""Exercise optional Arrow annotation import using an independent PyArrow writer."""
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import shutil
import subprocess

import numpy as np
import pyarrow as pa
import pyarrow.feather as feather


def sha(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "manifest", "reader-manifest", "template", "baseline", "output"):
        parser.add_argument("--" + key, type=lambda arg: Path(arg).resolve(), required=True)
    args = parser.parse_args()
    root = args.output
    sources = root / "inputs"
    sources.mkdir(parents=True)
    baseline = json.loads(args.baseline.read_bytes())
    fixture = sources / "default.feather"
    shutil.copyfile(args.baseline.with_name("points.feather"), fixture)
    assert sha(fixture) == baseline["fixture_sha"].lower()
    assert sha(args.host) == baseline["host_sha"].lower()
    template_sha = sha(args.template)
    assert template_sha == baseline["template_sha"].lower()
    descriptor, = json.loads(args.manifest.read_bytes())["plugins"]
    module = args.manifest.with_name(descriptor["module"]["path"])
    module_sha, manifest_sha = sha(module), sha(args.manifest)
    provider = "org.qupathxx.arrow-points"
    assert descriptor["abi"] == 3 and descriptor["provider_id"] == provider
    assert descriptor["module"]["sha256"] == module_sha and not descriptor["dependencies"]
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env.pop("PYTHONPATH", None)
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    command = [args.host, "--reader-plugins", args.reader_manifest, "--format-plugins",
               args.manifest, "annotations-import", provider]
    options = {"coordinate_space": "image-pixels", "id": "source_id", "x": "x_px", "y": "y_px", "name": "title"}
    table = feather.read_table(fixture)
    assert table.column("source_id").num_chunks == 2 and table["source_id"].to_pylist() == ["0001", "1", "μ"]
    imports, reopens, refusals = {}, {}, {}
    integer_cases = {}

    def run(argv, expected=0):
        result = subprocess.run(argv, cwd=root, env=env, encoding="utf-8", capture_output=True, timeout=30)
        assert result.returncode == expected, (argv, result.returncode, result.stdout, result.stderr)
        return result

    def write(label, data, compression="uncompressed", chunksize=2):
        path = sources / f"{label}.arrow"
        feather.write_feather(data, path, compression=compression, chunksize=chunksize)
        assert feather.read_table(path).equals(data)
        return path

    def column(index, array, name=None):
        return table.set_column(index, name or table.column_names[index], array)

    def invoke(src, dest, opts, *extra, template=args.template):
        flags = [] if opts is None else ["--plugin-options-json", json.dumps(opts)]
        return [*command, template, "1", src, dest, *flags, *extra]

    def accept(label, src, opts=options, *extra, template=args.template, ids=("0001", "1", "μ")):
        dest = root / f"{label}.qpxproj"
        receipt = json.loads(run(invoke(src, dest, opts, *extra, template=template)).stdout)
        assert receipt["state"] == "saved" and receipt["receipt"]["source_sha256"] == sha(src)
        assert receipt["receipt"]["provider"]["module_sha256"] == module_sha
        assert [row["source_id"] for row in receipt["objects"]] == list(ids)
        imports[label] = receipt
        return dest

    def refuse(label, src, diagnostic, opts=options):
        dest = root / f"{label}.qpxproj"
        before = sha(src)
        result = run(invoke(src, dest, opts), 2)
        assert diagnostic.lower() in result.stderr.lower() and not dest.exists() and sha(src) == before, result.stderr
        refusals[label] = json.loads(result.stderr)["error"]

    accept("default", fixture)
    pinned = accept("plain", write("uncompressed", table))
    dictionary_baseline = json.loads(args.baseline.with_name("dictionary-baseline.json").read_bytes())
    categorical = sources / "dictionary-default.feather"
    shutil.copyfile(args.baseline.with_name("dictionary.feather"), categorical)
    assert sha(categorical) == dictionary_baseline["fixture_sha"].lower()
    assert table["source_id"].to_pylist() == feather.read_table(categorical)["source_id"].to_pylist()
    accept("dictionary-default", categorical)

    integer = column(1, pa.array([0, 1, 3], type=pa.uint32()))
    integer = integer.set_column(2, "y_px", pa.array([1, 2, 0], type=pa.int16()))
    classified = integer.append_column("phenotype", pa.array(["tumor", None, "stroma"]))
    class_opts = {**options, "class": "phenotype"}
    classified_file = write("signed-unsigned", classified, "lz4")
    refuse("implicit-class", classified_file, "no explicit native class mapping", class_opts)
    classified_template = root / "classified-template.qpxproj"
    run([args.probe, args.template, args.reader_manifest, "seed-classes", classified_template])
    accept("integer", classified_file, class_opts, "--class-map-json", '{"tumor":"Tumor","stroma":"Stroma"}',
           template=classified_template)
    categorical_labels = classified.set_column(0, "source_id", pa.DictionaryArray.from_arrays(
        pa.array([1, 2, 0], type=pa.int8()), pa.array(["μ", "0001", "1"])))
    categorical_labels = categorical_labels.set_column(3, "title", pa.DictionaryArray.from_arrays(
        pa.array([2, 1, 0], type=pa.uint16()), pa.array(["Spot C", None, "Spot A"], type=pa.large_string())))
    categorical_labels = categorical_labels.set_column(4, "phenotype",
        classified["phenotype"].combine_chunks().dictionary_encode())
    for index in (1, 2):
        categorical_labels = categorical_labels.set_column(index, classified.column_names[index],
            classified.column(index).combine_chunks().dictionary_encode())
    accept("integer-dictionaries", write("categorical-labels", categorical_labels),
           class_opts, "--class-map-json", '{"tumor":"Tumor","stroma":"Stroma"}', template=classified_template)

    for i, dtype in enumerate(("int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64")):
        info = np.iinfo(dtype)
        values = [int(info.min), 2**53+1 if info.bits == 64 else 1, int(info.max)]
        data = column(0, pa.array(values, type=pa.from_numpy_dtype(np.dtype(dtype))))
        label = f"source-{dtype}"
        source = write(label, data, ("uncompressed", "lz4", "zstd")[i % 3], chunksize=1)
        ids = list(map(str, values))
        accept(label, source, ids=ids)
        integer_cases[label] = (dtype, ids)

    ipc = sources / "typed-ipc.arrow"
    ipc_options = pa.ipc.IpcWriteOptions(compression="zstd")
    with pa.OSFile(str(ipc), "wb") as stream, pa.ipc.new_file(stream, data.schema, options=ipc_options) as writer:
        writer.write_table(data, max_chunksize=1)
    with pa.OSFile(str(ipc), "rb") as stream:
        assert pa.ipc.open_file(stream).read_all().equals(data)
    accept("typed-ipc", ipc, ids=ids)
    integer_cases["typed-ipc"] = ("uint64", ids)

    refuse("duplicate-integer-id", write("repeat-integer-id", column(0, pa.array([1, 1, 2], type=pa.uint64()))),
           "IDs are not unique")
    refuse("null-integer-id", write("missing-integer-id", column(0, pa.array([1, None, 2], type=pa.int64()))),
           "ID is empty or null")
    refuse("real-id", write("floating-id", column(0, pa.array([.5, 1.5, 2.5]))),
           "ID column must be UTF-8 or an integer")
    refuse("implicit-frame", fixture, "require explicit", None)
    refuse("wrong-frame", fixture, "require image-pixels", {**options, "coordinate_space": "micrometers"})
    refuse("same-column", fixture, "distinct columns", {**options, "y": "x_px"})
    refuse("missing-column", fixture, "column is missing", {**options, "x": "other"})
    refuse("duplicate-column", write("ambiguous", table.append_column("x_px", table["x_px"])), "ambiguous")
    refuse("invalid-name", write("nul-name", column(1, table["x_px"], "x_px\0hidden")), "Arrow IPC V5 footer")
    refuse("null-coordinate", write("missing-x", column(1, pa.array([None, 1.25, 3.5]))), "coordinate is null")
    refuse("negative-coordinate", write("negative-x", column(1, pa.array([0, -1, 3]))), "coordinate is negative")
    refuse("infinite-coordinate", write("inf-x", column(1, pa.array([0., float("inf"), 3.]))), "finite nonnegative")
    refuse("inexact-coordinate", write("wide-x", column(1, pa.array([0, 2**53 + 1, 3], type=pa.uint64()))),
           "not exactly representable")
    refuse("null-id", write("missing-id", column(0, pa.array(["0001", None, "μ"]))), "ID is empty or null")
    refuse("duplicate-id", write("repeated", column(0, pa.array(["0001", "1", "0001"]))), "IDs are not unique")
    compressed = write("zstd-batches", table, "zstd")
    accept("zstd", compressed)
    accept("zstd-dictionaries", write("zstd-categorical", feather.read_table(categorical), "zstd"))
    zstd_magic = bytes.fromhex("28 b5 2f fd")
    data = compressed.read_bytes()
    assert zstd_magic in data, "The writer must emit a real Zstandard frame"
    invalid_frame = sources / "invalid-zstd-frame.arrow"
    invalid_frame.write_bytes(data.replace(zstd_magic, b"\0" * 4, 1))
    refuse("damaged-zstd-frame", invalid_frame, "Invalid Arrow IPC values")
    dictionary = pa.DictionaryArray.from_arrays(pa.array([0, 1, 2]), pa.array(["0001", None, "μ"]))
    refuse("null-dictionary-id", write("null-value", column(0, dictionary)), "ID is empty or null")
    dictionary = pa.DictionaryArray.from_arrays(pa.array([0, None, 1]), pa.array(["0001", "μ"]))
    refuse("null-dictionary-index", write("null-index", column(0, dictionary)), "ID is empty or null")
    dictionary = pa.DictionaryArray.from_arrays(pa.array([0, 1, 2]), pa.array(["0001", "1", "0001"]))
    refuse("duplicate-decoded-id", write("repeated-value", column(0, dictionary)), "IDs are not unique")
    dictionary = pa.array([b"x", b"y", b"z"]).dictionary_encode()
    refuse("binary-dictionary", write("binary-dictionary-src", column(1, dictionary)), "dictionary values only")
    for value, diagnostic in ((float("inf"), "finite nonnegative"), (None, "coordinate is null")):
        encoded = pa.DictionaryArray.from_arrays(pa.array([0, 1, 2]), pa.array([.5, value, 3.5]))
        refuse("invalid-float-dictionary-" + str(value), write("float-src-" + str(value), column(1, encoded)), diagnostic)
    dictionary = pa.DictionaryArray.from_arrays(pa.array([0, 1, 2]), pa.array([0, None, 2], type=pa.uint64()))
    refuse("null-integer-dictionary", write("numeric-null", column(0, dictionary)), "ID is empty or null")
    dictionary = pa.DictionaryArray.from_arrays(pa.array([0, 1, 2]), pa.array([0, 2**53 + 1, 3], type=pa.uint64()))
    refuse("inexact-integer-dictionary-coordinate", write("numeric-inexact", column(1, dictionary)),
           "not exactly representable")

    def delta_file(label, dictionaries, batches, *, value_type=pa.string(), compression=None,
                   expected=("0001", "1", "μ"), column_index=0):
        path = sources / (label + ".arrow")
        records = []
        offset = 0
        for values, indices in zip(dictionaries, batches, strict=True):
            ids = pa.DictionaryArray.from_arrays(pa.array(indices, type=pa.int32()),
                                                  pa.array(values, type=value_type))
            columns = table.slice(offset, len(indices)).combine_chunks()
            records.append(columns.set_column(column_index, table.column_names[column_index], ids).to_batches()[0])
            offset += len(indices)
        assert offset == table.num_rows
        writer_options = pa.ipc.IpcWriteOptions(emit_dictionary_deltas=True, compression=compression)
        with pa.OSFile(str(path), "wb") as stream, pa.ipc.new_file(
                stream, records[0].schema, options=writer_options) as writer:
            for batch in records:
                writer.write_batch(batch)
        with pa.OSFile(str(path), "rb") as stream:
            assert pa.ipc.open_file(stream).read_all().column(column_index).to_pylist() == list(expected)
        return path

    accept("dictionary-delta", delta_file("delta-dictionary", [["0001", "1"], ["0001", "1", "μ"]],
                                        [[0, 1], [2]]))
    # Two successive compressed deltas and an unused null dictionary entry.
    accept("dictionary-delta-lz4", delta_file("delta-lz4",
        [[None, "0001"], [None, "0001", "1"], [None, "0001", "1", "μ"]],
        [[1], [2], [3]], value_type=pa.large_string(), compression="lz4"))
    accept("dictionary-delta-zstd", delta_file("delta-zstd",
        [[None, "0001"], [None, "0001", "1"], [None, "0001", "1", "μ"]],
        [[1], [2], [3]], value_type=pa.large_string(), compression="zstd"))
    for kind, values, value_type in (
        ("uint64", [0, 2**53 + 1, 2**64 - 1], pa.uint64()),
        ("int64", [-(2**63), 2**53 + 1, 2**63 - 1], pa.int64()),
    ):
        label = "dictionary-delta-" + kind
        source = delta_file(label, [[None, *values[:i]] for i in (1, 2, 3)], [[1], [2], [3]],
                            value_type=value_type, compression="zstd", expected=values)
        ids = list(map(str, values))
        accept(label, source, ids=ids)
        integer_cases[label] = (kind, ids)
    for index, value_type in ((1, pa.float64()), (2, pa.float32())):
        values = table.column(index).to_pylist()
        label = "dictionary-delta-" + str(value_type)
        source = delta_file(label, [[None, *values[:i]] for i in (1, 2, 3)], [[1], [2], [3]],
                            value_type=value_type, compression="lz4", expected=values, column_index=index)
        accept(label, source)
    base_values = ["0001", "1"] + ["unused"] * 99_997
    oversized = delta_file("delta-too-many", [base_values, base_values + ["μ", "unused"]],
                           [[0, 1], [99_999]], compression="lz4")
    refuse("combined-dictionary-entry-limit", oversized, "Invalid Arrow IPC dictionary")
    truncated = sources / "short.feather"
    truncated.write_bytes(fixture.read_bytes()[:-9])
    refuse("truncation", truncated, "Arrow IPC file")
    over_budget = table.append_column("other", pa.array(["x" * (9 * 1024 * 1024), "", ""]))
    for codec in ("lz4", "zstd"):
        refuse("decompression-quota-" + codec, write("large-field-" + codec, over_budget, codec),
               "decoder work limit exceeded")
    pinned_sha = sha(pinned)
    result = run(invoke(fixture, pinned, options), 2)
    assert sha(pinned) == pinned_sha and sha(fixture) == baseline["fixture_sha"].lower()
    refusals["existing-project"] = json.loads(result.stderr)["error"]

    sources.rename(root / "inputs-unavailable")
    for label in imports:
        saved = root / f"{label}.qpxproj"
        if label in integer_cases:
            kind, ids = integer_cases[label]
            output = root / (label + ".geojson")
            result = run([args.host, "geojson-export", saved, "1", output])
            features = json.loads(output.read_bytes())["features"]
            for feature, point, identifier in zip(features, table.to_pylist(), ids, strict=True):
                metadata = feature["properties"]["metadata"]
                assert metadata["qpx.annotation-plugin.source_id"] == identifier
                assert metadata["qpx.annotation-plugin.source.arrow.source_id_type"] == kind
                assert feature["geometry"] == {"type": "Point", "coordinates": [point["x_px"], point["y_px"]]}
                assert feature["properties"]["name"] == (point["title"] or identifier)
        else:
            result = run([args.probe, saved, args.reader_manifest, label])
        reopens[label] = {"sha256": sha(saved), "stdout": result.stdout.strip()}
    assert sha(args.template) == template_sha and sha(module) == module_sha and sha(args.manifest) == manifest_sha
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "producer": pa.__version__, "host_sha256": sha(args.host),
        "probe_sha256": sha(args.probe), "module_sha256": module_sha, "manifest_sha256": manifest_sha,
        "imports": imports, "reopens": reopens, "refusals": refusals,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{len(imports)} imports, {len(reopens)} exact reopens, {len(refusals)} refusals")


if __name__ == "__main__":
    main()
