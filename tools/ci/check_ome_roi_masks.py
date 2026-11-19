"""Exercise OME XML Mask shapes and the existing ABI5 import and reopen."""

import argparse
import base64
import bz2
from hashlib import sha256
import json
from math import floor
from pathlib import Path
import subprocess
import zlib

NS = "http://www.openmicroscopy.org/Schemas/OME/2016-06"
PROVIDER = "org.qupathxx.ome-roi"


def sha(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "probe", "manifest", "prior-manifest", "prior-mask-manifest", "template", "output"):
        parser.add_argument("--" + key, type=lambda value: Path(value).resolve(), required=True)
    parser.add_argument("--vector-fixtures", required=True, type=lambda value: Path(value).resolve())
    parser.add_argument("--vector-template", required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    root = args.output
    root.mkdir(parents=True)
    template_sha = sha(args.template)
    entry, = json.loads(args.manifest.read_bytes())["plugins"]
    module = args.manifest.with_name(entry["module"]["path"])
    assert entry["module"]["sha256"] == sha(module) and entry["abi"] == 5 and entry["version"] == "1.3.0"
    coverage = "11100" "10100" "11100" "00000" "00001"
    sample = int(coverage.ljust(32, "0"), 2).to_bytes(4, "big")

    def bindata(payload=sample, length=None, extra="", wrap=False):
        encoded = base64.b64encode(payload).decode()
        if length is None:
            length = len(encoded)
        if wrap:
            encoded = "\n " + encoded[:4] + "\n " + encoded[4:] + "\n"
        return f'<BinData Length="{length}" BigEndian="false"{extra}>{encoded}</BinData>'

    def fixture(*, data=None, transform="", x=2, y=3, w=5, h=5, canvas=16, bound=True):
        image = (f'<Image ID="Image:0"><Pixels ID="Pixels:0" DimensionOrder="XYCZT" Type="uint8" '
                 f'SizeX="{canvas}" SizeY="16" SizeZ="2" SizeC="3" SizeT="2">'
                 '<Channel ID="Channel:0"/><MetadataOnly/></Pixels><ROIRef ID="ROI:0"/></Image>')
        if data is None:
            data = bindata()
        return (f'<OME xmlns="{NS}">{image if bound else ""}<ROI ID="ROI:0"><Union>'
                f'<Mask ID="Shape:mask" Text="Mask region" Locked="true" TheZ="1" TheT="1" '
                f'X="{x}" Y="{y}" Width="{w}" Height="{h}">{transform}{data}</Mask>'
                '<Point ID="Shape:point" X="1.25" Y="2.5" TheZ="0" TheT="0"/>'
                '</Union></ROI></OME>')

    def invoke(source, output, manifest=args.manifest, status=0, template=args.template, options=None):
        command = [args.host, "--format-plugins", manifest, "annotations-import",
                   PROVIDER, template, "1", source, output]
        if options is not None:
            command.extend(("--plugin-options-json", json.dumps(options)))
        result = subprocess.run(command, cwd=root, capture_output=True, encoding="utf-8", timeout=30)
        assert result.returncode == status, (command, result.returncode, result.stdout, result.stderr)
        return result

    inputs, imports, reopens, refused = {}, {}, {}, {}
    codecs = {"zlib": zlib.compress, "bzip2": bz2.compress}

    def compressed(codec, payload):
        return bindata(payload, extra=f' Compression="{codec}"', wrap=True)

    def affine(a, b, c, d, x, y):
        return f'<Transform A00="{a}" A01="{b}" A10="{c}" A11="{d}" A02="{x}" A12="{y}"/>'

    orientations = {
        "grid-identity": (1, 0, 0, 1, -2, -3),
        "grid-flip-x": (-1, 0, 0, 1, 18, 1),
        "grid-flip-y": (1, 0, 0, -1, 1, 9),
        "grid-rotate-180": (-1, 0, 0, -1, 11, 13),
        "grid-transpose": (0, 1, 1, 0, -3, 2),
        "grid-rotate-90": (0, -1, 1, 0, 11, 1),
        "grid-rotate-270": (0, 1, -1, 0, 7, 7),
        "grid-antitranspose": (0, -1, -1, 0, 12, 11),
    }

    def transformed_coverage(matrix):
        a, b, c, d, tx, ty = matrix
        target = ["0"] * 256
        for i, bit in enumerate(coverage):
            if bit == "1":
                x, y = 2.5 + i % 5, 3.5 + i // 5
                u, v = floor(a*x + b*y + tx), floor(c*x + d*y + ty)
                assert 0 <= u < 16 and 0 <= v < 16 and target[16*v+u] == "0"
                target[16*v+u] = "1"
        return "".join(target)

    for name, xml in (
        ("mixed", fixture(data=bindata(wrap=True))),
        ("translated", fixture(x=3, y=1, transform='<Transform A00="1" A01="0" A10="0" A11="1" A02="-1" A12="2"/>')),
        *((name, fixture(data=compressed(name, encode(sample)))) for name, encode in codecs.items()),
        *((name, fixture(h=6, transform=affine(*matrix),
                         data=compressed("zlib", zlib.compress(sample)) if name == "grid-rotate-90" else None))
          for name, matrix in orientations.items()),
    ):
        source = root / f"{name}.ome.xml"
        source.write_text(xml, encoding="utf-8")
        inputs[name] = sha(source)
        output = root / f"{name}.qpx"
        if name == "mixed":
            old = invoke(source, root / "prior-output.qpx", args.prior_manifest, status=2)
            assert "unsupported shape" in old.stderr and not (root / "prior-output.qpx").exists(), old.stderr
            refused["prior-module"] = json.loads(old.stderr)["error"]
        if name == "grid-rotate-90":
            old_output = root / "prior-grid.qpx"
            old = invoke(source, old_output, args.prior_mask_manifest, status=2)
            assert "integral translation only" in old.stderr and not old_output.exists(), old.stderr
            refused["prior-mask-module"] = json.loads(old.stderr)["error"]
        report = json.loads(invoke(source, output).stdout)
        assert report["receipt"]["source_sha256"] == inputs[name]
        assert report["receipt"]["provider"]["module_sha256"] == sha(module)
        assert [row["source_id"] for row in report["objects"]] == ["Shape:mask", "Shape:point"]
        imports[name] = report
        hidden = source.with_suffix(".offline")
        source.rename(hidden)
        try:
            command = [args.probe, output]
            if name in orientations:
                command.extend(("--coverage", transformed_coverage(orientations[name])))
            reopen = subprocess.run(command, cwd=root, capture_output=True, encoding="utf-8", timeout=30)
            assert reopen.returncode == 0, reopen.stderr
            reopens[name] = reopen.stdout.strip()
        finally:
            hidden.rename(source)

    invalid_streams = []
    for name, encode in codecs.items():
        stream = encode(sample)
        for case, payload in (
            ("short", encode(sample[:-1])), ("long", encode(sample + b"\0")),
            ("truncated", stream[:-2]), ("trailing", stream + b"\0"),
            ("concatenated", stream + stream), ("incorrect-header", b"\0" + stream[1:]),
        ):
            invalid_streams.append((f"{name}-{case}", fixture(data=compressed(name, payload)),
                                    "OME mask stream"))
        invalid_streams.append((f"{name}-tail-bits",
                                fixture(data=compressed(name, encode(sample[:-1] + b"\x81"))),
                                "unused OME mask bits"))

    for name, xml, error in (
        ("no-image", fixture(bound=False), "selected Image Pixels canvas"),
        ("oversized-canvas", fixture(canvas=1000001), "canvas exceeds native bounds"),
        ("different-target-canvas", fixture(canvas=17), "mask canvas differs"),
        ("outside-crop", fixture(x=12), "crop lies outside its canvas"),
        ("fractional-x", fixture(x=2.5), "integral origin"),
        ("scaled", fixture(transform=affine(2, 0, 0, 1, 0, 0)), "grid-preserving XY orientation"),
        ("sheared", fixture(transform=affine(1, 1, 0, 1, 0, 0)), "grid-preserving XY orientation"),
        ("degenerate", fixture(transform=affine(0, 1, 0, 0, 0, 0)), "grid-preserving XY orientation"),
        ("fractional-translation", fixture(transform=affine(0, 1, -1, 0, 0.5, 7)), "integral origin"),
        ("reflected-out-of-bounds", fixture(transform=affine(-1, 0, 0, 1, 6, 0)), "outside its canvas"),
        ("exchanged-out-of-bounds", fixture(h=6, transform=affine(0, 1, 1, 0, 8, 0)), "outside its canvas"),
        ("missing-data", fixture(data=""), "requires one BinData value"),
        ("duplicate-data", fixture(data=bindata() * 2), "Repeated OME mask BinData"),
        ("unsupported-compression", fixture(data=bindata(extra=' Compression="gzip"')),
         "Unsupported OME mask BinData compression"),
        ("wrong-length", fixture(data=bindata(length=4)), "Length differs"),
        ("short-payload", fixture(data=bindata(payload=b"\xff", length=8)), "Length differs"),
        ("bad-base64", fixture(data=bindata().replace("==", "!=", 1)), "base64 encoding"),
        ("nonzero-padding", fixture(data=bindata(payload=sample[:-1]+b"\x81")), "unused OME mask bits"),
        ("empty-mask", fixture(data=bindata(payload=b"\0" * 4)), "coverage is empty"),
        *invalid_streams,
    ):
        path = root / f"{name}.ome.xml"
        path.write_text(xml, encoding="utf-8")
        output = root / f"{name}.qpx"
        result = invoke(path, output, status=2)
        assert error in result.stderr and not output.exists(), result.stderr
        refused[name] = json.loads(result.stderr)["error"]
    vectors = {}
    expected = json.loads((args.vector_fixtures / "fixtures.json").read_bytes())
    for name in ("point", "locked", "primitives", "multi-second", "annotation"):
        filename = name + ".ome.xml"
        options = next(row["options"] for row in expected["positive"] if row["file"] == filename)
        source = args.vector_fixtures / filename
        before, after = (root / f"{name}-{mode}.qpx" for mode in ("prior", "current"))
        invoke(source, before, args.prior_manifest, template=args.vector_template, options=options)
        invoke(source, after, template=args.vector_template, options=options)
        comparison = subprocess.run([args.probe, "--compare", before, after], cwd=root,
                                    capture_output=True, encoding="utf-8", timeout=30)
        assert comparison.returncode == 0, comparison.stderr
        vectors[name] = {"prior": sha(before), "current": sha(after)}
    assert template_sha == sha(args.template)
    result = {"passed": True, "imports": imports, "reopens": reopens, "refusals": refused,
              "module_sha256": sha(module), "manifest_sha256": sha(args.manifest),
              "host_sha256": sha(args.host), "probe_sha256": sha(args.probe), "vector_regressions": vectors}
    (root / "validation.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(imports)} imports, {len(reopens)} offline reopens, "
          f"{len(refused)} refusal controls and {len(vectors)} vector regressions")


if __name__ == "__main__":
    main()
