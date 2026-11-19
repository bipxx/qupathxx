"""Exercise the offline native verifier using completed native bundle fixtures."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import zipfile
import zlib


def positions(archive, name):
    with zipfile.ZipFile(io.BytesIO(archive)) as source:
        members = source.infolist()
    central = struct.unpack_from("<I", archive, len(archive) - 6)[0]
    for member in members:
        if member.filename == name:
            payload = member.header_offset + 30 + len(name)
            return member, central, payload
        central += 46 + len(member.filename)
    raise ValueError(name)


def change_member(archive, name, change, recalculate_crc=True):
    data = bytearray(archive)
    member, central, start = positions(data, name)
    end = start + member.file_size
    changed = change(bytes(data[start:end]))
    assert len(changed) == end - start
    data[start:end] = changed
    if recalculate_crc:
        crc = zlib.crc32(changed)
        struct.pack_into("<I", data, member.header_offset + 14, crc)
        struct.pack_into("<I", data, central + 16, crc)
    return data


def verify(exe, archives, output):
    output.mkdir(parents=True)
    checks = []

    def run(name, path, error=None):
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        command = [str(exe), "verify-raw-bundle", str(path)]
        result = subprocess.run(command, cwd=output, capture_output=True, text=True, timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        checks.append({"name": name, "argv": command, "exit": result.returncode, "sha256": before})
        assert hashlib.sha256(path.read_bytes()).hexdigest() == before
        if error is not None:
            assert result.returncode == 1 and error in result.stderr and not result.stdout, result
            return
        assert result.returncode == 0 and not result.stderr, result
        report = json.loads(result.stdout)
        assert report["state"] == "verified" and report["schema"] == "qpx.omero-cli/1"
        with zipfile.ZipFile(path) as source:
            assert report["receipt"] == json.loads(source.read("receipt.json"))

    def extract(name, path, target, error=None):
        archive = path.read_bytes()
        previous = target.read_bytes() if target.is_file() else None
        command = [str(exe), "extract-raw-bundle", str(path), str(target)]
        result = subprocess.run(command, cwd=output, capture_output=True, text=True, timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf8")
        checks.append({"name": name, "argv": command, "exit": result.returncode,
                       "sha256": hashlib.sha256(archive).hexdigest()})
        assert path.read_bytes() == archive
        assert not list(target.parent.glob(target.name + ".qpx-tmp-*"))
        if error is not None:
            assert result.returncode == 1 and error in result.stderr and not result.stdout, result
            assert (target.read_bytes() if target.is_file() else None) == previous
            return
        assert result.returncode == 0 and not result.stderr, result
        report = json.loads(result.stdout)
        assert report["schema"] == "qpx.omero-cli/1" and report["state"] == "extracted"
        with zipfile.ZipFile(io.BytesIO(archive)) as source:
            assert report["receipt"] == json.loads(source.read("receipt.json"))
            assert target.read_bytes() == source.read("raw.npy")

    for index, archive in enumerate(archives):
        run(f"verified-{index}", archive)
        extract(f"extracted-{index}", archive, output / f"exact-{index}.npy")
    original = archives[0].read_bytes()

    def replace_result_hash(receipt):
        start = receipt.index(b'"result_sha256":"') + len(b'"result_sha256":"')
        flip = b"1" if receipt[start] == ord("0") else b"0"
        return receipt[:start] + flip + receipt[start + 1:]

    def change_numpy_type(data):
        assert data.count(b"<u2") == 1
        return data.replace(b"<u2", b"<i2", 1)

    def change_last(data):
        return data[:-1] + bytes((data[-1] ^ 1,))

    refusals = {
        "samples-crc": (change_member(original, "raw.npy", change_last, False), "NPY CRC differs"),
        "samples-digest": (change_member(original, "raw.npy", change_last), "sample digest differs"),
        "numpy-type": (change_member(original, "raw.npy", change_numpy_type), "NPY header differs"),
        "receipt-digest": (change_member(original, "receipt.json", replace_result_hash),
                           "receipt digest differs"),
        "trailing": (original + b"\0", "unexpected bytes after ZIP directory"),
        "truncated": (original[:-1], "truncated archive"),
    }
    for name, (data, error) in refusals.items():
        path = output / (name + ".zip")
        path.write_bytes(data)
        run(name, path, error)
        extract(f"{name}-extraction", path, output / f"{name}.npy", error)
    extract("existing-output", archives[0], output / "exact-0.npy", "already exists")
    extract("archive-as-output", archives[0], archives[0], "already exists")
    copied = output / "archive-copy.zip"
    copied.write_bytes(original)
    alias = output / "archive-alias.npy"
    os.link(copied, alias)
    extract("archive-hard-link", copied, alias, "already exists")
    extract("missing-output-parent", archives[0], output / "missing" / "raw.npy", "cannot create export temporary file")
    assert not (output / "missing").exists()
    (output / "validation.json").write_text(json.dumps({"passed": True, "checks": checks},
                                                       indent=2) + "\n", encoding="utf-8")
    print(f"Passed {len(checks)} native offline bundle checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--archive", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    verify(args.exe.resolve(), [path.resolve() for path in args.archive], args.output.resolve())
