"""Independent development oracle; never imported by product workflows.

The native tests consume the frozen JSON and do not need NumPy/Python.
Rebuild this authored fixture only when its declared numeric inputs change.
"""
import hashlib
import json
from pathlib import Path
import numpy as np


def main():
    root = Path(__file__).resolve().parents[1]
    output = root / "tests/fixtures/expression-reduction"
    output.mkdir(exist_ok=True)
    raw = np.array([[10, 0, 1, 100], [9, 0, 2, 100], [11, 0, 0, 100],
                    [0, 10, 1, 100], [0, 9, 2, 100], [0, 11, 0, 100]], dtype=float)
    normalized = raw / raw.sum(axis=1)[:, None] * 100
    cases = {}
    for name, values, center in [("raw", raw[:, :3], True),
                                  ("uncentered", raw[:, :3], False),
                                  ("normalized", normalized[:, :3], True),
                                  ("log", np.log1p(normalized[:, :3]), True)]:
        means = values.mean(axis=0)
        matrix = values - means if center else values
        _, singular, vt = np.linalg.svd(matrix, full_matrices=False)
        loadings = vt[:2].T
        scores = matrix @ loadings
        cases[name] = dict(means=means.tolist(), singular=singular[:2].tolist(),
                           total_energy=float(np.square(matrix).sum()),
                           component_energy=np.square(scores).sum(axis=0).tolist(),
                           score_gram=(scores @ scores.T).tolist(),
                           loading_projector=(loadings @ loadings.T).tolist())
    data = dict(schema="qpx.expression-reduction-oracle/1", numpy=np.__version__,
                source="Independent NumPy/LAPACK SVD of authored6x4 values; not native output",
                raw=raw.astype(int).tolist(), selected_features=[0, 1, 2],
                normalization_target=100, normalization_universe=4,
                absolute_tolerance=1e-8, relative_tolerance=1e-9, cases=cases)
    reference = output / "reference.json"
    reference.write_text(json.dumps(data, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    blob = reference.read_bytes()
    record = dict(filename="expression-reduction/reference.json", bytes=len(blob),
                  sha256=hashlib.sha256(blob).hexdigest(), generator="tools/reference_expression_reduction.py",
                  purpose="Authored low-rank PCA/SVD/normalization oracle; no biological input or claim",
                  material=dict(id="qupathxx-authored-expression-reduction", category="admitted_open",
                                kind="evaluation_data", license_id="AGPL-3.0-only",
                                license_evidence="LICENSE", attribution="qupathxx contributors; authored numeric inputs",
                                source_ids=[]))
    manifest = root / "tests/fixtures/manifest.json"
    inventory = json.loads(manifest.read_text(encoding="utf-8"))
    inventory["expression_reduction"] = record
    manifest.write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
    print(f"Frozen independent SVD oracle: {reference}, {len(blob)} bytes, NumPy{np.__version__}")


if __name__ == "__main__":
    main()
