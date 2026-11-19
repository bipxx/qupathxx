"""Generate identity-geometry MetaImages with SimpleITK 2.5.3."""
import argparse
from pathlib import Path
import zlib

import numpy as np
import SimpleITK as sitk


def wide_samples(shape, vector):
    cases = (
        ("i64", np.int64, [-(1 << 63), -(1 << 53) - 1, -1, 0, 1, (1 << 53) + 1, (1 << 63) - 1]),
        ("u64", np.uint64, [0, 1, (1 << 53) + 1, (1 << 63) - 1, 1 << 63, (1 << 64) - 2, (1 << 64) - 1]),
    )
    target = (*shape, 4) if vector else shape
    return [(name, np.resize(np.array(values, dtype=dtype), target)) for name, dtype, values in cases]


def write_slices(output, compressed, numbered, spaced=False):
    samples = np.arange(12, dtype="<u2").reshape(2, 2, 3)
    image = sitk.GetImageFromArray(samples)
    image.SetSpacing((0.0005, 0.0015, 0.003))
    header = output / "study.mhd"
    sitk.WriteImage(image, str(header), False)
    text = header.read_text(encoding="ascii").split("ElementDataFile = ", 1)[0]
    if compressed:
        text = text.replace("CompressedData = False", "CompressedData = True")
    extension = ".zraw" if compressed else ".raw"
    if numbered and spaced:
        pattern = "scan %03d raw" + extension
        locator = f"{pattern} 2 5 3\n"
        names = [pattern % index for index in (2, 5)]
    elif numbered:
        pattern = "slice%03d" + extension
        locator = f"{pattern} -3 3 5\n"
        names = [pattern % index for index in (-3, 2)]
    else:
        stems = ("last section", "first section") if spaced else ("tail", "head")
        names = [stem + extension for stem in stems]
        locator = "LIST 2D\n" + "\n".join(names) + "\n"
    for name, plane in zip(names, samples):
        data = plane.tobytes()
        (output / name).write_bytes(zlib.compress(data) if compressed else data)
    header.write_text(text + "ElementDataFile = " + locator, encoding="ascii")
    reopened = sitk.ReadImage(str(header))
    assert reopened.GetSpacing() == image.GetSpacing() and reopened.GetDirection() == image.GetDirection()
    assert np.array_equal(sitk.GetArrayFromImage(reopened), samples)


def write_spaced(output, compressed):
    for mode in ("listed", "numbered", "detached"):
        (output / mode).mkdir()
    write_slices(output / "listed", compressed, numbered=False, spaced=True)
    write_slices(output / "numbered", compressed, numbered=True, spaced=True)
    samples = np.arange(12, dtype="<u2").reshape(2, 2, 3)
    image = sitk.GetImageFromArray(samples)
    image.SetSpacing((0.0005, 0.0015, 0.003))
    header = output / "detached/study data.mhd"
    sitk.WriteImage(image, str(header), compressed)
    extension = ".zraw" if compressed else ".raw"
    assert f"ElementDataFile = study data{extension}\n" in header.read_text(encoding="ascii")
    actual = sitk.ReadImage(str(header))
    assert actual.GetSpacing() == image.GetSpacing()
    assert np.array_equal(sitk.GetArrayFromImage(actual), samples)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--compressed", action="store_true")
    parser.add_argument("--vector", action="store_true")
    parser.add_argument("--wide-integers", action="store_true")
    slices = parser.add_mutually_exclusive_group()
    slices.add_argument("--slice-list", action="store_true")
    slices.add_argument("--numbered-slices", action="store_true")
    slices.add_argument("--spaced-filenames", action="store_true")
    args = parser.parse_args()
    if sitk.Version_VersionString() != "2.5.3":
        raise ValueError("Generate these fixtures with SimpleITK 2.5.3")
    args.output.mkdir()
    if args.slice_list or args.numbered_slices or args.spaced_filenames:
        if args.vector or args.wide_integers:
            parser.error("The slice fixtures have their own fixed oracle")
        if args.spaced_filenames:
            write_spaced(args.output, args.compressed)
        else:
            write_slices(args.output, args.compressed, args.numbered_slices)
        return
    for shape in ((2, 3), (2, 2, 3)):
        samples = np.arange(np.prod(shape), dtype=np.uint16).reshape(shape)
        if args.vector:
            samples = samples[..., None] * 257 + np.array([0, 8192, 32768, 60000], dtype=np.uint16)
        cases = wide_samples(shape, args.vector) if args.wide_integers else [("producer", samples)]
        for name, values in cases:
            image = sitk.GetImageFromArray(values, isVector=args.vector)
            image.SetSpacing((0.0005, 0.0015, 0.003)[:len(shape)])
            for suffix in ("mha", "mhd"):
                path = str(args.output / f"{name}-{len(shape)}d.{suffix}")
                sitk.WriteImage(image, path, args.compressed)
                round_trip = sitk.ReadImage(path)
                assert round_trip.GetSize() == image.GetSize()
                assert round_trip.GetSpacing() == image.GetSpacing()
                assert round_trip.GetDirection() == image.GetDirection()
                assert round_trip.GetOrigin() == image.GetOrigin()
                assert round_trip.GetNumberOfComponentsPerPixel() == (4 if args.vector else 1)
                assert np.array_equal(sitk.GetArrayFromImage(round_trip), values)


if __name__ == "__main__":
    main()
