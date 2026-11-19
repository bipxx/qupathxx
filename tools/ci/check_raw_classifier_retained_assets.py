"""Verify raw classifier outputs respect saved project analysis paths."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("batch", "fixture", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    if args.output.parent != args.fixture.parent:
        parser.error("--output must be a sibling of --fixture to preserve relative image links")
    args.output.mkdir()
    env = {key: value for key, value in os.environ.items() if not key.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, command, exit_code=0):
        result = subprocess.run([args.batch, *command], cwd=args.output, env=env,
                                capture_output=True, encoding="utf-8", timeout=45)
        assert result.returncode == exit_code, (label, result.stdout, result.stderr)
        if exit_code == 2:
            assert not result.stdout, (label, result.stdout)
        response = result.stderr if exit_code == 2 else result.stdout
        checks[label] = json.loads(response.splitlines()[-1])
        return checks[label]

    def copy_asset(folder, pattern):
        source, = (args.fixture / folder).glob(pattern)
        target = args.output / source.relative_to(args.fixture)
        target.parent.mkdir()
        return shutil.copyfile(source, target)

    project = shutil.copyfile(args.fixture / "source.qpxproj", args.output / "source.qpxproj")
    matrix = copy_asset(".qpx-expression", "*.qmx")
    archive = copy_asset(".qpx-expression-reductions", "*.qpr")
    retained_model = args.fixture / "baseline.qrm"
    retained_stream = args.fixture / "baseline.qws"
    preserved = {p: sha256(p) for p in (project, matrix, archive, retained_model, retained_stream)}
    model = args.output / "trained.qrm"
    checkpoint = args.output / "samples.qtc"
    fitted_archive = args.output / "training.qtt"
    stream = args.output / "prediction.qws"
    training_opts = ["--x-axis", "0,8,7,2", "--y-axis", "0,6,5,2", "--channels", "2",
                     "--tile-width", "2", "--tile-height", "2", "--max-samples", "12",
                     "--samples-per-class", "6", "--work-bytes", "16777216", "--trees", "5",
                     "--max-depth", "3", "--forest-work-bytes", "2097152"]
    prediction_opts = ["--model-grid", "--tile-width", "2", "--tile-height", "2"]

    def train(model_dest, checkpoint_dest, archive_dest):
        return ["--train-raw-pixels", project, "2", model_dest, "--checkpoint", checkpoint_dest,
                "--archive", archive_dest, *training_opts]

    def predict(model_source, target):
        return ["--export-raw-pixels", project, "2", model_source, target, *prediction_opts]

    def contour(model_source, target):
        return ["--contour-raw-pixels", project, "2", model_source, target, *prediction_opts]

    def replay(target):
        return ["--replay-raw-contours", project, "2", retained_stream, target]

    commands = {
        "model": lambda target: train(target, checkpoint, fitted_archive),
        "checkpoint": lambda target: train(model, target, fitted_archive),
        "training-archive": lambda target: train(model, checkpoint, target),
        "training-project": lambda target: [*train(model, checkpoint, fitted_archive), "--output-project", target],
        "prediction": lambda target: predict(retained_model, target),
        "prediction-project": lambda target: [*predict(retained_model, stream), "--output-project", target],
        "contour": lambda target: contour(retained_model, target),
        "contour-replay": replay,
    }
    reserved = {"qmx": matrix, "qpr": archive}
    reserved.update({suffix: matrix.with_name(matrix.name + "-" + suffix) for suffix in ("journal", "wal", "shm")})
    offline = {p: p.with_suffix(".offline") for p in (matrix, archive)}
    for original, saved in offline.items():
        original.rename(saved)
    try:
        for route, command in commands.items():
            for name, target in reserved.items():
                refused = run(f"{route}-{name}", command(target), 2)
                assert "must not replace its input" in refused["error"], (route, name, refused)
                assert not target.exists()
                assert not any(path.exists() for path in (model, checkpoint, fitted_archive, stream))
        trained = run("train-offline", train(model, checkpoint, fitted_archive))
        predicted = run("predict-offline", predict(model, stream))
    finally:
        for original, saved in offline.items():
            saved.rename(original)

    def baseline(name):
        return json.loads((args.fixture / (name + "-baseline.jsonl")).read_text(encoding="utf-8").splitlines()[-1])

    prior_training = baseline("training")
    assert trained["state"] == "trained"
    for key in ("rows", "table_sha256", "model_sha256", "request_sha256", "prefix_sha256"):
        assert trained[key] == prior_training[key], key
    verified = run("verify-prediction", ["--verify-raw-pixels", stream])
    assert predicted["state"] == "saved" and verified["state"] == "verified"
    assert predicted["summary"] == verified["summary"]
    prior_prediction = baseline("prediction")["summary"]
    for key in ("input_sha256", "sha256", "class_counts"):
        assert verified["summary"][key] == prior_prediction[key], key
    contoured = run("contour", contour(model, args.output / "contours.qpx"))
    replayed = run("contour-replay", replay(args.output / "replayed.qpx"))
    assert contoured["state"] == replayed["state"] == "saved"
    prior_contour = baseline("contour")
    for key in ("prediction_sha256", "components", "vertices", "class_counts", "object_ids"):
        assert contoured[key] == replayed[key] == prior_contour[key], key
    for label, options, error in (
        ("live-work-cap", ["--total-work-bytes", str(320 * 1024**2 + 131072)],
         "replay file/producer/boundary/geometry"),
        ("file-work-cap", ["--file-work-bytes", str(400 * 1024**2),
                          "--total-work-bytes", str(384 * 1024**2)], "file/producer/boundary/geometry"),
    ):
        target = args.output / (label + ".qpx")
        result = run(label, [*replay(target), *options], 1)
        assert result["state"] == "failed" and error in result["error"] and not target.exists()
    assert all(sha256(p) == digest for p, digest in preserved.items())
    assert not list(args.output.rglob("*.qpx-tmp-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "batch_sha256": sha256(args.batch), "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} raw classifier path protection and replay checks passed")


if __name__ == "__main__":
    main()
