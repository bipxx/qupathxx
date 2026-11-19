# SPDX-License-Identifier: AGPL-3.0-only
"""Verify native BDV imports of a pinned two-setup, two-timepoint pybdv project."""

import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

from check_bdv_n5 import IMAGE, LEVELS, RAW, TIME_COUNT, Z_COUNT
from check_ngff05 import npy, sha, verify_inventory


SETUP_IDS = (2, 7)
CHANNEL_NAMES = ["Nuclei", "Marker"]
CHANNEL_IDS = (9, 1)
CHANNEL_COUNT = len(SETUP_IDS)
SETUPS = "./SequenceDescription/ViewSetups"
REGISTRATIONS = "./ViewRegistrations"
IMAGE_LOADER = "./SequenceDescription/ImageLoader"
MISSING_VIEWS = "./SequenceDescription/MissingViews"
MISSING_PAIR = (2, 1)
MISSING_DIAGNOSTIC = "BigDataViewer view is declared missing"
RAW_METADATA = {**RAW, "channels": CHANNEL_COUNT, "channel_names": CHANNEL_NAMES, "dimension_order": "XYZCT"}


def setup_nodes(tree):
    return {int(node.findtext("id")): node for node in tree.findall(f"{SETUPS}/ViewSetup")}


def registrations(tree):
    return {(int(node.attrib["setup"]), int(node.attrib["timepoint"])): node
            for node in tree.findall(f"{REGISTRATIONS}/ViewRegistration")}


def fixture_inputs(fixture, backend, partitioned, shared_view_roles, missing_view):
    receipt_path = fixture / "producer.json"
    receipt = json.loads(receipt_path.read_bytes())
    assert receipt["versions"] == {"pybdv": "0.6.0", "z5py": "3.0.2", "h5py": "3.16.0", "numpy": "2.5.5"}
    assert receipt["backend"] == ("h5" if backend == "hdf5" else "n5")
    assert receipt["loader"] == {"format": f"bdv.{backend}"}
    assert receipt.get("partitioned", False) == partitioned
    assert bool(receipt.get("shared_view_roles")) == shared_view_roles
    assert receipt["setups"] == [{"id": setup, "name": name, "channel_id": channel}
                                 for setup, name, channel in zip(SETUP_IDS, CHANNEL_NAMES, CHANNEL_IDS)]
    project = fixture / "project"
    verify_inventory(project, receipt["members"])
    tree = ET.parse(project / "dataset.xml")
    nodes = setup_nodes(tree)
    assert sorted(nodes) == list(SETUP_IDS)
    assert [nodes[key].findtext("name") for key in SETUP_IDS] == CHANNEL_NAMES
    assert [nodes[key].findtext("attributes/channel") for key in SETUP_IDS] == list(map(str, CHANNEL_IDS))
    if shared_view_roles:
        roles = receipt["shared_view_roles"]
        assert {name: role["id"] for name, role in roles.items()} == {"tile": 4, "angle": 6, "illumination": 8}
        for name, role in roles.items():
            path = f"{SETUPS}/Attributes[@name='{name}']/{name.capitalize()}"
            definition = tree.find(path)
            for key, value in role.items():
                assert definition.findtext(key) == (" ".join(map(str, value)) if isinstance(value, list) else str(value))
            assert all(node.findtext(f"attributes/{name}") == str(role["id"]) for node in nodes.values())
    views = registrations(tree)
    expected_views = {(setup, t) for setup in SETUP_IDS for t in range(TIME_COUNT)}
    missing = {MISSING_PAIR} if missing_view else set()
    assert {(entry["setup"], entry["timepoint"]) for entry in receipt.get("missing_views", [])} == missing
    assert {(int(node.attrib["setup"]), int(node.attrib["timepoint"]))
            for node in tree.findall(f"{MISSING_VIEWS}/MissingView")} == missing
    assert set(views) == expected_views - missing
    assert all(tuple(map(float, view.findtext("ViewTransform/affine").split())) ==
               (0.25, 0, 0, 0, 0, 0.5, 0, 0, 0, 0, 1.5, 0) for view in views.values())
    arrays = [npy(fixture / f"expected-level-{level}.npy",
                  (TIME_COUNT, CHANNEL_COUNT, Z_COUNT, shape["height"], shape["width"]))
              for level, shape in enumerate(LEVELS)]
    return receipt, receipt_path, project, arrays


def crop(arrays, level, t, z, x, y, width, height, first=0, channels=CHANNEL_COUNT):
    geometry = LEVELS[level]
    x0, y0 = x // geometry["downsample"], y // geometry["downsample"]
    stride, rows = geometry["width"], geometry["height"]
    return tuple(arrays[level][(((t * CHANNEL_COUNT + c) * Z_COUNT + z) * rows + yy) * stride + xx]
                 for yy in range(y0, y0 + height) for xx in range(x0, x0 + width)
                 for c in range(first, first + channels))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=("n5", "hdf5"), required=True)
    parser.add_argument("--contract-version", type=int, choices=(2, 3, 4, 5), default=2)
    parser.add_argument("--partitioned", action="store_true")
    parser.add_argument("--partition-mutations", action="store_true")
    parser.add_argument("--shared-view-roles", action="store_true")
    parser.add_argument("--missing-view", action="store_true")
    for option in ("region", "manifest", "fixture", "output"):
        parser.add_argument(f"--{option}", type=Path, required=True)
    args = parser.parse_args()
    if (args.contract_version == 5 and args.backend != "hdf5") or \
            (args.partitioned and (args.backend != "hdf5" or args.contract_version < 3)) or \
            (args.partition_mutations and not args.partitioned):
        parser.error("Partitioned reads require HDF5 contract 3 or newer")
    shared_roles_revision = 4 if args.backend == "hdf5" else 3
    if args.shared_view_roles and args.contract_version < shared_roles_revision:
        parser.error("The selected contract must admit shared acquisition roles")
    if args.missing_view and args.contract_version <= shared_roles_revision:
        parser.error("The selected contract must admit missing views")
    fixture = args.fixture.resolve(strict=True)
    region = args.region.resolve(strict=True)
    manifest = args.manifest.resolve(strict=True)
    output = args.output.resolve()
    if output.is_relative_to(fixture):
        parser.error("Output must be separate from the producer fixture")
    output.mkdir(parents=True)
    receipt, receipt_path, project, arrays = fixture_inputs(
        fixture, args.backend, args.partitioned, args.shared_view_roles, args.missing_view)
    provider = f"org.qupathxx.bdv-{args.backend}"
    contract = f"bdv-{args.backend}-uint16/{args.contract_version}"
    entry, = (item for item in json.loads(manifest.read_bytes())["plugins"] if item["provider_id"] == provider)
    assert entry["contract"] == contract and entry["version"] == f"1.{args.contract_version - 1}.0"
    module = manifest.parent / entry["module"]["path"]
    assert sha(module) == entry["module"]["sha256"]
    prefix = [region, "--reader-plugins", manifest]
    reader = ["--reader", provider]
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

    def raw(name, source=project, *, t=0, first=0, z=0, level=0, x=0, y=0,
            width=1, height=1, channels=CHANNEL_COUNT, status=0, diagnostic=None):
        dest = output / f"{name}.npy"
        result = run(name, [*prefix, *reader, "--raw", source, dest, "--level", level, "--time", t,
                            "--channel", first, "--z", z, "--x", x, "--y", y, "--width", width,
                            "--height", height, "--channels", channels], status)
        if status:
            assert not dest.exists(), name
            if diagnostic:
                assert diagnostic in commands[-1]["stderr"], commands[-1]
            return
        info = json.loads(result)
        expected = {"schema": "qpx.raw-region/2", "reader": provider, "dtype": "uint16",
                    "level": level, "time": t, "first_channel": first, "z": z, "x": x, "y": y,
                    "width": width, "height": height, "channels": channels, "valid_pixels": width * height}
        assert all(info[key] == value for key, value in expected.items()), (name, info)
        snapshot = info["reader_snapshot"]
        implementation = snapshot["implementation"]
        assert snapshot["schema"] == "qpx.reader-plugin-link/2"
        assert implementation["provider_id"] == provider and implementation["contract"] == contract
        assert implementation["module_sha256"] == entry["module"]["sha256"]
        assert implementation["capabilities"] == 11  # RGBA | RAW | SOURCE_STORE
        observed = snapshot["observed"]
        assert observed["scope"] == "directory_store" and Path(observed["locator"]) == source
        assert Path(observed["primary"]["path"]) == source / "dataset.xml"
        digest = observed["directory_sha256"]
        assert len(digest) == 64 and len(bytes.fromhex(digest)) == 32
        assert identities.setdefault(source, digest) == digest
        assert all(snapshot["metadata"][key] == value for key, value in IMAGE.items()), snapshot["metadata"]
        assert all(snapshot["raw"][key] == value for key, value in RAW_METADATA.items()), snapshot["raw"]
        assert npy(dest, (height, width, channels)) == crop(
            arrays, level, t, z, x, y, width, height, first, channels), name

    def variant(name):
        dest = output / "variants" / name / "project"
        shutil.copytree(project, dest)
        return dest

    def edit_xml(name, edit):
        source = variant(name)
        path = source / "dataset.xml"
        tree = ET.parse(path)
        edit(tree)
        tree.write(path, encoding="utf-8")
        return source

    def change_xml(name, edit, status=2):
        source = edit_xml(name, edit)
        first = int(args.missing_view)
        raw(name, source, t=1, z=2, level=1, x=3, y=3, width=4, height=3,
            first=first, channels=CHANNEL_COUNT - first, status=status)
        if status == 0:
            assert identities[source] != identities[project]

    def reverse_setups(tree):
        parent = tree.find(SETUPS)
        nodes = parent.findall("ViewSetup")
        for node in nodes:
            parent.remove(node)
        parent.extend(reversed(nodes))

    def assign_different_tiles(tree):
        catalog = ET.Element("Attributes", name="tile")
        tree.find(SETUPS).insert(0, catalog)
        nodes = setup_nodes(tree)
        for index, setup in enumerate(SETUP_IDS):
            ET.SubElement(ET.SubElement(catalog, "Tile"), "id").text = str(index)
            ET.SubElement(nodes[setup].find("attributes"), "tile").text = str(index)

    def change_shared_roles():
        def definition(tree, role):
            return tree.find(f"{SETUPS}/Attributes[@name='{role}']/{role.capitalize()}")

        def remove_geometry(tree):
            for role, tags in (("tile", ("location",)), ("angle", ("axis", "degrees"))):
                node = definition(tree, role)
                for tag in tags:
                    node.remove(node.find(tag))

        def add_second_tile(tree):
            catalogue = tree.find(f"{SETUPS}/Attributes[@name='tile']")
            ET.SubElement(ET.SubElement(catalogue, "Tile"), "id").text = "9"

        change_xml("named-shared-roles", remove_geometry, status=0)
        change_xml("unknown-tile-id",
                   lambda tree: setattr(setup_nodes(tree)[7].find("attributes/tile"), "text", "77"))
        change_xml("missing-shared-angle",
                   lambda tree: setup_nodes(tree)[7].find("attributes").remove(
                       setup_nodes(tree)[7].find("attributes/angle")))
        change_xml("unknown-illumination-id",
                   lambda tree: setattr(setup_nodes(tree)[7].find("attributes/illumination"), "text", "77"))
        change_xml("nonzero-tile-location",
                   lambda tree: setattr(definition(tree, "tile").find("location"), "text", "10 0 0"))
        change_xml("nonzero-angle",
                   lambda tree: setattr(definition(tree, "angle").find("degrees"), "text", "90"))
        change_xml("incomplete-angle-metadata",
                   lambda tree: definition(tree, "angle").remove(definition(tree, "angle").find("axis")))
        change_xml("second-tile", add_second_tile)
        change_xml("unknown-acquisition-catalog",
                   lambda tree: ET.SubElement(tree.find(SETUPS), "Attributes", name="phase"))
    def partition_mutations():
        import h5py
        import numpy as np

        def first(tree, tag):
            return tree.find(f"{IMAGE_LOADER}/partition/{tag}")

        change_xml("windows-partition-separator",
                   lambda tree: setattr(first(tree, "path"), "text", "parts\\part-0.h5"), status=0)
        change_xml("incomplete-partitions",
                   lambda tree: tree.find(IMAGE_LOADER).remove(tree.findall(f"{IMAGE_LOADER}/partition")[-1]))
        change_xml("escaped-partition", lambda tree: setattr(first(tree, "path"), "text", "../outside.h5"))
        change_xml("windows-escaped-partition", lambda tree: setattr(first(tree, "path"), "text", "..\\outside.h5"))
        change_xml("overlapping-partitions",
                   lambda tree: setattr(tree.findall(f"{IMAGE_LOADER}/partition/timepoints")[-1], "text", "0"))
        change_xml("invalid-map-length", lambda tree: setattr(first(tree, "setupsMapped"), "text", "14"))
        change_xml("duplicate-mapped-setup", lambda tree: setattr(first(tree, "setupsMapped"), "text", "14 14"))
        missing = variant("missing-companion")
        (missing / "parts/part-0.h5").unlink()
        raw("missing-companion", missing, status=2)
        for name, target in (
            ("mismatched-partition-file", h5py.ExternalLink("parts/part-1.h5", "/t00011/s19/0/cells")),
            ("mismatched-mapped-view", h5py.ExternalLink("parts/part-0.h5", "/t00011/s14/0/cells")),
            ("external-link-escape", h5py.ExternalLink("../outside.h5", "/t00011/s19/0/cells")),
            ("soft-master-link", h5py.SoftLink("/t00000/s07/0/cells")),
        ):
            source = variant(name)
            with h5py.File(source / "dataset.h5", "r+") as master:
                key = "t00000/s02/0/cells"
                del master[key]
                master[key] = target
            raw(name, source, status=2)
        name = "mismatched-partition-type"
        source = variant(name)
        with h5py.File(source / "parts/part-0.h5", "r+") as partition:
            partition["s19"].attrs["dataType"] = np.bytes_("int16")
        raw(name, source, status=2)

        name = "stable-pixels-changed-companion-identity"
        source = variant(name)
        raw("before-companion-change", source, t=0, width=12, height=8)
        before = identities.pop(source)
        with h5py.File(source / "parts/part-0.h5", "r+") as partition:
            partition.attrs["unselected-metadata"] = np.bytes_("identity change")
        raw(name, source, t=0, width=12, height=8)
        assert identities[source] != before

    def missing_views():
        def mark(tree, setup, time):
            ET.SubElement(tree.find(MISSING_VIEWS), "MissingView", setup=str(setup), timepoint=str(time))

        def register_missing(tree):
            node = copy.deepcopy(registrations(tree)[(7, 1)])
            node.attrib["setup"] = str(MISSING_PAIR[0])
            tree.find(REGISTRATIONS).append(node)

        raw("missing-channel-alone", t=1, first=0, channels=1, status=2, diagnostic=MISSING_DIAGNOSTIC)
        raw("span-contains-missing-channel", t=1, status=2, diagnostic=MISSING_DIAGNOSTIC)
        change_xml("duplicate-missing-view", lambda tree: mark(tree, *MISSING_PAIR))
        change_xml("unknown-missing-setup", lambda tree: mark(tree, 12, 1))
        change_xml("unknown-missing-time", lambda tree: mark(tree, 7, 2))
        change_xml("undeclared-storage-gap", lambda tree: tree.find("./SequenceDescription").remove(tree.find(MISSING_VIEWS)))
        change_xml("missing-with-registration", register_missing, status=0)

        name = "all-times-missing-for-channel"
        source = edit_xml(name, lambda tree: mark(tree, 2, 0))
        raw(name, source, t=0, first=1, channels=1)
        raw("physically-present-but-declared-missing", source, t=0, channels=1, status=2, diagnostic=MISSING_DIAGNOSTIC)
        pam = output / "display-missing-channel.pam"
        run("display-missing-channel", [*prefix, *reader, source, 0, 0, 0, 4, 3, pam], 1)
        assert MISSING_DIAGNOSTIC in commands[-1]["stderr"] and not pam.exists()

        source = edit_xml("all-channels-missing-at-time", lambda tree: mark(tree, 7, 1))
        raw("complete-time-in-sparse-project", source)
        raw("all-channels-missing-at-time", source, t=1, status=2, diagnostic=MISSING_DIAGNOSTIC)

        def mark_all(tree):
            for setup, time in registrations(tree):
                mark(tree, setup, time)

        change_xml("wholly-missing-project", mark_all)
        if args.partitioned:
            def map_absent_partition(tree):
                partition = ET.SubElement(tree.find(IMAGE_LOADER), "partition")
                ET.SubElement(partition, "path", type="relative").text = "parts/never-acquired.h5"
                ET.SubElement(partition, "timepoints").text = "1"
                ET.SubElement(partition, "setups").text = "2"

            change_xml("only-missing-views-in-partition", map_absent_partition, status=0)

    passed = False
    try:
        catalog = json.loads(run("catalog-with-plugin", [*prefix, "--readers"]))
        descriptor, = (item for item in catalog["readers"] if item["provider_id"] == provider)
        assert descriptor["available"] and descriptor["display_rgba"] and descriptor["raw_samples"]
        assert descriptor["contract"] == contract and descriptor["module_sha256"] == entry["module"]["sha256"]
        for level, shape in enumerate(LEVELS):
            for t in range(TIME_COUNT):
                for z in range(Z_COUNT):
                    first = int(args.missing_view and t == MISSING_PAIR[1])
                    raw(f"level-{level}-t{t}-z{z}", t=t, z=z, level=level,
                        width=shape["width"], height=shape["height"],
                        first=first, channels=CHANNEL_COUNT - first)
        raw("selected-channel-zero", t=0 if args.missing_view else 1, first=0, channels=1,
            z=2, x=3, y=2, width=7, height=5)
        raw("selected-channel-one", t=1, first=1, channels=1, z=2, x=3, y=2, width=7, height=5)
        first = int(args.missing_view)
        raw("pyramid-odd-origin", t=1, z=2, level=1, x=7, y=5, width=3, height=2,
            first=first, channels=CHANNEL_COUNT - first)
        raw("pyramid-last-pixel", t=1, z=2, level=1, x=11, y=7, first=first, channels=CHANNEL_COUNT - first)
        change_xml("document-order-is-not-channel-order", reverse_setups, status=0)

        for level, x, y, width, height in ((0, 3, 2, 4, 3), (1, 7, 5, 3, 2)):
            pam = output / f"display-level-{level}.pam"
            run(f"display-level-{level}", [*prefix, *reader, project, x, y, level, width, height, pam])
            header, encoded = pam.read_bytes().split(b"ENDHDR\n", 1)
            assert f"WIDTH {width}\nHEIGHT {height}\nDEPTH 4\n".encode() in header
            values = crop(arrays, level, 0, 0, x, y, width, height)
            expected = bytes(component for index in range(0, len(values), 2) for component in
                             ((values[index] * 255 + 32767) // 65535,
                              (values[index + 1] * 255 + 32767) // 65535, 0, 255))
            assert encoded == expected, f"display-level-{level}"

        for name, edit in (
            ("duplicate-channel-id", lambda tree: setattr(setup_nodes(tree)[7].find("attributes/channel"), "text",
                                                         setup_nodes(tree)[2].findtext("attributes/channel"))),
            ("different-view-role", assign_different_tiles),
            ("different-setup-dimensions", lambda tree: setattr(setup_nodes(tree)[7].find("size"), "text",
                                                                "11 8 3")),
            ("different-voxel-size", lambda tree: setattr(setup_nodes(tree)[7].find("voxelSize/size"), "text",
                                                        "0.5 0.5 1.5")),
            ("unregistered-channel", lambda tree: tree.find(REGISTRATIONS).remove(registrations(tree)[(7, 1)])),
            ("shifted-channel", lambda tree: setattr(registrations(tree)[(7, 1)].find("ViewTransform/affine"),
                                                     "text", "0.25 0 0 6 0 0.5 0 0 0 0 1.5 0")),
        ):
            change_xml(name, edit)
        raw("invalid-channel", first=2, channels=1, status=2)
        raw("invalid-channel-span", first=1, status=2)
        if args.partition_mutations:
            partition_mutations()
        if args.shared_view_roles:
            change_shared_roles()
        if args.missing_view:
            missing_views()

        protected = variant("protected-project")
        inside = protected / "output.npy"
        run("output-inside-source", [*prefix, *reader, "--raw", protected, inside], 2)
        assert not inside.exists()
        verify_inventory(protected, receipt["members"])
        verify_inventory(project, receipt["members"])
        passed = True
    finally:
        stamp = {"passed": passed, "fixture_sha256": sha(receipt_path),
                 "oracle_sha256": [sha(fixture / f"expected-level-{i}.npy") for i in range(len(LEVELS))],
                 "region_sha256": sha(region),
                 "manifest_sha256": sha(manifest), "module_sha256": sha(module), "commands": commands}
        (output / "results.json").write_text(json.dumps(stamp, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(commands)} native multi-setup BDV {args.backend.upper()} commands")


if __name__ == "__main__":
    main()
