"""Fetch public reader fixtures, verifying their committed identities."""
import hashlib
import json
from pathlib import Path
import urllib.request


def main():
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / "tests/fixtures/manifest.json").read_text())
    destination = root / "build/fixtures"
    destination.mkdir(parents=True, exist_ok=True)
    for entry in manifest["downloads"]:
        path = destination / entry["filename"]
        if not path.exists():
            partial = path.with_suffix(path.suffix + ".partial")
            try:
                urllib.request.urlretrieve(entry["url"], partial)
                if hashlib.sha256(partial.read_bytes()).hexdigest() != entry["sha256"]:
                    raise RuntimeError(f"Fixture checksum mismatch: {entry['filename']}")
                partial.replace(path)
            finally:
                partial.unlink(missing_ok=True)
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            raise RuntimeError(f"Fixture checksum mismatch: {entry['filename']}")
        print(path)


if __name__ == "__main__":
    main()
