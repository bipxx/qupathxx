"""Verify the frozen native Windows reader archive on a validation host."""

import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import zipfile

import check_material_staging as admission
from stage_native_release import sha


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("archive", "archive-receipt", "probe", "metaimage-fixture", "vti-fixture", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--inventory", action="append", type=Path, required=True)
    parser.add_argument("--archive-sha256", required=True, help="Expected hash from the delivery record")
    args = parser.parse_args()
    if os.name != "nt" or not __debug__:
        parser.error("Run on Windows with Python assertions enabled")

    review = json.loads(args.archive_receipt.read_bytes())
    digest = sha(args.archive)
    if digest != args.archive_sha256 or digest != review["archive_sha256"]:
        parser.error("Archive SHA-256 does not match the frozen delivery")
    powershell = shutil.which("pwsh.exe")
    if powershell is None:
        parser.error("PowerShell 7 is required for the manifest-owned removal check")
    entries = admission.inventory(args.inventory)
    expected = review["material_members"]
    count = admission.check_archive(args.archive, entries, expected=expected)
    fixture_hashes = {
        name: sha(args.metaimage_fixture / name) for name in ("study.mhd", "tail.zraw", "head.zraw")
    }
    fixture_hashes["arrays.vti"] = sha(args.vti_fixture)
    probe_hash = sha(args.probe)

    scripts = Path(__file__).resolve().parent
    work = args.work.resolve()
    work.mkdir()
    install = work / "installed"
    install.mkdir()
    with zipfile.ZipFile(args.archive) as packet:
        packet.extractall(install)
    if admission.check_stage(install, entries) != count:
        raise RuntimeError("Extracted material count differs from the archive")
    members = {str(install / name): entries[ident]["sha256"] for name, ident in expected.items()}
    if any(sha(Path(path)) != value for path, value in members.items()):
        raise RuntimeError("Installed file differs from its admitted archive member")

    setup = work / "validation setup"
    setup.mkdir()
    probe = shutil.copyfile(args.probe, setup / args.probe.name)
    for name in ("geos_c.dll", "geos.dll", "libopenslide-1.dll"):
        shutil.copyfile(install / "qupathxx" / name, setup / name)
    outputs = work / "user data"
    subprocess.run([
        sys.executable, "-E", scripts / "check_installed_reader_matrix_replay.py", "--use-installed", "--desktop",
        "--runtime", install / "qupathxx", "--output", outputs, "--probe", probe,
        "--metaimage-manifest", install / "providers/metaimage/reader-plugins.json",
        "--metaimage-fixture", args.metaimage_fixture.resolve(),
        "--vti-manifest", install / "providers/vti/reader-plugins.json", "--vti-fixture", args.vti_fixture.resolve(),
        "--counts-manifest", install / "providers/counts/format-plugins.json",
        "--geometry-manifest", install / "providers/geometry/format-plugins.json",
    ], check=True)
    workflow = json.loads((outputs / "validation.json").read_bytes())
    if not workflow["passed"] or len(workflow["checks"]) != 15:
        raise RuntimeError("Incomplete frozen reader workflow")
    if any(members.get(path) != value for path, value in workflow["runtime_and_providers_sha256"].items()):
        raise RuntimeError("Reader workflow used unexpected installed files")

    sentinel = install / "Research notes.txt"
    sentinel.write_text("Authored notes must survive manifest-owned removal.\n", encoding="utf-8")
    protected = {str(path): sha(path) for path in outputs.rglob("*") if path.is_file()}
    protected[str(sentinel)] = sha(sentinel)
    receipt = work / "installation-removal.json"
    write_json(receipt, {
        "state": "install_and_use_passed", "work": str(work), "installed_files": members,
        "protected_user_files": protected, "install_run_verified": True, "uninstall_verified": False,
        "archive_sha256": digest, "compiled_source_sha256": review["source_sha256"],
        "host": {"platform": platform.platform(), "machine": platform.machine(),
                 "runner_image": os.environ.get("ImageOS"), "runner_version": os.environ.get("ImageVersion")},
        "fixture_sha256": fixture_hashes, "setup_probe_sha256": probe_hash,
    })
    subprocess.run([powershell, "-NoProfile", "-File", scripts / "remove_native_handoff.ps1", "-Receipt", receipt],
                   check=True)
    removed = json.loads(receipt.read_text(encoding="utf-8-sig"))
    if not removed["uninstall_verified"] or [path for path in install.rglob("*") if path.is_file()] != [sentinel]:
        raise RuntimeError("Manifest-owned removal left unexpected installed files")
    if any(sha(Path(path)) != value for path, value in protected.items()):
        raise RuntimeError("Manifest-owned removal changed user data")
    print(f"Verified {count} archive members and preserved {len(protected)} user and validation files.")


if __name__ == "__main__":
    main()
