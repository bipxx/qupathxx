"""Verify offline QMX frame inspection and implicit source-mode ROI selection."""

import argparse
from hashlib import sha256
import json
from pathlib import Path
import subprocess


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("format", "batch", "fixtures", "apng-manifest", "fits-manifest", "output"):
        parser.add_argument("--" + key, type=lambda value: Path(value).resolve(), required=True)
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True)
    fixtures = args.fixtures
    checks = {}

    def invoke(name, executable, command, error=None):
        response = subprocess.run([executable, *command], cwd=root, capture_output=True,
                                  encoding="utf-8", timeout=60)
        (root / f"{name}.log").write_text(response.stdout + response.stderr, encoding="utf-8")
        assert response.returncode == (2 if error else 0), (name, response.stdout, response.stderr)
        record = json.loads(response.stderr if error else response.stdout)
        if error:
            assert error in record["error"], (name, record)
        checks[name] = record
        return record

    original = fixtures / "project-indexed.qmx"
    info = invoke("offline-matrix-info", args.format, ["matrix-info", original])
    attach = json.loads((fixtures / "project-indexed-matching-attach.log").read_bytes())
    assert info["file_sha256"] == digest(original) == attach["file_sha256"]
    assert info["frame_sha256"] == attach["frame_sha256"]
    frame = info["spatial_frame"]
    assert frame["id"] == "qpx.format-import-cli-image/2" and frame["unit"] == "pixel"
    assert frame["image_level_zero"] and frame["width"] > 0 and frame["height"] > 0
    assert frame["plane"] == {"channel": -1, "z": 0, "time": 0}
    other_info = invoke("same-canvas-alternate-info", args.format, ["matrix-info", fixtures / "view-1.qmx"])
    other = other_info["spatial_frame"]
    assert other_info["frame_sha256"] != info["frame_sha256"] and other["source_id"] != frame["source_id"]
    assert (other["width"], other["height"], other["plane"]) == (frame["width"], frame["height"], frame["plane"])
    store = invoke("directory-store-info", args.format, ["matrix-info", fixtures / "project-indexed-store.qmx"])
    assert store["source_scope"] == "directory_store"
    assert store["spatial_frame"]["id"] == frame["id"] and store["spatial_frame"]["source_id"] != frame["source_id"]

    readers = ["--reader-plugins", args.apng_manifest, "--reader-plugins", args.fits_manifest]
    bindings = []
    for selected_frame in (0, 1):
        project_path = fixtures / f"frame-{selected_frame}.qpx"
        project_digest = digest(project_path)
        offline_images = invoke(f"project-frame-{selected_frame}-offline", args.format,
                                ["project-images", project_path])["images"]
        online_images = invoke(f"project-frame-{selected_frame}-installed", args.format,
                               [*readers, "project-images", project_path])["images"]
        assert online_images == [dict(image, available=True) for image in offline_images]
        apng, fits = offline_images
        for image, reader, options in [
            (apng, "org.qupathxx.apng", {"frame": selected_frame}),
            (fits, "org.qupathxx.fits", {"hdu": 2}),
        ]:
            assert image["reader"] == reader and not image["available"]
            plugin = image["reader_plugin"]
            assert plugin["options"] == options and plugin["version"]
            provider, contract, digest_hex = plugin["binding"].split(":")
            assert (provider, contract) == (reader, "plugin/1")
            assert len(bytes.fromhex(digest_hex)) == 32
        assert digest(project_path) == project_digest
        bindings.append((apng["reader_plugin"]["binding"], fits["reader_plugin"]["binding"]))
    assert bindings[0][0] != bindings[1][0] and bindings[0][1] == bindings[1][1]

    project = fixtures / "reader-bound-annotated.qpx"
    legacy = fixtures / "legacy-owned-annotated.qpx"
    input_digests = {path: digest(path) for path in (project, legacy, original)}
    explicit = json.loads((fixtures / "reader-bound-roi.json").read_bytes())
    assert explicit["frame_id"] == frame["id"]
    implicit = {key: value for key, value in explicit.items() if key != "frame_id"}

    def aggregate(name, spec, project_path=project, association=attach["association_id"], error=None):
        spec_path = root / f"{name}.json"
        spec_path.write_text(json.dumps(spec), encoding="utf-8")
        output = root / f"{name}.qpx"
        record = invoke(name, args.batch, [*readers, "--aggregate-expression", project_path, output,
                        "--image", "1", "--matrix", association, "--spec", spec_path], error)
        assert output.is_file() == (error is None)
        return output, record

    _, explicit_record = aggregate("explicit", explicit)
    saved, result = aggregate("implicit", implicit)
    baseline = json.loads((fixtures / "reader-bound-aggregate.log").read_bytes())
    for key in ("record", "result_sha256"):
        assert explicit_record[key] == result[key] == baseline[key]
    aggregate("matching-plane", dict(implicit, z=0, time=0))
    aggregate("conflicting-plane", dict(implicit, z=1), error="ROI plane differs from the stored QMX frame")
    aggregate("implicit-registration", dict(implicit, coordinates="registered", registration="alignment"),
              error="Omitting frame_id requires a QMX level-zero pixel frame in source mode")

    legacy_attach = json.loads((fixtures / "legacy-matching-attachment.log").read_bytes())
    legacy_spec = json.loads((fixtures / "legacy-owned-roi.json").read_bytes())
    _, legacy_result = aggregate("explicit-legacy", legacy_spec, legacy, legacy_attach["association_id"])
    legacy_baseline = json.loads((fixtures / "legacy-owned-aggregate.log").read_bytes())
    assert legacy_result["result_sha256"] == legacy_baseline["result_sha256"]
    del legacy_spec["frame_id"]
    aggregate("implicit-legacy", legacy_spec, legacy, legacy_attach["association_id"],
              error="Omitting frame_id requires a reader-bound frame for a plugin image")

    flags = ["--image", "1", "--record", result["record"]]
    replay = invoke("offline-replay", args.batch, ["--verify-expression-aggregation", saved, *flags])
    assert replay["result_sha256"] == result["result_sha256"]
    exported = root / "sums.csv"
    invoke("offline-export", args.batch, ["--export-expression-aggregation", saved, exported, *flags])
    assert exported.read_bytes() == (fixtures / "reader-bound-sums.csv").read_bytes()
    assert input_digests == {path: digest(path) for path in input_digests}
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "format_sha256": digest(args.format), "batch_sha256": digest(args.batch),
        "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(checks)} QMX frame and ROI checks")


if __name__ == "__main__":
    main()
