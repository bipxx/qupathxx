"""Exercise the installed SVG annotation provider and native project reopen."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from check_project_matrix_index import sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("host", "old-host", "manifest", "legacy-manifest", "reader-manifest", "template", "probe",
                "baseline", "canvas-baseline", "output"):
        parser.add_argument("--" + key, type=lambda path: Path(path).resolve(), required=True)
    args = parser.parse_args()
    out = args.output
    sources = out / "sources"
    sources.mkdir(parents=True)
    baseline = json.loads(args.baseline.read_bytes())
    original = sha(args.template)
    assert original == baseline["template_sha"].lower()
    assert baseline["host_sha"].lower() == sha(args.old_host)
    before_canvas = json.loads(args.canvas_baseline.read_bytes())
    assert before_canvas["exit_code"] == 0 and before_canvas["source_width"] == 8 and before_canvas["target_width"] == 4
    assert before_canvas["host_sha"].lower() == sha(args.old_host)
    entry, = json.loads(args.manifest.read_bytes())["plugins"]
    module = args.manifest.with_name(entry["module"]["path"])
    identity = (sha(module), sha(args.manifest))
    assert entry["provider_id"] == "org.qupathxx.svg-annotations" and entry["abi"] == 6
    assert entry["module"]["sha256"] == identity[0]
    reader = [args.host, "--reader-plugins", args.reader_manifest]
    command = [*reader, "--format-plugins", args.manifest, "annotations-import",
               entry["provider_id"], args.template, "1"]
    env = os.environ.copy()
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {"imports": [], "reopened": [], "refusals": [], "compatibility": []}

    def run(cmd, status=0):
        p = subprocess.run(cmd, cwd=out, env=env, capture_output=True, encoding="utf-8", timeout=30)
        assert p.returncode == status, (cmd, p.returncode, p.stdout, p.stderr)
        return p

    def accepted(label, source, ids):
        output = out / (label + ".qpxproj")
        report = json.loads(run([*command, source, output]).stdout)
        assert [row["source_id"] for row in report["objects"]] == ids
        assert report["receipt"]["provider"]["module_sha256"] == identity[0]
        assert report["receipt"]["source_sha256"] == sha(source)
        checks["imports"].append(report)
        return (label, output)

    writer = sources / "overlay.svg"
    shutil.copyfile(args.baseline.with_name(writer.name), writer)
    assert sha(writer) == baseline["fixture_sha"].lower()
    accepted_outputs = [accepted("writer", writer, ["region", "triangle", "ell", "trace", "ring"])]
    old_host = command[:]
    old_host[0] = args.old_host
    unavailable = out / "unsupported-host.qpxproj"
    error = run([*old_host, writer, unavailable], 2).stderr
    assert "provider unavailable" in error.lower() and not unavailable.exists(), error
    checks["compatibility"].append("older host refuses required canvas ABI")
    legacy = command[:]
    legacy[4] = args.legacy_manifest
    older = legacy[:]
    older[0] = args.old_host
    a = json.loads(run([*older, writer, out / "legacy-old.qpxproj"]).stdout)
    b = json.loads(run([*legacy, writer, out / "legacy-new.qpxproj"]).stdout)
    a.pop("output")
    b.pop("output")
    assert a == b and a["receipt"]["provider"]["abi"] == 3
    checks["compatibility"].append("ABI3 preview, object mappings and receipt are unchanged")
    affine = sources / "affine.svg"
    affine.write_text(
        '<s:svg xmlns:s="http://www.w3.org/2000/svg" width="4px" height="3px" viewBox="0 0 4 6" preserveAspectRatio="none">'
        '<s:rect x="1" width="1" height="2" transform="matrix(1 0 .25 1 0 0)" id="tilted" fill="#09f">'
        '<s:title>A &amp; B</s:title></s:rect>'
        '<s:circle cx="1" cy="1" r=".5" id="sphere"/>'
        '<s:path fill="none" stroke="red" d="M0 2l1 0v2"/></s:svg>', encoding="utf-8"
    )
    accepted_outputs.append(accepted("affine", affine, ["tilted", "sphere", "svg-shape-3"]))

    root = '<svg xmlns="http://www.w3.org/2000/svg" width="4" height="3">'
    controls = (
        ("curve", root, '<path d="M0 0 Q1 2 3 0 Z"/>', "requires straight"),
        ("rounded", root, '<rect width="1" height="1" rx=".1"/>', "Rounded SVG"),
        ("rotated-ellipse", root, '<ellipse cx="1" cy="1" rx=".2" ry=".1" transform="matrix(1 0 .5 1 0 0)"/>',
         "cannot be represented exactly"),
        ("clip-geometry", root, '<rect x="3" width="2" height="1"/>', "clipped by its viewport"),
        ("css", root, '<rect width="1" height="1" style="display:none"/>', "Unsupported SVG attribute"),
        ("stylesheet", '<?xml-stylesheet href="layout.css" type="text/css"?>' + root,
         '<rect width="1" height="1"/>', "processing instructions"),
        ("external-ref", root, '<use href="other.svg#object"/>', "Unsupported SVG annotation element"),
        ("embedded-image", root, '<image href="image.png"/>', "Unsupported SVG annotation element"),
        ("ambiguous-fill", root, '<path d="M0 0H4V3H0Z M1 1H3V2H1Z"/>', "closed even-odd fill"),
        ("filled-polyline", root, '<polyline points="0,0 1,0 1,1" stroke="red"/>', "requires fill=none"),
        ("duplicate-id", root, '<rect width="1" height="1" id="dup"/><rect x="1" width="1" height="1" id="dup"/>',
         "source IDs are not unique"),
        ("singular-transform", root, '<rect width="1" height="1" transform="scale(0)"/>', "Singular SVG transform"),
        ("root-transform", root[:-1] + ' transform="scale(2)">', '<rect width="1" height="1"/>', "root transforms"),
        ("fractional-canvas", root.replace('width="4"', 'width="4.5"'), '<rect width="1" height="1"/>',
         "whole-pixel dimensions"),
        ("doctype", '<!DOCTYPE svg [<!ENTITY a "expansion">]>' + root, '<rect width="1" height="1"/>', "DTD"),
        ("wrong-namespace", '<svg xmlns="urn:other" width="4" height="3">', '<rect width="1" height="1"/>', "SVG namespace"),
    )
    for label, header, body, diagnostic in controls:
        source = sources / (label + ".svg")
        source.write_text(header + body + "</svg>", encoding="utf-8")
        output = out / (label + ".qpxproj")
        before = sha(source)
        report = run([*command, source, output], 2)
        assert diagnostic.lower() in report.stderr.lower(), (label, report.stderr)
        assert not output.exists() and sha(source) == before
        checks["refusals"].append(label)
    mismatch = sources / "mismatch.svg"
    shutil.copyfile(args.canvas_baseline.with_name("different-canvas.svg"), mismatch)
    before = sha(mismatch)
    assert before == before_canvas["report"]["receipt"]["source_sha256"]
    output = out / "mismatch.qpxproj"
    error = run([*command, mismatch, output], 2).stderr
    assert "canvas dimensions differ" in error and "8x6 versus 4x3" in error, error
    assert not output.exists() and sha(mismatch) == before
    checks["refusals"].append("canvas mismatch with in-bounds geometry")
    sources.rename(out / "unavailable")
    for label, path in accepted_outputs:
        result = run([args.probe, path, args.reader_manifest, label])
        checks["reopened"].append({"output_sha256": sha(path), "result": result.stdout.strip()})
    assert sha(args.template) == original and (sha(module), sha(args.manifest)) == identity
    validation = {"passed": True, "module_sha256": identity[0], "manifest_sha256": identity[1],
                  "host_sha256": sha(args.host), "probe_sha256": sha(args.probe), **checks}
    (out / "validation.json").write_text(json.dumps(validation, indent=2) + "\n", encoding="utf-8")
    print({key: len(values) for key, values in checks.items()})


if __name__ == "__main__":
    main()
