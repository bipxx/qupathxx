"""Check RGB output paths and compare prediction arrays using the retained SDK."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import numpy as np
from qupathxx import PixelStreamFile


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("batch", "fixture", "model", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    if args.output.parent != args.fixture.parent:
        parser.error("--output must be a sibling of --fixture to preserve relative image links")
    args.output.mkdir()
    windows = Path(os.environ["SYSTEMROOT"])
    env = {key: value for key, value in os.environ.items() if not key.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, command, expected=0):
        result = subprocess.run([args.batch, *command], cwd=args.output, env=env,
                                capture_output=True, encoding="utf-8", timeout=45)
        assert result.returncode == expected, (label, result.stdout, result.stderr)
        response = (result.stdout or result.stderr).strip()
        payload = json.loads(response if command[0] == "--estimate-stains" else response.splitlines()[-1])
        checks[label] = payload
        return payload

    project = shutil.copyfile(args.fixture / "source.qpxproj", args.output / "source.qpxproj")

    def copy_asset(folder, pattern):
        source, = (args.fixture / folder).glob(pattern)
        target = args.output / source.relative_to(args.fixture)
        target.parent.mkdir()
        return shutil.copyfile(source, target)

    matrix = copy_asset(".qpx-expression", "*.qmx")
    archive = copy_asset(".qpx-expression-reductions", "*.qpr")
    before = {path: sha256(path) for path in (project, matrix, archive, args.model)}

    def pixels(path):
        return ["--export-pixels", project, "2", "1", args.model, path, "--block-size", "16",
                "--window", "16,16,16,16"]

    def stains(path):
        return ["--estimate-stains", project, path, "--image", "2", "--roi", "1"]

    routes = {"pixels": (pixels, 1), "stains": (stains, 2)}
    companions = {suffix: matrix.with_name(matrix.name + "-" + suffix) for suffix in ("journal", "wal", "shm")}
    offline = {path: path.with_suffix(".offline") for path in (matrix, archive)}
    prior_stream = run("verify-retained-pixels", ["--verify-pixels", args.fixture / "retained-pixels.qps"])
    prior_block = PixelStreamFile(str(args.fixture / "retained-pixels.qps")).read_block(0)
    prior_model = json.loads((args.fixture / "retained-stains.json").read_bytes())

    def normal_outputs(label):
        stream = args.output / (label + ".qps")
        exported = run(label + "-pixels", pixels(stream))
        verified = run(label + "-verify", ["--verify-pixels", stream])
        assert exported["state"] == "saved" and verified["state"] == "verified"
        assert exported["summary"] == verified["summary"]
        for key in ("blocks", "valid_pixels", "classified_pixels", "class_counts"):
            assert verified["summary"][key] == prior_stream["summary"][key], key
        # The verified file checksum covers capture provenance as well as pixels.
        block = PixelStreamFile(str(stream)).read_block(0)
        for method in ("labels_numpy", "valid_numpy", "vote_fractions_numpy"):
            np.testing.assert_array_equal(getattr(block, method)(), getattr(prior_block, method)())
        estimate = args.output / (label + ".json")
        report = run(label + "-stains", stains(estimate))
        assert json.loads(estimate.read_bytes()) == report
        for key in ("input_sha256", "model_sha256", "stains", "samples", "eligible", "sampled"):
            assert report[key] == prior_model[key], key

    for original, saved in offline.items():
        original.rename(saved)
    try:
        for route, (command, status) in routes.items():
            for name, target in {"qmx": matrix, "qpr": archive, **companions}.items():
                refused = run(f"{route}-{name}", command(target), status)
                assert "must not replace its input" in refused["error"], (route, name, refused)
                assert not target.exists()
        normal_outputs("offline")
    finally:
        for original, saved in offline.items():
            saved.rename(original)

    alias = args.output / "matrix-alias.qps"
    os.link(matrix, alias)
    refused = run("pixel-replace-matrix-alias", [*pixels(alias), "--replace-existing"], expected=1)
    assert "aliases its input" in refused["error"]
    assert sha256(alias) == before[matrix]
    normal_outputs("online")
    assert all(sha256(path) == digest for path, digest in before.items())
    assert not list(args.output.rglob("*.qpx-tmp-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "batch_sha256": sha256(args.batch), "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} RGB export path protection and replay checks passed")


if __name__ == "__main__":
    main()
