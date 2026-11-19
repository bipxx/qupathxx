"""Development-only synthetic producer proof using pinned upstream AnnData.

This is an independent test fixture recipe, never a shipped format fallback.
The native provider/host remain the sole product interpretation authorities.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata as metadata
import json
from pathlib import Path
import shutil
import sys


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf8")


def inventory(root: Path) -> dict[str, dict[str, object]]:
    return {p.relative_to(root).as_posix(): {"bytes": p.stat().st_size, "sha256": sha(p)}
            for p in sorted(root.rglob("*")) if p.is_file()}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    tools, output = args.tools.resolve(), args.output.resolve()
    assert tools.is_dir(), "Provide the admitted pinned development tool directory"
    assert not output.exists(), "Preserve existing producer fixtures/results; choose a fresh output"
    sys.path.insert(0, str(tools))
    pinned = {"anndata": "0.12.7", "zarr": "2.18.7", "numcodecs": "0.15.1",
              "scipy": "1.16.3", "pandas": "2.3.3", "numpy": "2.5.4", "h5py": "3.15.1"}
    versions = {name: metadata.version(name) for name in pinned}
    assert versions == pinned, ("Producer versions differ", versions)
    import anndata
    from anndata.io import write_zarr
    import numpy as np
    import pandas as pd
    from scipy import sparse
    from numcodecs import Blosc, Zlib

    output.mkdir(parents=True)
    report: dict[str, object] = {
        "schema": "qpx.anndata-zarr-producer-proof/1", "passed": False,
        "scope": "First-party synthetic literals passed to actual pinned upstream AnnData/Zarr writer; independent development evidence, never product runtime/fallback",
        "versions": versions, "writer": "anndata.io.write_zarr(store, adata, compressor=Zlib(level=1), convert_strings_to_categoricals=False)",
        "recipe_sha256": sha(Path(__file__)), "fixtures": {},
    }

    def options(names: bool = False) -> dict[str, str]:
        result = {"matrix": "X", "observation_ids": "obs/_index", "feature_ids": "var/_index",
                  "domain": "counts", "value_domain": "raw-counts"}
        if names:
            result["feature_names"] = "var/feature_name"
        return result

    def positions(name: str, ids: list[str]) -> Path:
        path = output / (name + ".positions.tsv")
        lines = ["observation_id\tx_px\ty_px\n"]
        lines += [f"{identity}\t{(i % 32 + 0.5) / 2}\t{(i // 32 + 0.5) / 2}\n"
                  for i, identity in enumerate(ids)]
        path.write_text("".join(lines), encoding="utf8")
        return path

    def describe(name: str, store: Path, ids: list[str], selected: dict[str, str], expected: object) -> dict[str, object]:
        table = positions(name, ids)
        members = inventory(store)
        arrays = {key: json.loads((store / key).read_text(encoding="utf8"))
                  for key in members if key.endswith(".zarray")}
        value = {"store": str(store), "positions": str(table), "positions_sha256": sha(table),
                 "options": selected, "members": members, "member_count": len(members),
                 "physical_arrays": arrays, "consolidated_metadata": (store / ".zmetadata").is_file(),
                 "expected": expected}
        report["fixtures"][name] = value
        return value

    obs_ids = ["spot-α", "spot-B", "spot-C", "spot-D"]
    feature_ids = ["GENE-A", "GENE-β", "GENE-C"]
    counts = [[9007199254740993, 0, 18446744073709551615], [0, 7, 0], [3, 0, 0], [0, 0, 11]]
    obs = pd.DataFrame(index=pd.Index(np.array(obs_ids, dtype=object), dtype=object))
    var = pd.DataFrame({"feature_name": np.array(["Marker A", "Marker B", "Marker C"], dtype=object)},
                       index=pd.Index(np.array(feature_ids, dtype=object), dtype=object))
    tiny = anndata.AnnData(X=np.array(counts, dtype=np.uint64), obs=obs, var=var)
    tiny_store = output / "tiny-counts.zarr"
    write_zarr(tiny_store, tiny, chunks=(2, 2), compressor=Zlib(level=1), convert_strings_to_categoricals=False)
    expected_tiny = {"features": feature_ids, "observations": obs_ids,
                     "feature_names": ["Marker A", "Marker B", "Marker C"],
                     "shape": [4, 3], "counts_row_major_decimal": [str(v) for row in counts for v in row]}
    describe("tiny-counts", tiny_store, obs_ids, options(True), expected_tiny)

    blosc_store = output / "default-blosc.zarr"
    write_zarr(blosc_store, tiny, chunks=(2, 2), convert_strings_to_categoricals=False)
    default = describe("default-blosc", blosc_store, obs_ids, options(True),
                       expected_tiny)
    assert default["physical_arrays"]["X/.zarray"]["compressor"]["id"] == "blosc"
    assert (blosc_store / "X/0.0").read_bytes()[2] & 2  # Blosc direct-copy frame

    block_obs = [f"repeated-prefix-spot-{i:03d}" for i in range(128)]
    block_features = [f"repeated-prefix-gene-{i:03d}" for i in range(64)]
    block = np.tile(np.arange(64, dtype=np.uint64) % 5, (128, 1))
    repeated = anndata.AnnData(X=block, obs=pd.DataFrame(index=block_obs),
                              var=pd.DataFrame(index=block_features))
    for name, codec in [("compressed-lz4", {}), ("compressed-blosclz", {
            "compressor": Blosc(cname="blosclz", clevel=5, shuffle=Blosc.BITSHUFFLE)})]:
        path = output / f"{name}.zarr"
        write_zarr(path, repeated, chunks=(64, 32), convert_strings_to_categoricals=False, **codec)
        assert not ((path / "X/0.0").read_bytes()[2] & 2)  # genuinely compressed
        describe(name, path, block_obs, options(), {"features": block_features,
            "observations": block_obs, "shape": [128, 64], "row_value_rule": "feature index modulo 5"})

    broken = output / "truncated-blosc.zarr"
    shutil.copytree(output / "compressed-lz4.zarr", broken)
    damaged = broken / "X/0.0"
    damaged.write_bytes(damaged.read_bytes()[:-1])
    describe("truncated-blosc", broken, block_obs, options(), {"refusal_contains": "blosc"})

    n = 1024
    moderate_obs = [f"obs-{i:04d}" for i in range(n)]
    moderate_features = [f"feature-{i:04d}" for i in range(n)]
    matrix = sparse.csr_matrix((np.arange(1, n + 1, dtype=np.uint64),
                                np.arange(n, dtype=np.int32), np.arange(n + 1, dtype=np.int32)), shape=(n, n))
    moderate = anndata.AnnData(X=matrix,
        obs=pd.DataFrame(index=pd.Index(np.array(moderate_obs, dtype=object), dtype=object)),
        var=pd.DataFrame(index=pd.Index(np.array(moderate_features, dtype=object), dtype=object)))
    # These additional synthetic sparse layers exercise real writer-generated
    # store membership. They are omitted by the selected X profile, never
    # mistaken for biological replicates or flattened into the chosen matrix.
    for index in range(8):
        moderate.layers[f"membership-{index:02d}"] = matrix.copy()
    moderate_store = output / "moderate-csr.zarr"
    write_zarr(moderate_store, moderate, compressor=Zlib(level=1), convert_strings_to_categoricals=False)
    medium = describe("moderate-csr", moderate_store, moderate_obs, options(),
        {"shape": [n, n], "stored_entries": n, "nonzero_rule": "count(row=i,feature=i)=i+1; all other coordinates measured0",
         "observations": moderate_obs, "features": moderate_features, "unselected_synthetic_membership_layers": 8})
    assert medium["member_count"] > 64, "Producer proof must exercise more than64 actual store members"

    wide_size = 10240
    wide_ids = [f"axis-{i:05d}" for i in range(wide_size)]
    # The logical size exceeds the former dense cap, and the stored-entry
    # count crosses two 4,096-entry pages. Keep the source genuinely sparse.
    wide_matrix = sparse.csc_matrix((np.arange(1, wide_size + 1, dtype=np.uint64),
        np.arange(wide_size, dtype=np.int32), np.arange(wide_size + 1, dtype=np.int32)),
        shape=(wide_size, wide_size))
    wide = anndata.AnnData(X=wide_matrix, obs=pd.DataFrame(index=wide_ids), var=pd.DataFrame(index=wide_ids))
    wide_store = output / "wide-csc.zarr"
    write_zarr(wide_store, wide, convert_strings_to_categoricals=False)
    describe("wide-csc", wide_store, wide_ids, options(), {"shape": [wide_size, wide_size],
        "stored_entries": wide_size, "nonzero_rule": "count(row=i,feature=i)=i+1"})

    conflict_store = output / "conflict.zarr"
    shutil.copytree(tiny_store, conflict_store)
    physical_before = sha(conflict_store / "X/.zarray")
    consolidated = json.loads((conflict_store / ".zmetadata").read_text(encoding="utf8"))
    consolidated["metadata"]["X/.zarray"]["shape"] = [4, 4]
    write(conflict_store / ".zmetadata", consolidated)
    assert sha(conflict_store / "X/.zarray") == physical_before
    describe("conflict", conflict_store, obs_ids, options(True),
             {"refusal_contains": "consolidated", "physical_shape": [4, 3], "consolidated_shape": [4, 4],
              "physical_array_sha256_unchanged": physical_before})

    report["loaded_upstream_module_files"] = {
        name: {"path": str(Path(module.__file__).resolve()), "sha256": sha(Path(module.__file__))}
        for name, module in sorted(sys.modules.items())
        if getattr(module, "__file__", None) and Path(module.__file__).is_file()
        and (Path(module.__file__).resolve().is_relative_to(tools) or name.split(".")[0] in {"numpy", "h5py"})}
    report["passed"] = True
    write(output / "producer-report.json", report)
    print(json.dumps({"passed": True, "report": str(output / "producer-report.json"),
                      "fixtures": {name: value["member_count"] for name, value in report["fixtures"].items()}}))


if __name__ == "__main__":
    main()
