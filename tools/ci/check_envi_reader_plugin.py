"""Verify the installed ENVI module against retained and authored rasters."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

import numpy as np


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("region", "probe", "manifest", "baseline", "output"):
        parser.add_argument("--" + name, required=True, type=lambda path: Path(path).resolve())
    parser.add_argument("--format-cli", type=lambda path: Path(path).resolve(),
                        help="Exercise explicit companion layouts and saved project source review")
    args = parser.parse_args()
    item, = json.loads(args.manifest.read_bytes())["plugins"]
    module = args.manifest.with_name(item["module"]["path"])
    identity = (sha(module), sha(args.manifest))
    assert item["module"]["sha256"] == identity[0] and item["provider_id"] == "org.qupathxx.envi"
    baseline = json.loads(args.baseline.read_bytes())
    assert baseline["host_sha"].lower() == sha(args.region)
    root = args.output
    inputs = root / "input pairs"
    inputs.mkdir(parents=True)
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    base = [args.region, "--reader-plugins", args.manifest, "--reader", item["provider_id"]]
    rows = {k: [] for k in ("raw", "display", "validity", "refusals", "project")}

    def run(command, code=0):
        r = subprocess.run(command, cwd=root, env=env, capture_output=True, encoding="utf-8", timeout=30)
        assert r.returncode == code, (command, r.returncode, r.stdout, r.stderr)
        return r

    names = ["Blue band", "Green band", "Red band"]
    cube = np.arange(36, dtype="<u2").reshape(3, 4, 3) * 137
    for entry in baseline["files"]:
        copied = inputs / entry["file"]
        shutil.copyfile(args.baseline.parent / copied.name, copied)
        assert sha(copied) == entry["Hash"].lower()

    def raw(label, header, expected, bands, *options, companion_path=None, reader_options=()):
        target = root / (label + ".npy")
        report = json.loads(run(base + [*reader_options, "--raw", header, target, *options]).stdout)
        value = np.load(target, allow_pickle=False)
        assert value.shape == expected.shape and value.dtype == expected.dtype
        assert value.tobytes() == expected.tobytes(), label
        snapshot = report["reader_snapshot"]
        assert snapshot["raw"]["channel_names"] == bands
        assert snapshot["raw"]["pixel_width_um"] is None and snapshot["raw"]["pixel_height_um"] is None
        assert report["valid_pixels"] == expected.shape[0] * expected.shape[1]
        assert snapshot["implementation"]["module_sha256"] == identity[0]
        assert snapshot["observed"]["scope"] == "compound_files"
        companion, = snapshot["observed"]["companions"]
        assert Path(companion["path"]).samefile(companion_path or header.with_suffix(".img"))
        rows["raw"].append(label)
        return target

    for layout in ("bip", "bil", "bsq"):
        header = inputs / (layout + ".hdr")
        raw(layout, header, cube, names, "--channels", "3")
        raw(layout + "-crop", header, cube[1:3, 2:4, 1:3], names, "--channel", "1", "--channels", "2",
            "--x", "2", "--y", "1", "--width", "2", "--height", "2")

    def pair(label, values, code, layout="bip", byte_order=0, offset=b"", extra="", bands=None):
        header = inputs / (label + ".hdr")
        height, width, channels = values.shape
        fields = f"ENVI\nsamples = {width}\nlines = {height}\nbands = {bands or channels}\nheader offset = {len(offset)}\n"
        fields += f"file type = ENVI Standard\ndata type = {code}\ninterleave = {layout}\nbyte order = {byte_order}\n"
        header.write_bytes((fields + extra).encode("ascii"))
        ordered = {"bip": values, "bil": values.transpose(0, 2, 1), "bsq": values.transpose(2, 0, 1)}.get(layout, values)
        encoded = ordered.byteswap() if byte_order == 1 and ordered.dtype.itemsize > 1 else ordered
        header.with_suffix(".img").write_bytes(offset + encoded.tobytes())
        return header

    for code, dtype in ((1, "u1"), (2, "<i2"), (3, "<i4"), (13, "<u4")):
        info = np.iinfo(dtype)
        value = np.array([info.min, info.max, 1, 2, 17, 0], dtype=dtype).reshape(2, 3, 1)
        name = "data-type-" + str(code)
        h = pair(name, value, code, "bsq", 1, b"preamble")
        raw(name, h, value, ["Band 1"])

    def render(name, header, expected):
        output = root / (name + ".pam")
        run(base + [header, "0", "0", "0", str(expected.shape[1]), str(expected.shape[0]), output])
        assert output.read_bytes().split(b"ENDHDR\n", 1)[1] == expected.tobytes()
        rows["display"].append(name)
        return output

    def display(name, header, gray, alpha):
        return render(name, header, np.stack((gray, gray, gray, alpha), axis=-1).astype("u1"))

    uint_gray = np.floor(cube[:, :, 0].astype(float) * 255 / 65535 + .5)
    display("first-band", inputs / "bil.hdr", uint_gray, np.full((3, 4), 255))
    for code, width, bits in (
        (4, 4, [0x80000000, 0x3f000000, 0x3f800000, 0x7f800000, 0x7f800001, 1]),
        (5, 8, [0x8000000000000000, 0x3fe0000000000000, 0x3ff0000000000000, 0x7ff0000000000000,
                0x7ff0000000000001, 1]),
    ):
        value = np.array(bits, dtype=f"<u{width}").view(f"<f{width}").reshape(2, 3, 1)
        name = "data-type-" + str(code)
        h = pair(name, value, code, "bil", 1, b"header bytes", "band names = {\n Scalar value\n}\n")
        raw(name, h, value, ["Scalar value"])
        display(name, h, np.array([[0, 128, 255], [0, 0, 0]]), np.array([[255, 255, 255], [0, 0, 255]]))

    def refuse(label, header, diagnostic, *options, reader_options=()):
        target = root / (label + ".npy")
        before = sha(header)
        result = run(base + [*reader_options, "--raw", header, target, *options], 2)
        assert diagnostic in (result.stdout + result.stderr), (label, result.stdout, result.stderr)
        assert not target.exists() and sha(header) == before
        rows["refusals"].append(label)

    scalar = np.zeros((2, 3, 1), dtype="<u2")
    refuse("unknown-interleave", pair("interleave", scalar, 12, "xyz"), "Unknown ENVI interleave")
    refuse("duplicate-key", pair("duplicate", scalar, 12, extra="SAMPLES = 3\n"), "duplicate ENVI field")
    refuse("georeferenced", pair("map", scalar, 12, extra="map info = {UTM}\n"), "uncalibrated raster profile")
    for value, error in (("-1", "outside the sample range"), ("65536", "outside the sample range"),
                         ("0.5", "Invalid ENVI data ignore value"), ("+-1", "Invalid ENVI data ignore value")):
        name = f"bad-ignore-{len(rows['refusals'])}"
        refuse(name, pair(name + "-src", scalar, 12, extra=f"data ignore value = {value}\n"), error)
    floats = np.array([[[0.1], [0.5], [1.0]]], dtype="<f4")
    refuse("nonfinite-ignore", pair("nonfinite-ignore-src", floats, 4, extra="data ignore value = NaN\n"),
           "Invalid ENVI data ignore value")

    def validity(label, cli, expected, mask, *options):
        target = root / f"{label}.npy"
        report = json.loads(run([*cli, target, "--with-validity", *options]).stdout)
        structured = np.load(target, allow_pickle=False)
        assert structured["values"].dtype == expected.dtype
        np.testing.assert_array_equal(structured["valid"], mask)
        assert structured["values"].tobytes() == expected.tobytes()
        assert report["valid_pixels"] == np.count_nonzero(mask)
        rows["validity"].append(label)
        return target

    mask_values = np.array([[[-9999, 11], [21, -9999], [31, 32]]], dtype="<i2")
    masked = inputs / "masked.hdr"
    if not masked.is_file():
        masked = pair("masked", mask_values, 2, "bil", 1, extra="data ignore value = -9999\n")
    masked_cli = base + ["--raw", masked]
    masked_array = validity("mask-both-bands", masked_cli, np.array([[[0, 0], [0, 0], [31, 32]]], dtype="<i2"),
                            [[False, False, True]], "--channels", "2")
    validity("mask-band-choice-and-edge", masked_cli, np.array([[[0], [11], [0], [32]]], dtype="<i2"),
             [[False, True, False, True]], "--channel", "1", "--x", "-1", "--width", "4")
    refuse("mask-plain-array", masked, "requires complete valid samples", "--channels", "2")
    display("ignore-first-band", masked, np.array([[0, 128, 128]]), np.array([[0, 255, 255]]))

    color = np.array([[[17, 31, 223, 53], [19, 37, 227, 59]]], dtype="u1")
    rgb = pair("default-rgb", color, 1, "bsq", extra="default bands = {3,\n 1, 4}\n")
    rgba = np.array([[[223, 17, 53, 255], [227, 19, 59, 255]]], dtype="u1")
    expected_rgb = render("rgb-order", rgb, rgba)
    gray = pair("default-gray", color, 1, "bil", extra="default bands = {2}\n")
    display("selected-grayscale", gray, color[:, :, 1], np.full((1, 2), 255))
    raw("rgb-keeps-native-order", rgb, color, [f"Band {number}" for number in range(1, 5)], "--channels", "4")
    masked_rgb = pair("default-rgb-masked", color, 1,
                      extra="default bands = {3, 1, 4}\ndata ignore value = 19\n")
    render("rgb-selected-ignore", masked_rgb, np.array([[[223, 17, 53, 255], [0, 0, 0, 0]]], dtype="u1"))
    raw("rgb-ignore-outside-raw-selection", masked_rgb, color[:, :, 1:],
        [f"Band {number}" for number in range(1, 5)], "--channel", "1", "--channels", "3")
    float_rgb = pair("default-rgb-float", np.array([[[0.5, np.nan, 1.0], [0.25, 0, 0.75]]], dtype="<f4"),
                     4, "bil", 1, extra="default bands = {3, 1, 2}\n")
    render("rgb-selected-nonfinite", float_rgb, np.array([[[0, 0, 0, 0], [191, 64, 0, 255]]], dtype="u1"))
    for label, value, reason in (
        ("empty", "{}", "Empty ENVI default band"),
        ("zero", "{0}", "default band is outside"),
        ("out-of-range", "{1, 5, 3}", "default band is outside"),
        ("two", "{1, 2}", "requires one or three"),
        ("four", "{1, 2, 3, 4}", "requires one or three"),
        ("trailing", "{1, 2, 3,}", "requires one or three"),
        ("unbraced", "1, 2, 3", "requires braces"),
    ):
        refuse("default-bands-" + label, pair("invalid-bands-" + label, color, 1,
               extra=f"default bands = {value}\n"), reason)

    float_mask = pair("float-mask", floats, 4, "bsq", 1, extra="data ignore value = +1e-1\n")
    validity("float-mask-typed-value", base + ["--raw", float_mask], np.array([[[0], [0.5], [1]]], dtype="<f4"),
             [[False, True, True]])

    wide = {}
    for label, code, dtype, samples, sentinel, opposite_order, gray_values in (
        ("signed64", 14, "<i8", [[-(2**63), 7], [-(2**53 + 1), 8], [-(2**53), 9], [2**63 - 1, -1]],
         -(2**53 + 1), 0, [0, 0, 127, 255]),
        ("unsigned64", 15, "<u8", [[0, 1], [2**64 - 1, 2], [2**64 - 2, 3], [2**53 + 1, 4]],
         2**64 - 1, 1, [0, 0, 255, 0]),
    ):
        values = np.array([samples], dtype=dtype)
        header = inputs / (label + ".hdr")
        if not header.is_file():
            header = pair(label, values, code, "bil", 1 - opposite_order,
                          extra=f"data ignore value = {sentinel}\n")
        expected = values.copy()
        expected[0, 1, :] = 0
        array = validity(label + "-exact-mask", base + ["--raw", header], expected,
                         [[True, False, True, True]], "--channels", "2")
        wide[label] = (header, array)
        raw(label + "-other-channel", header, values[:, :, 1:], ["Band 1", "Band 2"], "--channel", "1")
        alternate = pair(label + "-opposite", values, code, "bip", opposite_order)
        raw(label + "-opposite-byte-order", alternate, values, ["Band 1", "Band 2"], "--channels", "2")
        display(label + "-display", header, np.array([gray_values]), np.array([[255, 0, 255, 255]]))
        refuse(label + "-plain-mask", header, "requires complete valid samples", "--channels", "2")
        overflow = 2**63 if code == 14 else 2**64
        malformed = pair(label + "-overflow", values, code, extra=f"data ignore value = {overflow}\n")
        refuse(label + "-ignore-overflow", malformed, "Invalid ENVI data ignore value")

    refuse("complex", pair("complex-src", scalar, 6), "outside the real raster profile")
    refuse("band-label-count", pair("labels", scalar, 12, extra="band names = {one, two}\n"), "band-name count differs")
    refuse("too-many-bands", pair("band-bound", scalar, 12, bands=257), "Invalid ENVI dimensions")
    refuse("out-of-range-channel", inputs / "bil.hdr", "channel", "--channel", "3")
    refuse("clipped-array", inputs / "bsq.hdr", "requires complete valid samples",
           "--channel", "1", "--channels", "2", "--x", "-1", "--y", "0", "--width", "3", "--height", "2")
    for label, payload, reason in (("short", b"\x00", "wrong extent"), ("trailing", bytes(13), "wrong extent"),
                                   ("absent", None, "companion is missing")):
        header = pair("source-" + label, scalar, 12)
        companion = header.with_suffix(".img")
        if payload is None:
            companion.unlink()
        else:
            companion.write_bytes(payload)
        refuse(label, header, reason)
    alias = pair("aliased-source", scalar, 12)
    alias.with_suffix(".img").unlink()
    os.link(alias, alias.with_suffix(".img"))
    refuse("aliased", alias, "distinct regular file")

    study = root / "saved study"
    study.mkdir()
    for suffix in (".hdr", ".img"):
        shutil.copyfile(inputs / ("bil" + suffix), study / ("study" + suffix))
    for mode in ("create", "reopen", "changed"):
        if mode == "changed":
            raster = study / "study.img"
            replacement = study / "replacement.img"
            replacement.write_bytes(raster.read_bytes())
            replacement.replace(raster)
        result = run([args.probe, mode, args.manifest, study])
        rows["project"].append(result.stdout.strip())

    if args.format_cli:
        cli = [args.format_cli, "--reader-plugins", args.manifest]
        for suffix, interleave in (("raw", "bil"), ("bil", "bil"), ("bip", "bip"), ("bsq", "bsq")):
            label = "named-" + suffix
            header = inputs / (label + ".hdr")
            raster = header.with_suffix("." + suffix)
            if not header.is_file():
                pair(label, cube, 12, interleave, 1,
                     extra="band names = {Blue band, Green band, Red band}\n")
                header.with_suffix(".img").rename(raster)
            decoy = header.with_suffix(".img")
            decoy.write_bytes(bytes(raster.stat().st_size))
            flags = ["--reader-options", json.dumps({"companion_layout": suffix})]
            direct = raw(label, header, cube, names, "--channels", "3", companion_path=raster, reader_options=flags)
            raw(label + "-default", header, np.zeros_like(cube), names, "--channels", "3")
            moved = raster.with_suffix(".offline")
            raster.rename(moved)
            try:
                refuse(label + "-missing", header, "Selected ENVI companion is missing", reader_options=flags)
            finally:
                moved.rename(raster)

            project = root / (label + ".qpxproj")
            result = run(cli + ["project-create", header, project, "--reader", item["provider_id"], *flags])
            assert json.loads(result.stdout)["image_id"] == 1
            project_hash = sha(project)
            decoy.write_bytes(b"\xff" * decoy.stat().st_size)
            replay = root / (label + "-replay.npy")
            result = run(base[:3] + ["--raw-project", project, "1", replay, "--channels", "3"])
            report = json.loads(result.stdout)
            observed, = report["reader_snapshot"]["observed"]["companions"]
            assert Path(observed["path"]).samefile(raster)
            assert sha(direct) == sha(replay) and report["project"]["project_sha256"] == project_hash
            review = cli + ["project-review-source", project, "1", header]
            assert not json.loads(run(review).stdout)["requires_acceptance"]
            replacement = raster.with_suffix(".copy")
            shutil.copyfile(raster, replacement)
            replacement.replace(raster)
            revised = json.loads(run(review).stdout)
            assert revised["comparison"] == "changed_dependencies" and revised["requires_acceptance"]
            assert sha(project) == project_hash
            rows["project"].append(f"{suffix} selection replayed after decoy edit and detected companion replacement")

        for label, (header, array) in wide.items():
            saved = root / (label + ".qpxproj")
            create = [args.format_cli, "--reader-plugins", args.manifest, "project-create",
                      header, saved, "--reader", item["provider_id"]]
            assert json.loads(run(create).stdout)["image_id"] == 1
            before = sha(saved)
            replay = root / (label + "-project.npy")
            report = run([args.region, "--reader-plugins", args.manifest, "--raw-project", saved, "1",
                          replay, "--channels", "2", "--with-validity"])
            assert sha(replay) == sha(array) and json.loads(report.stdout)["project"]["project_sha256"] == before
            assert sha(saved) == before
            rows["project"].append("bit-exact " + label + " project replay")
        project_rgb = root / "rgb-project.qpxproj"
        create_rgb = [args.format_cli, "--reader-plugins", args.manifest, "project-create",
                      rgb, project_rgb, "--reader", item["provider_id"]]
        assert json.loads(run(create_rgb).stdout)["image_id"] == 1
        project_before = sha(project_rgb)
        pam = root / "rgb-project.pam"
        report = run([args.region, "--reader-plugins", args.manifest, "--project-region", project_rgb,
                      "1", "0", "0", "0", "2", "1", pam, "--receipt"])
        assert sha(pam) == sha(expected_rgb) == json.loads(report.stdout)["sha256"]
        assert sha(project_rgb) == project_before
        rows["project"].append("reopened default RGB mapping matches direct display")
        for layout, header_name, raster_name in (
            ("bin", "binary.hdr", "binary.bin"),
            ("dat", "data.hdr", "data.dat"),
            ("appended", "field scan.bil.hdr", "field scan.bil"),
        ):
            header = inputs / header_name
            raster = inputs / raster_name
            shutil.copyfile(inputs / "bil.hdr", header)
            shutil.copyfile(inputs / "bil.img", raster)
            header.with_suffix(".img").write_bytes(bytes(raster.stat().st_size))
            selection = json.dumps({"companion_layout": layout})
            flags = ["--reader-options", selection]
            raw("companion-" + layout, header, cube, names, "--channels", "3", companion_path=raster,
                reader_options=flags)
            raw("default-" + layout, header, np.zeros_like(cube), names, "--channels", "3")

        header = inputs / "field scan.bil.hdr"
        raster = inputs / "field scan.bil"
        flags = ["--reader-options", '{"companion_layout":"appended"}']
        refuse("wrong-selection", header, "Selected ENVI companion is missing",
               reader_options=["--reader-options", '{"companion_layout":"dat"}'])
        refuse("unknown-selection", header, "Unknown ENVI companion_layout",
               reader_options=["--reader-options", '{"companion_layout":"../outside"}'])

        masked_project = root / "masked.qpx"
        input_hashes = [sha(path) for path in (masked, masked.with_suffix(".img"))]
        masked_id = json.loads(run(cli + ["project-create", masked, masked_project,
                                          "--reader", item["provider_id"]]).stdout)["image_id"]
        saved_hash = sha(masked_project)
        saved_array = root / "masked-project.npy"
        receipt = json.loads(run(base[:3] + ["--raw-project", masked_project, str(masked_id), saved_array,
                                             "--with-validity", "--channels", "2"]).stdout)
        assert receipt["valid_pixels"] == 1 and receipt["project"]["project_sha256"] == saved_hash
        assert saved_array.read_bytes() == masked_array.read_bytes() and sha(masked_project) == saved_hash
        assert input_hashes == [sha(path) for path in (masked, masked.with_suffix(".img"))]
        rows["project"].append("saved ignore value exports identical structured samples")

        project = root / "appended companion.qpx"
        result = json.loads(run(cli + ["project-create", header, project, "--reader", item["provider_id"],
                                      *flags]).stdout)
        assert result["image_id"] == 1
        protected = sha(project)
        review = cli + ["project-review-source", project, "1", header]
        before = json.loads(run(review).stdout)
        observed, = before["companions"]
        assert Path(observed["path"]).samefile(raster)
        assert before["coverage"] == "compound_primary_only" and not before["requires_acceptance"]
        after_replacement = raster.with_suffix(".copy")
        shutil.copyfile(raster, after_replacement)
        after_replacement.replace(raster)
        after = json.loads(run(review).stdout)
        assert after["comparison"] == "changed_dependencies" and after["requires_acceptance"]
        assert before["companions"] != after["companions"] and sha(project) == protected
        rows["project"].extend(["explicit appended source created and reviewed", "companion replacement detected"])

    assert identity == (sha(module), sha(args.manifest))
    (root / "validation.json").write_text(json.dumps({
        "passed": True, "module_sha256": identity[0], "manifest_sha256": identity[1],
        "baseline_sha256": sha(args.baseline), "host_sha256": sha(args.region), "sdk_probe_sha256": sha(args.probe),
        **({"project_cli_sha256": sha(args.format_cli)} if args.format_cli else {}),
        **rows,
    }, indent=2) + "\n", encoding="utf-8")
    print({name: len(values) for name, values in rows.items()})


if __name__ == "__main__":
    main()
