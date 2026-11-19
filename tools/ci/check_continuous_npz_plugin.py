"""Test the installed continuous NPZ provider and offline native QMX replay."""
import argparse
import json
import os
from pathlib import Path
import subprocess

import numpy as np

from check_counts_npz_plugin import sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "manifest", "probe", "baseline", "output"):
        parser.add_argument("--" + key, required=True, type=lambda path: Path(path).resolve())
    args = parser.parse_args()
    baseline = json.loads(args.baseline.read_bytes())
    item, = json.loads(args.manifest.read_bytes())["plugins"]
    module = args.manifest.with_name(item["module"]["path"])
    identity = (sha(module), sha(args.manifest))
    assert item["provider_id"] == "org.qupathxx.continuous-npz" and item["abi"] == 2
    assert item["module"]["sha256"] == identity[0]
    out = args.output
    sources = out / "sources"
    sources.mkdir(parents=True)
    image = out / "reference.ppm"
    image.write_bytes(b"P6\n8 6\n255\n" + bytes(8 * 6 * 3))
    positions = sources / "positions.tsv"
    positions.write_bytes(b"observation_id\tx_px\ty_px\n1\t6\t4\n0001\t1.25\t2.5\n")
    domain = "normalized-expression"
    options = json.dumps({"value_domain": domain})
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    command = [args.host, "--format-plugins", args.manifest, "matrix-index", item["provider_id"]]
    checks = {"indexed": [], "offline": [], "refusals": []}

    def run(cmd, status=0):
        result = subprocess.run(cmd, cwd=out, env=env, capture_output=True, encoding="utf-8", timeout=30)
        assert result.returncode == status, (cmd, result.returncode, result.stdout, result.stderr)
        return result

    def invoke(source, dest, selected=options, coordinates=positions):
        base = command + [source, coordinates, image, dest]
        return base if selected is None else base + ["--plugin-options-json", selected]

    retained = sources / "retained.npz"
    retained.write_bytes(args.baseline.with_name("study-values.npz").read_bytes())
    assert sha(retained) == baseline["fixture_sha"].lower()
    obs = ["0001", "1"]
    features = ["G-01", "G1", "\u03bc-feature"]
    wide = np.asfortranarray([[-0.0, 1 + 2**-52, -7.25], [2**-1074, 3.5, 0.0]], dtype=">f8")
    narrow = np.array([[-0.0, 2**-149, -8.5], [1 + 2**-23, 0.0, 6.25]], dtype="<f4")

    def produce(name, matrix=narrow, observations=obs, cols=features, compress=False, **extra):
        path = sources / (name + ".npz")
        save = np.savez_compressed if compress else np.savez
        save(path, values=matrix, observation_ids=observations, feature_ids=cols, **extra)
        return path

    offline = []
    for name, source, expected in (
        ("retained", retained, wide),
        ("stored-float32", produce("stored", observations=[b"0001", b"1"]), narrow),
        ("compressed-float64", produce("compressed", wide.astype("<f8", order="C"), compress=True), wide),
    ):
        with np.load(source, allow_pickle=False) as archive:
            values = archive["values"]
            assert values.astype("<f8").tobytes() == expected.astype("<f8").tobytes()
            bits = values[:, [2, 0, 1]].T.astype("<f8").view("<u8").ravel()
        target = out / (name + ".qmx")
        report = json.loads(run(invoke(source, target)).stdout)
        assert report["domain"] == "continuous" and report["value_domain"] == domain
        assert (report["observations"], report["features"], report["entries"]) == (2, 3, 6)
        assert report["file_sha256"] == sha(target) and report["positions_sha256"] == sha(positions)
        assert report["receipt"]["source_sha256"] == sha(source)
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        offline.append((name, target, report["file_sha256"], [format(bit, "016x") for bit in bits]))
        checks["indexed"].append(name)

    def refuse(name, source, diagnostic, selected=options, coordinates=positions):
        target = out / (name + ".qmx")
        before = sha(source)
        result = run(invoke(source, target, selected, coordinates), 2)
        assert diagnostic in result.stderr and not target.exists() and sha(source) == before, result.stderr
        checks["refusals"].append(name)

    refuse("absent-domain", retained, "explicit value-domain", None)
    refuse("raw-counts-domain", retained, "other than raw-counts", '{"value_domain":"raw-counts"}')
    refuse("extra-option", retained, "JSON object member limit", '{"value_domain":"normalized","extra":0}')
    refuse("wrong-dtype", produce("integers", np.ones((2, 3), dtype="u8")), "require a Float32 or Float64")
    refuse("wrong-shape", produce("short", narrow[:1]), "axis lengths differ")
    refuse("duplicate-axis", produce("dupes", observations=["1", "1"]), "duplicated")
    refuse("extra-member", produce("extra", other=np.zeros(1)), "exactly three")
    for name, nonfinite, dtype in (("nan", np.nan, ">f8"), ("inf", np.inf, "<f4"), ("neg-inf", -np.inf, "<f8")):
        broken = narrow.astype(dtype)
        broken[1, 2] = nonfinite
        refuse(name, produce("source-" + name, broken), "must be finite")
    missing = sources / "missing.tsv"
    missing.write_bytes(b"observation_id\tx_px\ty_px\n1\t6\t4\n")
    refuse("missing-position", retained, "position", coordinates=missing)
    sources.rename(out / "unavailable-sources")
    image.rename(out / "unavailable-sources" / image.name)
    for name, target, digest, bits in offline:
        report = json.loads(run([args.host, "matrix-info", target]).stdout)
        assert report["file_sha256"] == digest and report["value_domain"] == domain
        run([args.probe, target, digest, "continuous", domain, *bits])
        checks["offline"].append(name)
    assert identity == (sha(module), sha(args.manifest))
    (out / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": identity[0], "manifest_sha256": identity[1],
        "baseline_sha256": sha(args.baseline), "host_sha256": sha(args.host), "probe_sha256": sha(args.probe),
        **checks,
    }, indent=2) + "\n", encoding="utf-8")
    print({key: len(result) for key, result in checks.items()})


if __name__ == "__main__":
    main()
