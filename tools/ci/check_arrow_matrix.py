"""Check native columnar matrix import into a saved image and offline owned replay."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

import numpy as np
import pyarrow as pa
import pyarrow.feather as feather
import pyarrow.parquet as parquet


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "manifest", "reader-manifest", "template", "baseline", "output"):
        parser.add_argument("--" + key, required=True, type=lambda path: Path(path).resolve())
    args = parser.parse_args()
    sources = args.output / "originals"
    sources.mkdir(parents=True)
    baseline = json.loads(args.baseline.read_bytes())
    assert baseline["exit_code"] == 2 and sha(args.host) == baseline["host_sha"].lower()
    assert sha(args.template) == baseline["template_sha"].lower()
    fixtures = args.baseline.parent
    provider, = json.loads(args.manifest.read_bytes())["plugins"]
    is_parquet = provider["provider_id"] == "org.qupathxx.parquet-matrix"
    counts = sources / ("counts.parquet" if is_parquet else "counts.feather")
    positions = sources / "positions.tsv"
    shutil.copyfile(fixtures / counts.name, counts)
    shutil.copyfile(fixtures / positions.name, positions)
    assert sha(counts) == baseline["fixture_sha"].lower()
    table = parquet.read_table(counts) if is_parquet else feather.read_table(counts)
    assert table["Gene.A"].to_pylist() == [0, 2**53 + 1, 2**64 - 1]
    assert provider["provider_id"] in {"org.qupathxx.arrow-matrix", "org.qupathxx.parquet-matrix"}
    assert provider["abi"] == 2
    assert provider["domain"] == "matrix" and not provider["dependencies"]
    module = args.manifest.with_name(provider["module"]["path"])
    identity = sha(module), sha(args.manifest)
    assert identity[0] == provider["module"]["sha256"]
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    system = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))

    bootstrap = [args.host, "--reader-plugins", args.reader_manifest]
    command = [*bootstrap, "--format-plugins", args.manifest, "matrix-project-index", provider["provider_id"]]
    selection = {"observation_id": "spot", "feature_columns": ["Gene.B", "Gene.A"], "value_domain": "raw-counts"}
    continuous = {**selection, "value_domain": "normalized-expression"}
    indexed, reopens, refusals = {}, {}, {}
    integer_cases = {}

    def run(cmd, code=0):
        result = subprocess.run(cmd, cwd=args.output, env=env, encoding="utf-8", capture_output=True, timeout=40)
        assert result.returncode == code, (cmd, result.returncode, result.stdout, result.stderr)
        return json.loads(result.stdout or result.stderr)

    def invoke(source, dest, options, coordinates):
        argv = [*command, source, coordinates, args.template, "1", dest]
        return argv if options is None else [*argv, "--plugin-options-json", json.dumps(options)]

    def accept(label, source, options, coordinates=positions):
        dest = args.output / (label + ".qmx")
        report = run(invoke(source, dest, options, coordinates))
        assert report["file_sha256"] == sha(dest) and report["positions_sha256"] == sha(coordinates)
        assert report["receipt"]["source_sha256"] == sha(source)
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        assert json.loads(report["receipt"]["options"]) == options
        assert (report["observations"], report["features"], report["entries"]) == (3, 2, 6)
        assert report["value_domain"] == options["value_domain"]
        indexed[label] = report

    def refuse(label, source, expected, options=selection, coordinates=positions):
        dest = args.output / (label + ".qmx")
        before = sha(source)
        result = run(invoke(source, dest, options, coordinates), 2)
        assert expected.lower() in result["error"].lower() and not dest.exists(), result
        assert sha(source) == before
        refusals[label] = result["error"]

    def write(label, data, codec="uncompressed"):
        if is_parquet:
            path = sources / (label + ".parquet")
            parquet.write_table(data, path, row_group_size=2,
                                compression="NONE" if codec == "uncompressed" else codec,
                                write_page_checksum=True)
        else:
            path = sources / (label + ".arrow")
            feather.write_feather(data, path, chunksize=2, compression=codec)
        return path

    accept("counts", counts, selection)
    if is_parquet:
        plain_counts = write("plain-counts", table)
        accept("counts-uncompressed", plain_counts, selection)
        chunk = parquet.read_metadata(plain_counts).row_group(0).column(0)
        payload = bytearray(plain_counts.read_bytes())
        at = payload.index(b"0001", chunk.dictionary_page_offset, chunk.data_page_offset)
        payload[at] = ord("9")
        damaged = sources / "bad-checksum.parquet"
        damaged.write_bytes(payload)
        refuse("bad-page-checksum", damaged, "page or metadata decoding failed")
    wide = table.set_column(1, "Gene.A", pa.array([-0.0, 1 + 2**-52, 2**-1074], type=pa.float64()))
    wide = wide.set_column(2, "Gene.B", pa.array([2**-149, -7.5, 6.5], type=pa.float32()))
    accept("continuous", write("normalization", wide, "zstd"), continuous)

    for dtype in ("int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64"):
        info = np.iinfo(dtype)
        values = [int(info.min), 2**53+1 if info.bits == 64 else 1, int(info.max)]
        labels = list(map(str, values))
        ids = pa.array(values, type=pa.from_numpy_dtype(np.dtype(dtype)))
        name = f"spot-{dtype}"
        matrix = write(name, table.set_column(0, "spot", ids), "zstd")
        produced = parquet.read_table(matrix) if is_parquet else feather.read_table(matrix)
        assert produced["spot"].type == ids.type and produced["spot"].to_pylist() == values
        coordinate_file = sources / (name + ".tsv")
        coordinate_file.write_text(
            f"observation_id\tx_px\ty_px\n{labels[2]}\t3.5\t2.5\n"
            f"{labels[0]}\t0.5\t0.75\n{labels[1]}\t1.25\t1.25\n",
            encoding="utf-8", newline="\n")
        accept(name, matrix, selection, coordinate_file)
        integer_cases[name] = labels
    padded = sources / "numeric-padded.tsv"
    uint64_positions = (sources / "spot-uint64.tsv").read_text(encoding="utf-8")
    padded.write_text(uint64_positions.replace("\n0\t", "\n000\t"), encoding="utf-8")
    refuse("padded-integer-id", sources / ("spot-uint64" + (".parquet" if is_parquet else ".arrow")),
           "observation is missing from positions", coordinates=padded)
    for issue, ids, error in (
        ("duplicate", pa.array([1, 1, 1], type=pa.uint64()), "duplicate"),
        ("null", pa.array([0, None, 2], type=pa.int64()), "missing or null" if is_parquet else "ID is empty or null"),
        ("real", pa.array([0.5, 1.5, 2.5]), "ID column must be UTF-8 or an integer"),
    ):
        name = f"{issue}-integer-id"
        refuse(name, write(name, table.set_column(0, "spot", ids)), error)

    if not is_parquet:
        floating_delta = sources / "continuous-deltas.arrow"
        batches = []
        for i in range(wide.num_rows):
            columns = wide.slice(i, 1)
            for index in (1, 2):
                values = wide.column(index).slice(0, i + 1).combine_chunks()
                encoded = pa.DictionaryArray.from_arrays(pa.array([i], type=pa.int8()), values)
                columns = columns.set_column(index, wide.column_names[index], encoded)
            batches.append(columns.to_batches()[0])
        settings = pa.ipc.IpcWriteOptions(compression="zstd", emit_dictionary_deltas=True)
        with pa.OSFile(str(floating_delta), "wb") as stream, pa.ipc.new_file(
                stream, batches[0].schema, options=settings) as writer:
            for batch in batches:
                writer.write_batch(batch)
        with pa.OSFile(str(floating_delta), "rb") as stream:
            decoded = pa.ipc.open_file(stream).read_all()
        for index in (1, 2):
            kind = decoded.column(index).type.value_type
            actual = decoded.column(index).cast(kind).combine_chunks().to_numpy()
            expected = wide.column(index).combine_chunks().to_numpy()
            assert actual.dtype == expected.dtype and actual.tobytes() == expected.tobytes()
        accept("continuous-deltas", floating_delta, continuous)
        refuse("dictionary-float-as-count", floating_delta, "integer feature")
        for value, diagnostic in ((float("inf"), "must be finite"), (None, "feature value is null")):
            encoded = pa.DictionaryArray.from_arrays(pa.array([0, 1, 2]), pa.array([1., value, 3.]))
            source = write("dictionary-continuous-" + str(value), wide.set_column(1, "Gene.A", encoded))
            refuse("invalid-dictionary-continuous-" + str(value), source, diagnostic, continuous)

        unsigned = [0, 2**53 + 1, 2**64 - 1]
        indices = pa.array([1, 2, 0], type=pa.int8())
        ids = pa.DictionaryArray.from_arrays(indices, pa.array([unsigned[2], 0, unsigned[1]], type=pa.uint64()))
        encoded = table.set_column(0, "spot", ids)
        encoded = encoded.set_column(1, "Gene.A", ids)
        encoded = encoded.set_column(2, "Gene.B", pa.DictionaryArray.from_arrays(indices, pa.array([7, 5, 0])))
        dictionary_matrix = write("dictionary-uint64", encoded, "zstd")
        decoded = feather.read_table(dictionary_matrix)
        assert decoded["spot"].to_pylist() == decoded["Gene.A"].to_pylist() == unsigned
        assert decoded["Gene.B"].to_pylist() == [5, 0, 7]
        accept("dictionary-uint64", dictionary_matrix, selection, sources / "spot-uint64.tsv")
        integer_cases["dictionary-uint64"] = list(map(str, unsigned))
        missing = pa.DictionaryArray.from_arrays(indices, pa.array([7, None, 0], type=pa.int64()))
        refuse("null-dictionary-count", write("dictionary-missing", encoded.set_column(2, "Gene.B", missing)),
               "feature value is null", coordinates=sources / "spot-uint64.tsv")
        negative = pa.DictionaryArray.from_arrays(indices, pa.array([7, -5, 0], type=pa.int64()))
        refuse("negative-dictionary-count", write("dictionary-negative", encoded.set_column(2, "Gene.B", negative)),
               "cannot be negative", coordinates=sources / "spot-uint64.tsv")

    refuse("no-selection", counts, "explicit column", None)
    refuse("unknown-feature", counts, "column is missing", {**selection, "feature_columns": ["missing"]})
    refuse("repeated-feature", counts, "must be distinct", {**selection, "feature_columns": ["Gene.A", "Gene.A"]})
    refuse("float-as-count", write("wrong-count-type", wide), "integer feature")
    refuse("integer-as-continuous", counts, "Float32 or Float64", continuous)
    refuse("negative", write("negative-value", table.set_column(2, "Gene.B", pa.array([5, -1, 7]))), "cannot be negative")
    null_diagnostic = "missing or null" if is_parquet else "value is null"
    refuse("missing-value", write("null-value", table.set_column(2, "Gene.B", pa.array([5, None, 7]))), null_diagnostic)
    refuse("infinite", write("infinite-value", wide.set_column(1, "Gene.A", pa.array([1., 2., float("inf")]))),
           "must be finite", continuous)
    refuse("missing-id", write("null-id", table.set_column(0, "spot", pa.array(["0001", None, "μ"]))),
           "missing or null" if is_parquet else "ID is empty or null")
    refuse("duplicate-id", write("repeated-id", table.set_column(0, "spot", pa.array(["0001", "1", "0001"]))), "duplicate")
    incomplete = sources / "incomplete.tsv"
    incomplete.write_bytes(b"observation_id\tx_px\ty_px\n1\t1.25\t1.25\n")
    refuse("missing-positions", counts, "position observation sets differ", coordinates=incomplete)
    existing = args.output / "counts.qmx"
    run(invoke(counts, existing, selection, positions), 2)
    assert indexed["counts"]["file_sha256"] == sha(existing)
    refusals["overwrite"] = "existing QMX preserved"
    if is_parquet:
        refuse("unsupported-codec", write("gzip", table, "gzip"), "use uncompressed, Snappy or Zstandard")
        malformed = sources / "truncated.parquet"
        malformed.write_bytes(counts.read_bytes()[:-2])
        refuse("incomplete-footer", malformed, "local unencrypted Parquet file")

    unavailable = args.output / "unavailable"
    sources.rename(unavailable)
    for label, report in indexed.items():
        qmx = args.output / (label + ".qmx")
        project = args.output / (label + ".qpxproj")
        digest = report["file_sha256"]
        result = run([*bootstrap, "matrix-attach", args.template, "1", qmx, digest, project])
        assert result["owned"] and result["file_sha256"] == digest
        qmx.rename(unavailable / qmx.name)
        profile = "counts" if report["value_domain"] == "raw-counts" else "continuous"
        argv = [args.probe, project, result["association_id"], digest, args.reader_manifest, profile,
                provider["provider_id"], *integer_cases.get(label, [])]
        probe = subprocess.run(argv, cwd=args.output, env=env, capture_output=True, encoding="utf-8", timeout=40)
        assert probe.returncode == 0, (argv, probe.stderr)
        reopens[label] = result
    assert sha(args.template) == baseline["template_sha"].lower()
    assert (sha(module), sha(args.manifest)) == identity
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "producer": pa.__version__, "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
        "module_sha256": identity[0], "manifest_sha256": identity[1],
        "indexed": indexed, "reopens": reopens, "refusals": refusals,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(indexed)} imports, {len(reopens)} exact owned reopens, {len(refusals)} refusals")


if __name__ == "__main__":
    main()
