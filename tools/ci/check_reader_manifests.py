"""Verify two relocated reader packs and exact saved project links."""
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import sqlite3
import subprocess


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("old-region", "region", "app", "probe", "format-import", "envi-manifest",
                 "fits-manifest", "point-manifest", "envi-source", "fits-source", "output"):
        parser.add_argument(f"--{name}", type=lambda value: Path(value).resolve(), required=True)
    args = parser.parse_args()
    first, second = args.envi_manifest, args.fits_manifest
    root = args.output
    root.mkdir(parents=True)
    input_paths = {
        "envi_manifest": first, "fits_manifest": second, "point_manifest": args.point_manifest,
        "envi_source": args.envi_source, "envi_companion": args.envi_source.with_suffix(".img"),
        "fits_source": args.fits_source,
    }
    originals = {name: digest(path) for name, path in input_paths.items()}
    system = Path(os.environ["SYSTEMROOT"])
    env = {k: v for k, v in os.environ.items() if not k.startswith(("QPX_", "QUPATHXX_", "PYTHON"))}
    env["PATH"] = os.pathsep.join(map(str, (system / "System32", system)))
    checks = {}

    def flags(*manifests):
        return [argument for path in manifests for argument in ("--reader-plugins", path)]

    def run(executable, *arguments, status=0):
        command = [executable, *arguments]
        result = subprocess.run(command, cwd=root, env=env, encoding="utf-8",
                                capture_output=True, timeout=60)
        assert result.returncode == status, (command, result.returncode, result.stdout, result.stderr)
        return result

    def catalog(executable, *manifests):
        return json.loads(run(executable, *flags(*manifests), "--readers").stdout)

    first_id, second_id = "org.qupathxx.envi", "org.qupathxx.fits"

    def providers(report):
        return {entry["provider_id"]: entry for entry in report["readers"] if entry["available"]}

    for path, provider_id in ((first, first_id), (second, second_id)):
        before = catalog(args.old_region, path)
        after = catalog(args.region, path)
        assert not after["plugin_diagnostics"] and provider_id in providers(after)
        assert before == after
    checks["single_pack_compatibility"] = [first_id, second_id]

    combined = catalog(args.region, first, second)
    assert not combined["plugin_diagnostics"]
    for executable in (args.region, args.app):
        reverse = catalog(executable, second, first)
        assert not reverse["plugin_diagnostics"] and providers(reverse) == providers(combined)
        assert {first_id, second_id} <= providers(reverse).keys()
    checks["both_orders_and_desktop_catalog"] = combined

    missing = root / "missing.json"
    partial = catalog(args.region, first, missing, second)
    assert providers(partial) == providers(combined)
    assert len(partial["plugin_diagnostics"]) == 1 and str(missing) in partial["plugin_diagnostics"][0]
    checks["missing_manifest_isolation"] = partial["plugin_diagnostics"]

    claimant = root / "duplicate provider.json"
    declaration = json.loads(first.read_bytes())
    declaration["plugins"][0]["module"]["path"] = "absent.dll"
    claimant.write_text(json.dumps(declaration), encoding="utf-8")
    for order in ((first, claimant), (claimant, first)):
        conflicted = catalog(args.region, *order, second)
        assert first_id not in providers(conflicted) and second_id in providers(conflicted)
        messages = conflicted["plugin_diagnostics"]
        assert len(messages) == 2 and all(f"multiple manifests: {first_id}" in msg for msg in messages)
        assert all(any(str(path) in msg for msg in messages) for path in order)
    checks["duplicate_claim_isolation_both_orders"] = messages

    rejected = run(args.region, *flags(first, first.parent / "." / first.name), "--readers", status=2)
    assert "Duplicate reader plugin manifest path" in rejected.stderr
    rejected = run(args.region, *flags(*(root / f"{i}.json" for i in range(33))), "--readers", status=2)
    assert "between 1 and 32" in rejected.stderr
    checks["duplicate_path_and_count_refusals"] = True

    project = root / "two-readers.qpx"
    pair = [project, first, first_id, args.envi_source, second, second_id, args.fits_source]
    run(args.probe, "installed-create", *pair)
    run(args.probe, "installed-open", *pair)
    checks["saved_project_raw_and_persisted_links"] = {
        "project_sha256": digest(project), "resaved_sha256": digest(root / "resaved.qpx"),
    }
    run(args.app, *flags(first, second), "--project", project, "--image-id", "1", "--smoke")
    run(args.app, *flags(second, first), "--project", project, "--image-id", "2", "--smoke")
    checks["desktop_reopens_both_saved_images"] = True
    report = run(args.format_import, *flags(first, second), "catalog")
    assert json.loads(report.stdout)["schema"] == "qpx.format-plugin-catalog/1"
    points = root / "marks.tsv"
    points.write_bytes(b"id\tx\ty\tc\tz\tt\tclass\n0001\t0.5\t0.5\t-1\t0\t0\t\n")
    prior = project
    imports = []
    for image in ("1", "2"):
        marked = root / f"marked-{image}.qpx"
        outcome = run(args.format_import, *flags(second, first), "--format-plugins",
                      args.point_manifest, "points-import", "org.qupathxx.point-tsv",
                      prior, image, points, marked)
        imported = json.loads(outcome.stdout)
        assert imported["state"] == "saved" and imported["image_id"] == int(image)
        assert imported["objects"] == [{"object_id": 1, "source_id": "0001"}]
        imports.append(imported)
        prior = marked

    def links(path):
        with sqlite3.connect(path.as_uri() + "?mode=ro", uri=True) as db:
            return db.execute("SELECT image_id,payload FROM plugin_links ORDER BY image_id").fetchall()

    original_links = links(project)
    assert len(original_links) == 2 and original_links == links(prior)
    with sqlite3.connect(prior.as_uri() + "?mode=ro", uri=True) as db:
        assert db.execute("SELECT image_id,id FROM objects ORDER BY image_id,id").fetchall() == [
            ("1", "1"), ("2", "1"),
        ]
    checks["format_import_on_both_saved_images"] = imports

    assert originals == {name: digest(path) for name, path in input_paths.items()}
    binaries = {name: digest(getattr(args, name))
                for name in ("region", "old_region", "app", "probe", "format_import")}
    report = {"passed": True, "checks": checks, "sha256": {**binaries, **originals}}
    (root / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} check groups passed")


if __name__ == "__main__":
    main()
