"""Prepare only the PNG plugin's locked code inputs; never fetch specimen assets.

Development build control only. Runtime readers do not invoke this script.
Network use requires --download and is limited to the fixed libpng commit.
Zlib comes from an existing exact, admitted source cache.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import urllib.request


ROOT = Path(__file__).resolve().parents[2]
PNG_COMMIT = "cd952f49f95bb27154ae77dbb103032d95f6e580"
PNG_BASE = f"https://raw.githubusercontent.com/pnggroup/libpng/{PNG_COMMIT}/"


def regular(path):
    info = path.lstat()
    reparse = bool(getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))
    if not stat.S_ISREG(info.st_mode) or reparse:
        raise ValueError(f"Input must be a regular, unlinked file: {path}")
    return path.read_bytes()


def checked(data, record, name):
    if len(data) != record["bytes"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
        raise ValueError(f"Locked source differs: {name}")
    return data


def prepare(lock_path, output, cache, download):
    lock = json.loads(lock_path.read_text(encoding="utf8"))
    is_png = lock["schema"] == "qpx.png-reader-sources/1"
    if is_png and lock["commit"] != PNG_COMMIT:
        raise ValueError("Unexpected libpng commit")
    rows = {}
    for name, record in lock["members"].items():
        rel = PurePosixPath(name)
        if rel.is_absolute() or ".." in rel.parts or ":" in name or "\\" in name:
            raise ValueError(f"Invalid source member: {name}")
        target = output.joinpath(*rel.parts)
        if not target.resolve().is_relative_to(output.resolve()):
            raise ValueError(f"Source member escapes its output directory: {name}")
        if target.exists():
            data = checked(regular(target), record, name)
        elif cache and cache.joinpath(*rel.parts).exists():
            data = checked(regular(cache.joinpath(*rel.parts)), record, name)
        elif is_png and download:
            url = PNG_BASE + name
            if record["url"] != url:
                raise ValueError(f"Unexpected source URL: {name}")
            with urllib.request.urlopen(url, timeout=60) as response:
                data = checked(response.read(record["bytes"] + 1), record, name)
        else:
            raise ValueError(f"Locked source unavailable offline: {name}")
        if not target.exists():
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        rows[name] = record["sha256"]
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--libpng-cache", type=Path)
    parser.add_argument("--zlib-source", type=Path, required=True)
    parser.add_argument("--download", action="store_true")
    args = parser.parse_args()
    try:
        output = args.output.resolve()
        locks = ROOT / "plugins/png"
        png = prepare(locks / "libpng-source-lock.json", output / "libpng", args.libpng_cache, args.download)
        zlib = prepare(locks / "zlib-source-lock.json", output / "zlib", args.zlib_source, False)
        result = {"schema": "qpx.png-reader-prepared/1", "libpng_commit": PNG_COMMIT,
                  "libpng": png, "zlib": zlib, "network_allowed": args.download}
        (output / "prepared.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf8")
        print(json.dumps({"libpng_files": len(png), "zlib_files": len(zlib), "output": str(output)}))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, str(error) + "\n")


if __name__ == "__main__":
    main()
