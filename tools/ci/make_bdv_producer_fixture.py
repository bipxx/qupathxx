# SPDX-License-Identifier: AGPL-3.0-only
"""Make a synthetic BigDataViewer project with the pinned pybdv writer."""

import argparse
import hashlib
from importlib.metadata import version
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import numpy as np
from pybdv import make_bdv
from pybdv.metadata import get_affine
from pybdv.util import get_key, open_file


SHARED_VIEW_ROLES = {
    "tile": {"id": 4, "name": "Shared field", "location": [0, 0, 0]},
    "angle": {"id": 6, "name": "Zero angle", "axis": [0, 1, 0], "degrees": 0},
    "illumination": {"id": 8, "name": "Shared illumination"},
}


def partition_hdf5(project, setups, missing):
    """Repackage pybdv pixels using the upstream partition writer's layout."""
    import h5py

    data = project / "dataset.h5"
    original = data.rename(project.parent / "unpartitioned-oracle.h5")
    xml = project / "dataset.xml"
    tree = ET.parse(xml)
    loader = tree.find("./SequenceDescription/ImageLoader")
    partition_dir = project / "parts"
    partition_dir.mkdir()
    with h5py.File(original, "r") as source, h5py.File(data, "x") as master:
        for setup, _, _ in setups:
            key = f"s{setup:02d}"
            source.copy(key, master)
            master[key].attrs["dataType"] = np.bytes_("uint16")
        groups = [(0, setups), *[(1, [setup]) for setup in setups if (setup[0], 1) not in missing]]
        for index, (time, group) in enumerate(groups):
            name = f"parts/part-{index}.h5"
            local_time = 11 - time
            local_setups = [21 - setup for setup, _, _ in group]
            partition = ET.SubElement(loader, "partition")
            ET.SubElement(partition, "path", type="relative").text = name
            for tag, ids in (("timepoints", [time]), ("timepointsMapped", [local_time]),
                             ("setups", [setup for setup, _, _ in group]), ("setupsMapped", local_setups)):
                ET.SubElement(partition, tag).text = " ".join(map(str, ids))
            with h5py.File(project / name, "x") as part:
                for (setup, _, _), local_setup in zip(group, local_setups):
                    source.copy(f"s{setup:02d}", part, name=f"s{local_setup:02d}")
                    part[f"s{local_setup:02d}"].attrs["dataType"] = np.bytes_("uint16")
                    for level in range(2):
                        key = get_key(True, timepoint=time, setup_id=setup, scale=level)
                        local_key = get_key(True, timepoint=local_time, setup_id=local_setup, scale=level)
                        cell = source[key]
                        samples = cell[:].view(np.dtype("u2").newbyteorder(cell.dtype.byteorder))
                        file_type = ">u2" if setup == setups[0][0] else "<u2"
                        part.create_dataset(local_key, data=samples, dtype=file_type,
                                            chunks=cell.chunks, compression="gzip")
                        object_name = local_key.lstrip("/") if level else "/" + local_key.lstrip("/")
                        master[key] = h5py.ExternalLink(name, object_name)
    ET.indent(tree)
    tree.write(xml, encoding="utf-8", xml_declaration=True)


def declare_missing_views(xml, missing):
    tree = ET.parse(xml)
    sequence = tree.find("./SequenceDescription")
    missing_node = ET.SubElement(sequence, "MissingViews")
    for setup, time in sorted(missing):
        ET.SubElement(missing_node, "MissingView", setup=str(setup), timepoint=str(time))
    ET.indent(tree)
    tree.write(xml, encoding="utf-8", xml_declaration=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="New fixture directory")
    parser.add_argument("--backend", choices=("n5", "h5"), default="n5")
    parser.add_argument("--multiple-setups", action="store_true", help="Write two co-registered channel setups")
    parser.add_argument("--shared-view-roles", action="store_true", help="Write common tile, angle and illumination")
    parser.add_argument("--partitioned", action="store_true", help="Write explicit HDF5 partition maps and master links")
    parser.add_argument("--missing-view", action="store_true", help="Omit the first channel at the second timepoint")
    parser.add_argument("--shifted", action="store_true", help="Translate the second view in X")
    args = parser.parse_args()
    if args.partitioned and args.backend != "h5":
        parser.error("Partitioned fixtures require the HDF5 backend")
    if args.shared_view_roles and not args.multiple_setups:
        parser.error("Shared view roles require multiple setups")
    if args.missing_view and not args.multiple_setups:
        parser.error("The missing-view fixture requires multiple setups")
    versions = {name: version(name) for name in ("pybdv", "z5py", "h5py", "numpy")}
    if versions["pybdv"] != "0.6.0" or versions["z5py"] != "3.0.2" or versions["h5py"] != "3.16.0":
        parser.error("Use pybdv 0.6.0, z5py 3.0.2 and h5py 3.16.0")
    root = args.output.resolve()
    root.mkdir(parents=True)
    project = root / "project"
    project.mkdir()
    data = project / ("dataset." + args.backend)
    xml = project / "dataset.xml"
    z, y, x = np.indices((3, 8, 12), dtype=np.uint16)
    setups = [(7, "Marker", 1), (2, "Nuclei", 9)] if args.multiple_setups else [(0, "Cells", 0)]
    missing = {(2, 1)} if args.missing_view else set()
    originals = {}
    for index, (setup, name, channel) in enumerate(setups):
        attributes = {"channel": {"id": channel}}
        if args.shared_view_roles:
            attributes.update({key: value.copy() for key, value in SHARED_VIEW_ROLES.items()})
        for t in range(2):
            if (setup, t) in missing:
                continue
            pixels = (t * 15000 + z * 5000 + y * 300 + x * 3 + 32768 + index * 2100).astype("<u2")
            originals[setup, t] = pixels
            transform = None
            if t == 1 and index == len(setups) - 1 and args.shifted:
                transform = [0.25, 0., 0., 6., 0., 0.5, 0., 0., 0., 0., 1.5, 0.]
            make_bdv(pixels, str(data), downscale_factors=[[1, 2, 2]], downscale_mode="mean",
                     resolution=[1.5, 0.5, 0.25], unit="micrometer", setup_id=setup,
                     setup_name=name, timepoint=t, affine=transform, chunks=[2, 3, 4], n_threads=1,
                     attributes=attributes)

    order = sorted(setups)
    if args.partitioned:
        partition_hdf5(project, order, missing)
    if missing:
        declare_missing_views(xml, missing)
    with open_file(str(data), "r") as source:
        for level in range(2):
            arrays = []
            for t in range(2):
                channels = []
                for setup, _, _ in order:
                    key = get_key(args.backend == "h5", timepoint=t, setup_id=setup, scale=level)
                    if (setup, t) in missing:
                        assert key not in source
                        shape = (3, 8, 12) if level == 0 else (3, 4, 6)
                        channels.append(np.full(shape, 65535, dtype="<u2"))
                        continue
                    raw = np.asarray(source[key][:])
                    if args.backend == "h5" and not args.partitioned:
                        assert raw.dtype == np.int16
                        samples = raw.view(np.dtype("u2").newbyteorder(raw.dtype.byteorder))
                    else:
                        assert raw.dtype.kind == "u" and raw.dtype.itemsize == 2
                        samples = raw
                    if level == 0:
                        np.testing.assert_array_equal(samples, originals[setup, t])
                    if level == 1:
                        assert samples.shape == (3, 4, 6)
                    channels.append(samples.astype("<u2"))
                arrays.append(np.stack(channels) if args.multiple_setups else channels[0])
            np.save(root / f"expected-level-{level}.npy", np.stack(arrays), allow_pickle=False)

    image_loader = ET.parse(xml).find("./SequenceDescription/ImageLoader")
    assert image_loader is not None
    registrations = {str(setup): [None if (setup, t) in missing else get_affine(str(xml), setup, timepoint=t) for t in range(2)]
                     for setup, _, _ in order}
    receipt = {"versions": versions, "backend": args.backend, "loader": image_loader.attrib,
               "registrations_xyz": registrations if args.multiple_setups else registrations["0"],
               "members": {path.relative_to(project).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                           for path in sorted(project.rglob("*")) if path.is_file()}}
    if args.multiple_setups:
        receipt["setups"] = [{"id": setup, "name": name, "channel_id": channel}
                             for setup, name, channel in order]
    if args.partitioned:
        receipt["partitioned"] = True
    if args.shared_view_roles:
        receipt["shared_view_roles"] = SHARED_VIEW_ROLES
    if missing:
        receipt["missing_views"] = [{"setup": setup, "timepoint": time} for setup, time in sorted(missing)]
    (root / "producer.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
