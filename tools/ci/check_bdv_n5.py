# SPDX-License-Identifier: AGPL-3.0-only
"""Compare the native BDV N5 provider with a pinned pybdv project and its arrays."""

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

from check_ngff05 import npy, sha, verify_inventory


LEVELS = [{"width": 12, "height": 8, "downsample": 1},
          {"width": 6, "height": 4, "downsample": 2}]
TIME_COUNT = 2
Z_COUNT = 3
IMAGE = {"width": 12, "height": 8, "levels": LEVELS, "pixel_width_um": 0.25, "pixel_height_um": 0.5}
RAW = {**IMAGE, "channels": 1, "sample_type": "uint16", "z_planes": Z_COUNT, "timepoints": TIME_COUNT,
       "series": 0, "series_count": 1, "dimension_order": "XYZT", "z_spacing_um": 1.5,
       "time_interval_seconds": None}
SETUP = "dataset.n5/setup0"
FIRST_VIEW = f"{SETUP}/timepoint0"
BLOCK = f"{FIRST_VIEW}/s0/1/0/0"
AFFINE = "./ViewRegistrations/ViewRegistration[@timepoint='1']/ViewTransform/affine"


def read_fixture(fixture, receipt, backend):
    assert receipt["versions"] == {"pybdv": "0.6.0", "z5py": "3.0.2", "h5py": "3.16.0", "numpy": "2.5.5"}
    assert receipt["backend"] == ("h5" if backend == "hdf5" else "n5")
    assert receipt["loader"] == {"format": f"bdv.{backend}"}
    assert receipt["registrations_xyz"] == [{"affine0": [
        0.25, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1.5, 0]}] * TIME_COUNT
    project = fixture / "project"
    verify_inventory(project, receipt["members"])
    return project, [npy(fixture / f"expected-level-{index}.npy",
                         (TIME_COUNT, Z_COUNT, level["height"], level["width"]))
                     for index, level in enumerate(LEVELS)]


def crop(arrays, level, time, z, x, y, width, height):
    shape = LEVELS[level]
    stride = shape["width"]
    base = (time * Z_COUNT + z) * shape["height"] * stride
    x //= shape["downsample"]
    y //= shape["downsample"]
    return tuple(arrays[level][base + yy * stride + xx]
                 for yy in range(y, y + height) for xx in range(x, x + width))


def main(*, backend="n5", describe=None, add_backend_options=None, check_backend=None):
    parser = argparse.ArgumentParser(description=describe or __doc__)
    for option in ("region", "manifest", "fixture", "output"):
        parser.add_argument(f"--{option}", type=Path, required=True)
    parser.add_argument("--shifted-fixture", type=Path, help="Producer fixture with translated second view")
    parser.add_argument("--contract-version", type=int, choices=(1, 2, 3, 4, 5), default=1)
    if add_backend_options:
        add_backend_options(parser)
    args = parser.parse_args()
    fixture = args.fixture.resolve(strict=True)
    shifted = args.shifted_fixture.resolve(strict=True) if args.shifted_fixture else None
    region = args.region.resolve(strict=True)
    manifest = args.manifest.resolve(strict=True)
    output = args.output.resolve()
    if output.is_relative_to(fixture) or (shifted and output.is_relative_to(shifted)):
        parser.error("Output must be separate from the original fixture")
    output.mkdir(parents=True)
    receipt_path = fixture / "producer.json"
    receipt = json.loads(receipt_path.read_bytes())
    project, arrays = read_fixture(fixture, receipt, backend)
    if backend == "hdf5":
        assert all(any(value > 32767 for value in level) for level in arrays)
    provider_id = f"org.qupathxx.bdv-{backend}"
    contract = f"bdv-{backend}-uint16/{args.contract_version}"
    entry, = (item for item in json.loads(manifest.read_bytes())["plugins"] if item["provider_id"] == provider_id)
    module = manifest.parent / entry["module"]["path"]
    assert module.is_file() and sha(module) == entry["module"]["sha256"]

    prefix = [region, "--reader-plugins", manifest]
    reader = ["--reader", provider_id]
    commands, identities = [], {}

    def run(name, command, status=0):
        argv = [str(value) for value in command]
        result = subprocess.run(argv, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
        commands.append({"name": name, "argv": argv, "exit_code": result.returncode,
                         "stdout": result.stdout, "stderr": result.stderr})
        assert result.returncode == status, (name, status, result.returncode, result.stdout, result.stderr)
        if status:
            assert result.stderr.strip(), name
        return result.stdout

    def raw(name, source=project, *, time=0, c=0, z=0, level=0, x=0, y=0,
            width=1, height=1, status=0, pixels=None):
        dest = output / f"{name}.npy"
        result = run(name, [*prefix, *reader, "--raw", source, dest, "--level", level, "--time", time,
                            "--channel", c, "--z", z, "--x", x, "--y", y, "--width", width,
                            "--height", height, "--channels", 1], status)
        if status:
            assert not dest.exists(), name
            return
        info = json.loads(result)
        expected = {"schema": "qpx.raw-region/2", "reader": provider_id, "dtype": "uint16", "level": level,
                    "time": time, "first_channel": c, "z": z, "x": x, "y": y, "width": width,
                    "height": height, "channels": 1, "valid_pixels": width * height}
        assert all(info[key] == value for key, value in expected.items()), (name, info)
        snapshot = info["reader_snapshot"]
        implementation = snapshot["implementation"]
        assert snapshot["schema"] == "qpx.reader-plugin-link/2"
        assert implementation["provider_id"] == provider_id and implementation["contract"] == contract
        assert implementation["module_sha256"] == entry["module"]["sha256"]
        assert implementation["capabilities"] == 11  # RGBA | RAW | SOURCE_STORE
        observed = snapshot["observed"]
        assert observed["scope"] == "directory_store" and Path(observed["locator"]) == source
        assert Path(observed["primary"]["path"]) == source / "dataset.xml"
        digest = observed["directory_sha256"]
        assert len(digest) == 64 and len(bytes.fromhex(digest)) == 32
        assert identities.setdefault(source, digest) == digest
        assert all(snapshot["metadata"][key] == value for key, value in IMAGE.items()), snapshot["metadata"]
        assert all(snapshot["raw"][key] == value for key, value in RAW.items()), snapshot["raw"]
        if pixels is None:
            pixels = crop(arrays, level, time, z, x, y, width, height)
        assert npy(dest, (height, width, 1)) == pixels, name

    def variant(name):
        dest = output / "variants" / name / "project"
        shutil.copytree(project, dest)
        return dest

    def bad_xml(name, edit):
        dest = variant(name)
        path = dest / "dataset.xml"
        tree = ET.parse(path)
        edit(tree)
        tree.write(path, encoding="utf-8")
        raw(name, dest, status=2)

    def bad_attributes(name, key, edit):
        dest = variant(name)
        path = dest / key / "attributes.json"
        data = json.loads(path.read_bytes())
        edit(data)
        path.write_text(json.dumps(data), encoding="utf-8")
        raw(name, dest, status=2)

    passed = False
    try:
        catalog = json.loads(run("catalog-with-plugin", [*prefix, "--readers"]))
        provider, = (item for item in catalog["readers"] if item["provider_id"] == provider_id)
        assert provider["available"] and provider["display_rgba"] and provider["raw_samples"]
        assert provider["contract"] == contract and provider["module_sha256"] == entry["module"]["sha256"]
        for level, shape in enumerate(LEVELS):
            for time in range(TIME_COUNT):
                for z in range(Z_COUNT):
                    raw(f"level-{level}-t{time}-z{z}", time=time, z=z, level=level,
                        width=shape["width"], height=shape["height"])
        raw("level-zero-chunk-seams", time=1, z=1, x=3, y=2, width=7, height=5)
        raw("level-one-odd-origin-crossing-seams", time=1, z=2, level=1, x=7, y=5, width=3, height=2)
        raw("level-one-last-pixel", time=1, z=2, level=1, x=11, y=7)

        if backend == "n5":
            missing = variant("sparse-n5-block")
            (missing / BLOCK).unlink()
            raw("sparse-n5-block", missing, x=4, width=4, height=3, pixels=(0,) * 12)
            assert identities[missing] != identities[project]

        for level, x, y, width, height in ((0, 3, 2, 4, 3), (1, 7, 5, 3, 2)):
            pam = output / f"display-level-{level}.pam"
            run(f"display-level-{level}", [*prefix, *reader, project, x, y, level, width, height, pam])
            header, encoded = pam.read_bytes().split(b"ENDHDR\n", 1)
            assert f"WIDTH {width}\nHEIGHT {height}\nDEPTH 4\n".encode() in header
            gray = ((v * 255 + 32767) // 65535 for v in crop(arrays, level, 0, 0, x, y, width, height))
            assert encoded == bytes(component for value in gray for component in (value, value, value, 255))

        for name, edit in (
            ("xml-path-escape", lambda tree: setattr(tree.find(f"./SequenceDescription/ImageLoader/{backend}"),
                                                    "text", "../" + ("dataset.h5" if backend == "hdf5" else "dataset.n5"))),
            ("missing-time-registration", lambda tree: tree.find("./ViewRegistrations").remove(
                tree.find("./ViewRegistrations/ViewRegistration[@timepoint='1']"))),
            ("xml-voxel-size-disagrees", lambda tree: setattr(tree.find(
                "./SequenceDescription/ViewSetups/ViewSetup/voxelSize/size"), "text", "0.5 0.5 1.5")),
        ):
            bad_xml(name, edit)
        if shifted:
            shifted_project = shifted / "project"
            shifted_receipt = json.loads((shifted / "producer.json").read_bytes())
            assert shifted_receipt["registrations_xyz"][1]["affine0"][3] == 6
            assert shifted_receipt["backend"] == receipt["backend"]
            verify_inventory(shifted_project, shifted_receipt["members"])
            raw("unsupported-registration", shifted_project, status=2)
            verify_inventory(shifted_project, shifted_receipt["members"])
        else:
            bad_xml("unsupported-registration", lambda tree: setattr(tree.find(AFFINE), "text",
                "0.25 0 0 6 0 0.5 0 0 0 0 1.5 0"))
        dtd = variant("xml-dtd")
        xml = dtd / "dataset.xml"
        xml.write_bytes(b'<!DOCTYPE SpimData SYSTEM "file:///missing-bdv.dtd">\n' + xml.read_bytes())
        raw("xml-dtd", dtd, status=2)

        if check_backend:
            check_backend(args, project, variant, raw, identities)
        else:
            check_n5_storage(raw, variant, bad_attributes)
        for name, coord in (("invalid-z", {"z": 3}), ("invalid-channel", {"c": 1}),
                            ("invalid-time", {"time": 2}), ("invalid-level", {"level": 2})):
            raw(name, status=2, **coord)

        protected = variant("protected-project")
        inside = protected / "output.npy"
        run("output-inside-source", [*prefix, *reader, "--raw", protected, inside], 2)
        assert not inside.exists()
        verify_inventory(protected, receipt["members"])
        verify_inventory(project, receipt["members"])
        passed = True
    finally:
        stamp = {"passed": passed, "fixture_sha256": sha(receipt_path), "region_sha256": sha(region),
                 "manifest_sha256": sha(manifest), "module_sha256": sha(module), "commands": commands}
        (output / "results.json").write_text(json.dumps(stamp, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(commands)} native BDV {backend.upper()} commands")


def check_n5_storage(raw, variant, bad_attributes):
    for name, key, edit in (
        ("unsupported-z-downsampling", SETUP, lambda data: data["downsamplingFactors"][1].__setitem__(2, 2)),
        ("pyramid-dimensions-disagree", f"{FIRST_VIEW}/s1", lambda data: data["dimensions"].__setitem__(0, 7)),
        ("pyramid-downsampling-disagrees", f"{FIRST_VIEW}/s1", lambda data: data["downsamplingFactors"].__setitem__(0, 3)),
        ("dtype-unadmitted", f"{FIRST_VIEW}/s0", lambda data: data.update(dataType="float32")),
    ):
        bad_attributes(name, key, edit)
    for name, edit in (("n5-block-mode", lambda data: b"\x00\x01" + data[2:]),
                       ("n5-block-truncated", lambda data: data[:-1])):
        dest = variant(name)
        block = dest / BLOCK
        block.write_bytes(edit(block.read_bytes()))
        raw(name, dest, x=4, status=2)


if __name__ == "__main__":
    main()
