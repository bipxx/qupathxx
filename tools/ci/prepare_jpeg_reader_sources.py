"""Copy the JPEG plugin's exact code/build/notice selection from a local cache.

Development build control only. Never downloads or selects upstream test images.
The source kit already supplies these offline inputs; Python is not a product
dependency. Unselected source-cache members never enter the output.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    lock = json.loads((ROOT / "plugins/jpeg/libjpeg-source-lock.json").read_text(encoding="utf8"))
    if lock["schema"] != "qpx.jpeg-reader-sources/1" or lock["version"] != "3.1.3":
        raise ValueError("Unexpected JPEG source lock")
    output = args.output.resolve()
    rows = {}
    for name, record in lock["members"].items():
        rel = PurePosixPath(name)
        if rel.is_absolute() or ".." in rel.parts or ":" in name or "\\" in name:
            raise ValueError("Invalid source member: " + name)
        source = args.source.joinpath(*rel.parts)
        info = source.lstat()
        if not stat.S_ISREG(info.st_mode) or bool(getattr(info, "st_file_attributes", 0) & 0x400):
            raise ValueError("Source must be a regular unlinked file: " + name)
        data = source.read_bytes()
        if len(data) != record["bytes"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
            raise ValueError("Locked source differs: " + name)
        target = output.joinpath(*rel.parts)
        if not target.resolve().is_relative_to(output):
            raise ValueError("Source escapes output: " + name)
        if target.exists():
            target_info = target.lstat()
            if (not stat.S_ISREG(target_info.st_mode) or bool(getattr(target_info, "st_file_attributes", 0) & 0x400)
                    or target.read_bytes() != data):
                raise ValueError("Existing output differs or is linked: " + name)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        rows[name] = record["sha256"]
    print(json.dumps({"schema": "qpx.jpeg-reader-prepared/1", "files": len(rows),
                      "network": False, "output": str(output)}))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit(str(error))
