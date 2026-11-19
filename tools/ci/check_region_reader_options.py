"""Compare invocation options with the retained FITS manifest-based controls."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare_snapshots(current, expected):
    for snapshot in (current, expected):
        payload = {key: value for key, value in snapshot.items() if key != "sha256"}
        encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"),
                             ensure_ascii=False).encode("utf-8")
        assert hashlib.sha256(encoded).hexdigest() == snapshot["sha256"]
    for field in ("implementation", "metadata", "raw"):
        assert current[field] == expected[field], field
    # Observation IDs and capture timestamps identify one process/open. The
    # stable reader binding, observed file identity and file versions must agree.
    def stable(observed):
        return {**{key: value for key, value in observed.items() if key not in ("generation", "primary")},
                "primary": {key: value for key, value in observed["primary"].items()
                            if key != "observed_unix_ms"}}

    assert stable(current["observed"]) == stable(expected["observed"])


def verify(region, retained, fits, output):
    output.mkdir(parents=True)
    source = fits / "science-bundle.fits"
    manifest = fits / "package-1.1.0/reader-plugins.json"
    bundle = json.loads(manifest.read_text(encoding="utf-8"))
    module = manifest.parent / bundle["plugins"][0]["module"]["path"]
    artifacts = {path: sha(path) for path in (region, retained, source, manifest, module)}
    assert bundle["plugins"][0]["module"]["sha256"] == artifacts[module]
    windows = Path(os.environ["SystemRoot"])
    env = dict(os.environ, PATH=os.pathsep.join(map(str, (windows / "System32", windows))))
    checks = []

    def run(name, argv, refused=False, diagnostic=None):
        command = list(map(str, argv))
        result = subprocess.run(command, cwd=output, env=env, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        checks.append({"name": name, "argv": command, "exit": result.returncode})
        assert (result.returncode in (1, 2)) if refused else result.returncode == 0, (name, result)
        if diagnostic is not None:
            assert diagnostic.lower() in result.stderr.lower(), (name, result.stderr)
        return result.stdout

    def prefix(binary, configuration=manifest):
        return [binary, "--reader-plugins", configuration, "--reader", "org.qupathxx.fits"]

    def pinned(name, options):
        folder = output / name
        folder.mkdir()
        shutil.copyfile(module, folder / module.name)
        configuration = folder / manifest.name
        configured = json.loads(manifest.read_text(encoding="utf-8"))
        configured["plugins"][0]["options"] = options
        configuration.write_text(json.dumps(configured) + "\n", encoding="utf-8")
        return configuration

    for hdu, z, width, height in ((1, 1, 2, 2), (2, 0, 3, 1)):
        crop = ["--z", z, "--width", width, "--height", height]
        dest = output / f"selected-{hdu}.npy"
        options = {"hdu": hdu}
        info = json.loads(run(f"raw-hdu-{hdu}", [*prefix(region), "--reader-options",
                                               json.dumps(options), "--raw", source, dest, *crop]))
        expected = fits / f"hdu-{hdu}.npy"
        assert dest.read_bytes() == expected.read_bytes()
        snapshot = info["reader_snapshot"]
        assert snapshot["implementation"]["module_sha256"] == artifacts[module]
        assert snapshot["implementation"]["options"] == options
        control = output / f"control-{hdu}.npy"
        config = pinned(f"pinned-hdu-{hdu}", options)
        previous = json.loads(run(f"pinned-raw-{hdu}", [*prefix(retained, config), "--raw",
                                                      source, control, *crop]))
        assert control.read_bytes() == dest.read_bytes()
        compare_snapshots(snapshot, previous["reader_snapshot"])
        ordinary = output / f"ordinary-{hdu}.npy"
        current = json.loads(run(f"ordinary-manifest-{hdu}", [*prefix(region, config), "--raw",
                                                            source, ordinary, *crop]))
        assert ordinary.read_bytes() == control.read_bytes()
        compare_snapshots(current["reader_snapshot"], snapshot)

    with_window = {"window": [-2, 2], "hdu": 2}
    window_manifest = pinned("pinned-window", with_window)
    for name, display in (("region", [0, 0, 0, 3, 1]), ("thumbnail", ["--thumbnail", 3, 1]),
                          ("resample", ["--resample", 0, 0, 0, 3, 1, 3, 1]),
                          ("nominal", ["--rgb-nominal", 0, 0, 3, 1, 1])):
        target = output / (name + "-actual.pam")
        expected = output / (name + "-expected.pam")
        if name in ("thumbnail", "resample", "nominal"):
            mode, *args = display
            tail = [mode, source, *args]
        else:
            tail = [source, *display]
        run("configured-" + name, [*prefix(retained, window_manifest), *tail, expected])
        run("invocation-" + name, [*prefix(region), "--reader-options", json.dumps(with_window),
                                   *tail, target])
        assert target.read_bytes() == expected.read_bytes()
    rgba = (output / "region-actual.pam").read_bytes().split(b"ENDHDR\n", 1)[1]
    assert rgba == bytes((143, 143, 143, 255, 32, 32, 32, 255, 128, 128, 128, 255))

    replacement = output / "replaced-default.npy"
    actual = json.loads(run("replaced-default", [*prefix(region, window_manifest), "--reader-options",
                                              '{"hdu":1}', "--raw", source, replacement, "--z", 1]))
    assert replacement.read_bytes() == (fits / "hdu-1.npy").read_bytes()
    assert actual["reader_snapshot"]["implementation"]["options"] == {"hdu": 1}
    for name, text, diagnostic in (
        ("malformed", "{", "malformed JSON"),
        ("not-object", "[1]", "JSON object"),
        ("missing-hdu", "{}", "hdu"),
        ("unknown-option", '{"hdu":1,"other":true}', "option"),
        ("out-of-range", '{"hdu":63}', "hdu"),
        ("missing-samples", '{"hdu":2}', "requires complete valid samples"),
    ):
        target = output / (name + ".npy")
        run(name, [*prefix(region, window_manifest), "--reader-options", text, "--raw", source, target],
            refused=True, diagnostic=diagnostic)
        assert not target.exists()
    for name, flags in (("builtin", ["--reader", "tiff"]), ("implicit", [])):
        target = output / (name + ".npy")
        run(name, [region, *flags, "--reader-options", "{}", "--raw", source, target],
            refused=True, diagnostic="explicit")
        assert not target.exists()
    occupied = output / "occupied.npy"
    occupied.write_bytes(b"keep existing output")
    run("existing-output", [*prefix(region), "--reader-options", '{"hdu":1}', "--raw", source, occupied],
        refused=True)
    assert occupied.read_bytes() == b"keep existing output"
    assert all(sha(path) == digest for path, digest in artifacts.items())
    (output / "validation.json").write_text(json.dumps({
        "schema": "qpx.reader-invocation-options-validation/1", "passed": True,
        "artifacts": {str(path): digest for path, digest in artifacts.items()}, "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Passed {len(checks)} FITS native region option checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ("region", "retained", "fits", "output"):
        parser.add_argument("--" + argument, type=Path, required=True)
    args = parser.parse_args()
    verify(args.region.resolve(), args.retained.resolve(), args.fits.resolve(), args.output.resolve())
