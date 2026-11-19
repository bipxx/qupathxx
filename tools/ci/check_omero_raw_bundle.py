"""Verify both members of native OMERO ZIP exports with Python's ZIP reader."""
import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZIP_STORED, ZipFile


def verify(archive, samples, receipt):
    expected = {"raw.npy": samples.read_bytes(), "receipt.json": receipt.read_bytes()}
    with ZipFile(archive) as source:
        assert source.namelist() == list(expected)
        assert source.testzip() is None
        for member in source.infolist():
            assert member.compress_type == ZIP_STORED and member.flag_bits == 0
            assert source.read(member) == expected[member.filename]
    return {
        "archive": str(archive.resolve()),
        "archive_sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
        "result_sha256": json.loads(expected["receipt.json"])["result_sha256"],
        "members": {name: hashlib.sha256(data).hexdigest() for name, data in expected.items()},
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli-root", type=Path, required=True)
    parser.add_argument("--exchange-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    cli = args.cli_root
    checks = [verify(cli / "raw.zip", cli / "raw.npy", cli / "raw.json")]
    for name in ("raw-c.npy", "raw-f.npy", "raw-c-blosc.npy"):
        samples = args.exchange_root / name
        checks.append(verify(samples.with_name(name + ".zip"), samples, samples.with_name(name + ".json")))
    args.output.write_text(json.dumps({"passed": True, "checks": checks}, indent=2) + "\n",
                           encoding="utf-8")
    print(f"Verified CRC and exact sample/receipt bytes for {len(checks)} OMERO ZIP files")


if __name__ == "__main__":
    main()
