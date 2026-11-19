"""Verify the installed MuData module and provider-free QMX replay."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

import h5py
import numpy as np

from check_counts_npz_plugin import sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "manifest", "probe", "baseline", "scale-baseline", "output"):
        parser.add_argument("--" + key, required=True, type=lambda path: Path(path).resolve())
    args = parser.parse_args()
    baseline = json.loads(args.baseline.read_bytes())
    assert baseline["exit_code"] == 2 and "provider unavailable" in baseline["diagnostic"]
    item, = json.loads(args.manifest.read_bytes())["plugins"]
    module = args.manifest.with_name(item["module"]["path"])
    identity = sha(module), sha(args.manifest)
    assert item["provider_id"] == "org.qupathxx.mudata" and item["abi"] == 5
    assert item["module"]["sha256"] == identity[0]
    command = [args.host, "--format-plugins", args.manifest, "matrix-index", item["provider_id"]]
    root = args.output
    inputs = root / "source files"
    inputs.mkdir(parents=True)
    study = inputs / "study.h5mu"
    shutil.copyfile(args.baseline.with_name(study.name), study)
    assert sha(study) == baseline["fixture_sha"].lower()
    image = inputs / "image.ppm"
    image.write_bytes(b"P6\n8 6\n255\n" + bytes(8 * 6 * 3))
    coordinates = {"0001": (1.25, 2.5), "1": (6, 4), "2": (3.5, 0)}
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {"indexed": [], "offline": [], "refusals": []}
    offline = []

    def options(modality, domain, matrix="X"):
        return json.dumps({"matrix": matrix, "modality": modality, "value_domain": domain}, separators=(",", ":"))

    def positions(name, ids):
        p = inputs / (name + ".tsv")
        p.write_bytes(("observation_id\tx_px\ty_px\n" + "".join(
            f"{id}\t{coordinates[id][0]}\t{coordinates[id][1]}\n" for id in reversed(ids)
        )).encode())
        return p

    def run(cmd, status=0):
        r = subprocess.run(cmd, cwd=root, env=env, capture_output=True, encoding="utf-8", timeout=30)
        assert r.returncode == status, (cmd, r.returncode, r.stdout, r.stderr)
        return r

    def invoke(source, pos, dest, selected):
        base = command + [source, pos, image, dest]
        return base if selected is None else base + ["--plugin-options-json", selected]

    def accept(name, source, selected, observations, features, values, stored=None):
        selected_json = json.loads(selected)
        domain = selected_json["value_domain"]
        counts = domain == "raw-counts"
        pos = positions(name, observations)
        target = root / (name + ".qmx")
        report = json.loads(run(invoke(source, pos, target, selected)).stdout)
        assert (report["observations"], report["features"]) == values.shape
        profile = "external-sparse-file/1" if stored is not None else "external-dense-typed/1"
        entries = int(stored.sum()) if stored is not None else values.size
        assert report["entries"] == entries and report["input_profile"] == profile
        assert report["positions_sha256"] == sha(pos)
        assert report["value_domain"] == domain and report["domain"] == ("counts" if counts else "continuous")
        assert report["receipt"]["source_sha256"] == sha(source)
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        assert json.loads(report["receipt"]["options"]) == selected_json and report["file_sha256"] == sha(target)
        bits = values.astype("<u8" if counts else "<f8").view("<u8")
        feature_rows = []
        for feature in reversed(range(values.shape[1])):
            rows = range(values.shape[0]) if stored is None else np.flatnonzero(stored[:, feature])
            feature_rows.append(" ".join([str(len(rows)), *(f"{o} {bits[o, feature]:016x}" for o in rows)]))
        expected = root / (name + ".oracle")
        content = [report["receipt"]["options"], domain, f"{values.shape[0]} {values.shape[1]}", profile, str(entries),
                   *observations, *features, *(f"{coordinates[id][0]} {coordinates[id][1]}" for id in observations),
                   *feature_rows]
        expected.write_bytes(("\n".join(content) + "\n").encode())
        offline.append((name, target, report["file_sha256"], expected))
        checks["indexed"].append(name)

    def altered(name, change):
        file = inputs / (name + ".h5mu")
        shutil.copyfile(study, file)
        with h5py.File(file, "r+") as h:
            change(h)
        return file

    rna_ids = ["0001", "1"]
    counts = np.array([[2**53 + 1, 0, 2**64 - 1], [17, 53, 0]], dtype="<u8")
    scores = np.array([[-0.0, 2**-1074], [1 + 2**-52, -3.125]])
    protein = np.array([[.25, -.5], [0., 16.]])
    with h5py.File(study, "r") as f:
        assert f["mod/rna/X/data"].shape == (int((counts != 0).sum()),)
        assert f["mod/protein/X/data"].shape == (int((protein != 0).sum()),)
    accept("rna-csr", study, options("rna", "raw-counts"), rna_ids, ["G-01", "G1", "\u03bc-feature"], counts, counts != 0)
    accept("protein-csc", study, options("protein", "normalized-protein"), ["1", "2"], ["P0", "P1"], protein, protein != 0)
    accept("score-dense", study, options("score", "signed-scores"), rna_ids, ["S0", "S1"], scores)

    def add_layer(h):
        layer = h["mod/score/layers"].create_dataset("shifted", data=scores.astype(">f8"),
                                                    compression="gzip", shuffle=True, fletcher32=True, chunks=(1, 2))
        layer.attrs["encoding-type"] = "array"
        layer.attrs["encoding-version"] = "0.2.0"

    layer = altered("compressed-layer", add_layer)
    accept("score-layer", layer, options("score", "signed-scores", "layers/shifted"), rna_ids, ["S0", "S1"], scores)

    pos = positions("controls", rna_ids)
    rna = options("rna", "raw-counts")
    score = options("score", "signed-scores")

    def refuse(name, source, diagnostic, selected=rna, coordinates=pos):
        target = root / (name + ".qmx")
        before = sha(source)
        result = run(invoke(source, coordinates, target, selected), 2)
        assert diagnostic in result.stderr and not target.exists() and sha(source) == before, (name, result.stderr)
        checks["refusals"].append(name)

    refuse("implicit-modality", study, "explicit modality", None)
    refuse("unknown-modality", study, "HDF5", options("absent", "raw-counts"))
    refuse("nonlocal-modality", study, "single group name", options("../rna", "raw-counts"))
    refuse("unknown-layer", study, "HDF5", options("score", "signed-scores", "layers/missing"))
    refuse("wrong-position-axis", study, "position", options("protein", "normalized-protein"))
    refuse("unsupported-version", altered("version", lambda h: h.attrs.modify("encoding-version", "9.9.9")),
           "unsupported encoding")
    refuse("sparse-duplicate", altered("dupes", lambda h: h["mod/rna/X/indices"].__setitem__(1, 0)), "duplicate entries")
    refuse("sparse-index", altered("index", lambda h: h["mod/rna/X/indices"].__setitem__(1, 3)), "exceeds modality axis")
    refuse("sparse-pointer", altered("ptr", lambda h: h["mod/rna/X/indptr"].__setitem__(0, 1)), "sparse pointers")
    refuse("nonfinite", altered("inf", lambda h: h["mod/score/X"].__setitem__((0, 1), np.inf)), "must be finite", score)
    refuse("lossy-interpretation", study, "loses precision", options("rna", "normalized-counts"))

    def external_link(h):
        del h["mod/rna/obs/_index"]
        h["mod/rna/obs/_index"] = h5py.ExternalLink("other.h5ad", "/obs/_index")

    refuse("external-axis", altered("external", external_link), "external/soft selected links")

    scale_baseline = json.loads(args.scale_baseline.read_bytes())
    assert scale_baseline["logical_entries"] == 144_000_000 and scale_baseline["stored_entries"] == 3
    assert scale_baseline["exit_code"] == 2 and "entries exceed import limits" in scale_baseline["diagnostic"]
    large = inputs / "scale.h5mu"
    empty = inputs / "empty.h5mu"
    large_pos = inputs / "scale.tsv"
    shutil.copyfile(args.scale_baseline.with_name("scale.h5mu"), large)
    assert sha(large) == scale_baseline["fixture_sha"].lower()
    shutil.copyfile(args.scale_baseline.with_name("positions.tsv"), large_pos)
    shutil.copyfile(large, empty)
    with h5py.File(empty, "r+") as h:
        dataset = h["mod/rna/X"]
        for key in ("indices", "data"):
            dtype = dataset[key].dtype
            del dataset[key]
            dataset.create_dataset(key, shape=(0,), dtype=dtype)
        dataset["indptr"][...] = 0
    for name, source, entries in (("scale", large, 3), ("empty-scale", empty, 0)):
        target = root / (name + ".qmx")
        report = json.loads(run(invoke(source, large_pos, target, rna)).stdout)
        assert (report["features"], report["observations"], report["entries"]) == (12000, 12000, entries)
        assert report["input_profile"] == "external-sparse-file/1" and report["receipt"]["source_sha256"] == sha(source)
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        assert report["file_sha256"] == sha(target) and report["positions_sha256"] == sha(large_pos)
        assert target.stat().st_size < 8 * 1024 * 1024
        offline.append((name, target, report["file_sha256"], name))
        checks["indexed"].append(name)

    inputs.rename(root / "unavailable")
    for name, target, digest, oracle in offline:
        report = json.loads(run([args.host, "matrix-info", target]).stdout)
        assert report["file_sha256"] == digest and report["receipt"]["provider"]["module_sha256"] == identity[0]
        run([args.probe, target, digest, oracle])
        checks["offline"].append(name)
    assert identity == (sha(module), sha(args.manifest))
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": identity[0], "manifest_sha256": identity[1],
        "baseline_sha256": sha(args.baseline), "scale_baseline_sha256": sha(args.scale_baseline),
        "host_sha256": sha(args.host), "probe_sha256": sha(args.probe), **checks,
    }, indent=2) + "\n", encoding="utf-8")
    print({name: len(items) for name, items in checks.items()})


if __name__ == "__main__":
    main()
