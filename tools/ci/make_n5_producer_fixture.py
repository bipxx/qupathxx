# SPDX-License-Identifier: AGPL-3.0-only
"""Write calibrated synthetic N5 datasets with the pinned TensorStore driver."""

import argparse
import hashlib
from importlib.metadata import version
import json
from pathlib import Path
import struct

import numpy as np
import tensorstore as ts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New fixture directory")
    args = parser.parse_args()
    versions = {name: version(name) for name in ("tensorstore", "numpy")}
    if versions != {"tensorstore": "0.1.86", "numpy": "2.5.5"}:
        parser.error("Use TensorStore 0.1.86 and NumPy 2.5.5")
    root = args.output.resolve()
    root.mkdir(parents=True)

    shape = (11, 7, 3, 2, 2)
    chunks = (4, 3, 2, 1, 1)
    x, y, z, c, t = np.indices(shape, dtype=np.uint16)
    samples = (x + 101 * y + 4000 * z + 15000 * c + 30000 * t).astype("<u2")
    samples[:4, :3, :2, 0, 0] = 0
    receipt = {"versions": versions, "datasets": {}}
    for name, compression in (
        ("raw", {"type": "raw"}),
        ("gzip", {"type": "gzip", "level": 5}),
        ("zlib", {"type": "gzip", "level": 5, "useZlib": True}),
    ):
        dataset = root / name / "cells"
        spec = {
            "driver": "n5",
            "kvstore": {"driver": "file", "path": dataset.as_posix() + "/"},
            "metadata": {"dimensions": shape, "blockSize": chunks, "dataType": "uint16",
                         "compression": compression, "axes": ["x", "y", "z", "c", "t"],
                         "units": ["nm", "nm", "nm", "", "s"],
                         "resolution": [250, 500, 1500, 1, 2.5]},
        }
        created = ts.open(spec, create=True).result()
        created.write(samples).result()
        reread = ts.open({"driver": "n5", "kvstore": spec["kvstore"]}, open=True).result()
        np.testing.assert_array_equal(reread.read().result(), samples)
        assert not (dataset / "0/0/0/0/0").exists(), "Expected an omitted fill block"
        corner = dataset / "2/2/1/1/1"
        mode, rank, *edge = struct.unpack(">HH5I", corner.read_bytes()[:24])
        assert (mode, rank) == (0, 5)
        members = {path.relative_to(dataset).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                   for path in sorted(dataset.rglob("*")) if path.is_file()}
        receipt["datasets"][name] = {
            "path": f"{name}/cells", "attributes": json.loads((dataset / "attributes.json").read_bytes()),
            "corner_block_shape": edge, "members": members,
        }
    # Test data is addressed T, C, Z, Y, X by the native checker.
    expected = samples.transpose(4, 3, 2, 1, 0).copy(order="C")
    np.save(root / "expected-tczyx.npy", expected, allow_pickle=False)
    receipt["raw_le_u16_sha256"] = hashlib.sha256(expected.tobytes()).hexdigest()
    (root / "producer.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
