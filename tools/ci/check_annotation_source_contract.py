"""Independent source-fact/plane fixtures and native ABI3 contract controller.

Development unit controls only, not an ImageJ parser or a product data authority.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from check_format_family_plugins import quantitative_tiff


def volume_tiff():
    """Literal uncompressed RGB, Z2/T2 with explicit OME mapping, no library."""
    xml = ("<?xml version=\"1.0\"?><OME xmlns=\"http://www.openmicroscopy.org/Schemas/OME/2016-06\">"
           "<Image ID=\"Image:0\"><Pixels ID=\"Pixels:0\" DimensionOrder=\"XYCZT\" Type=\"uint8\" "
           "SizeX=\"16\" SizeY=\"16\" SizeC=\"3\" SizeZ=\"2\" SizeT=\"2\" Interleaved=\"true\">"
           "<Channel ID=\"Channel:0:0\" SamplesPerPixel=\"3\"/><TiffData IFD=\"0\" FirstC=\"0\" "
           "FirstZ=\"0\" FirstT=\"0\" PlaneCount=\"4\"/></Pixels></Image></OME>\0").encode("ascii")
    if len(xml) % 2:
        xml += b"\0"
    offsets, at = [], 8
    for i in range(4):
        offsets.append(at)
        at += 2 + 12 * (13 if i == 0 else 12) + 4
    bits, formats, description = at, at + 6, at + 12
    pixels = description + len(xml)
    data = bytearray(b"II" + struct.pack("<HI", 42, offsets[0]))
    for i in range(4):
        fields = [(256, 4, 1, 16), (257, 4, 1, 16), (258, 3, 3, bits),
                  (259, 3, 1, 1), (262, 3, 1, 2), (273, 4, 1, pixels + i * 768),
                  (274, 3, 1, 1), (277, 3, 1, 3), (278, 4, 1, 16),
                  (279, 4, 1, 768), (284, 3, 1, 1), (339, 3, 3, formats)]
        if i == 0:
            fields.append((270, 2, len(xml), description))
        fields.sort()
        data += struct.pack("<H", len(fields))
        data += b"".join(struct.pack("<HHII", *field) for field in fields)
        data += struct.pack("<I", offsets[i + 1] if i < 3 else 0)
    assert len(data) == bits
    data += struct.pack("<HHHHHH", 8, 8, 8, 1, 1, 1) + xml + bytes([80]) * (768 * 4)
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures", type=Path, required=True)
    parser.add_argument("--test", type=Path)
    parser.add_argument("--module", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.test is None:
        args.fixtures.mkdir(parents=True, exist_ok=False)
        (args.fixtures / "image.tif").write_bytes(quantitative_tiff())
        (args.fixtures / "volume.tif").write_bytes(volume_tiff())
        print(json.dumps({"authored_sources": 2, "raw_code": 80, "volume_z": 2, "volume_time": 2}))
        return
    assert args.module and args.output
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    manifest = output / "format-plugins.json"
    module = args.module.resolve()
    # Manifest paths stay within the private package; no absolute module entry.
    import shutil
    shutil.copyfile(module, output / module.name)
    entry = {"provider_id": "org.qupathxx.test.annotation-source", "format_id": "test-annotation-source",
             "domain": "annotation", "display_name": "Controlled annotation source", "version": "1.0.0",
             "contract": "controlled-source-facts/1", "abi": 3, "profile": "windows-x64-msvc1944-release-md",
             "module": {"path": module.name, "sha256": sha(module)}, "dependencies": []}
    manifest.write_text(json.dumps({"schema": "qpx.format-plugins/1", "plugins": [entry]}, indent=2) + "\n", encoding="utf8")
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith("PYTHON")}
    env["PATH"] = os.pathsep.join([os.environ["SystemRoot"] + "/System32", os.environ["SystemRoot"]])
    artifacts = [args.test.resolve(), module, output / module.name, manifest]
    before = {str(path): sha(path) for path in artifacts}
    fixtures = {path.name: sha(path) for path in args.fixtures.iterdir() if path.is_file()}
    result = {"schema": "qpx.annotation-source-contract/1", "artifacts_before": before, "fixtures": fixtures,
              "commands": [], "passed": False, "scope": "Native source facts/plane/value-codec unit controls; not ImageJ format qualification"}
    modes = ["source", "planes", "contours", "refusals", "metadata-source-total", "metadata-property-total",
             "metadata-source-huge", "metadata-encoding", "probe-properties-huge", "probe-properties-pointer",
             "property-key-offset", "property-value-size", "property-duplicate", "property-utf8", "property-nul",
             "property-span", "property-fill-count", "property-fill-pointer", "name-offset", "name-utf8",
             "color-overflow", "color-without-flag", "row-encoding", "linear-zero", "linear-czt", "plane-kind", "row-reserved", "offline"]
    for mode in modes:
        work = output / ("source" if mode == "offline" else mode)
        if mode != "offline":
            work.mkdir()
        command = [str(args.test.resolve()), mode, str(manifest), str(args.fixtures.resolve()), str(work)]
        r = subprocess.run(command, capture_output=True, env=env)
        log = output / (mode + ".log")
        log.write_bytes(r.stdout + r.stderr)
        result["commands"].append({"mode": mode, "argv": command, "returncode": r.returncode, "log": log.name, "log_sha256": sha(log)})
        (output / "validation.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
        assert r.returncode == 0, log.read_text(encoding="utf8", errors="replace")
    result["artifacts_after"] = {str(path): sha(path) for path in artifacts}
    assert result["artifacts_before"] == result["artifacts_after"]
    assert result["fixtures"] == {path.name: sha(path) for path in args.fixtures.iterdir() if path.is_file()}
    result["passed"] = True
    (output / "validation.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
    print(json.dumps({"passed": True, "native_contract_checks": len(modes)}))


if __name__ == "__main__":
    main()
