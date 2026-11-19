"""Verify that a reviewed TMA proposal can only replay against its source."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("batch", "format-cli", "project", "image-source", "output"):
        parser.add_argument("--" + key, required=True, type=lambda value: Path(value).resolve())
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("PYTHON", "QPX_", "QUPATHXX_"))}
    windows = Path(env["SYSTEMROOT"])
    env["PATH"] = os.pathsep.join(map(str, (windows / "System32", windows)))
    checks = {}

    def run(label, argv, expected=0, binary=args.batch):
        result = subprocess.run([binary, *argv], cwd=args.output, env=env,
                                capture_output=True, encoding="utf-8", timeout=45)
        assert result.returncode == expected, (label, result.stdout, result.stderr)
        payload = json.loads(result.stdout if expected == 0 else result.stderr)
        checks[label] = payload
        return payload

    def detect(label, project, image, output, *flags):
        result = run(label, ["--detect-tma", project, output, "--image", image, "--rows", "3",
                             "--columns", "4", "--core-width", "32", "--core-height", "32", *flags])
        assert result["state"] == "saved"
        return result

    def import_grid(label, project, image, wire, digest, refusal=None, expected=1):
        output = args.output / (label + ".qpxproj")
        result = run(label, ["--import-qptma", project, wire, output, "--image", image,
                             "--expect-review", digest], 0 if refusal is None else expected)
        if refusal is None:
            assert result["state"] == "saved" and result["cores"] == 12 and output.is_file()
        else:
            assert refusal in result["error"] and not output.exists(), (label, result)
        return output

    originals = {path: sha256(path) for path in (args.project, args.image_source)}
    proposed = args.output / "reviewed.qptma"
    digest = detect("review", args.project, "2", proposed, "--review")["review_sha256"]
    assert re.fullmatch("[0-9a-f]{64}", digest)
    repeated = args.output / "repeated.qptma"
    ordinary = args.output / "ordinary.qptma"
    assert detect("repeat-review", args.project, "2", repeated, "--review")["review_sha256"] == digest
    assert "review_sha256" not in detect("ordinary-preview", args.project, "2", ordinary)
    assert proposed.read_bytes() == repeated.read_bytes() == ordinary.read_bytes()

    applied = import_grid("apply-reviewed", args.project, "2", proposed, digest)
    exported = args.output / "reopened.qptma"
    run("reopen-and-export", ["--export-qptma", applied, exported, "--image", "2"])
    header = b"Core name\t"
    assert proposed.read_bytes().split(header, 1)[1] == exported.read_bytes().split(header, 1)[1]

    mismatch = "TMA proposal review differs from the selected project, image or QPTMA bytes"
    import_grid("different-image", args.project, "3", proposed, digest, mismatch)
    edited = args.output / "edited.qptma"
    wire_bytes = proposed.read_bytes()
    edited.write_bytes(wire_bytes.replace(b"\ttrue\t\n", b"\ttrue\tEdited case\n", 1))
    assert edited.read_bytes() != wire_bytes
    import_grid("edited-proposal", args.project, "2", edited, digest, mismatch)
    import_grid("malformed-digest", args.project, "2", proposed, "bad",
                "Expected a lowercase SHA-256 TMA review digest", expected=2)

    changed = args.output / "changed-project.qpxproj"
    run("create-changed-project", ["project-add-image", args.project, args.image_source, changed],
        binary=args.format_cli)
    assert sha256(changed) != originals[args.project]
    import_grid("changed-project-refused", changed, "2", proposed, digest, mismatch)

    linked_source = args.output / "linked.ppm"
    shutil.copyfile(args.image_source, linked_source)
    linked_project = args.output / "linked.qpxproj"
    run("create-linked-project", ["project-create", linked_source, linked_project], binary=args.format_cli)
    project_sha = sha256(linked_project)
    linked_proposal = args.output / "linked.qptma"
    linked_digest = detect("linked-review", linked_project, "1", linked_proposal, "--review")["review_sha256"]
    import_grid("linked-replay", linked_project, "1", linked_proposal, linked_digest)
    with linked_source.open("r+b") as stream:
        stream.seek(-1, os.SEEK_END)
        previous = stream.read(1)
        stream.seek(-1, os.SEEK_END)
        stream.write(bytes([previous[0] ^ 1]))
    assert sha256(linked_source) != originals[args.image_source]
    import_grid("changed-linked-source", linked_project, "1", linked_proposal, linked_digest, mismatch)
    assert sha256(linked_project) == project_sha
    assert {path: sha256(path) for path in originals} == originals
    assert not list(args.output.rglob("*.qpx-tmp-*"))
    assert not list(args.output.rglob(".qpx-save-new-*"))
    (args.output / "validation.json").write_text(json.dumps({
        "passed": True, "batch_sha256": sha256(args.batch), "checks": checks,
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(checks)} TMA proposal review and replay checks passed")


if __name__ == "__main__":
    main()
