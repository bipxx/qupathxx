"""Select unmodified XZ Embedded decoder sources and notices for VTI."""

import argparse
from hashlib import sha256
from pathlib import Path
import tarfile
from zipfile import ZipFile, ZipInfo

REVISION = "ae63ae3a36ed01724674e8f3d750dc47bf125410"
UPSTREAM = f"https://codeload.github.com/tukaani-project/xz-embedded/tar.gz/{REVISION}"
UPSTREAM_SHA256 = "2dd3ad60d2a1c3fe39948bf977ddb977bc84bb92cc5f3b20b0dcae8faa619703"
ARCHIVE_NAME = "xz-embedded-decoder.zip"
ARCHIVE_SHA256 = "4b83ab578420027fb7ba8c247d5f98d844df150bccb8f18bca48b7de2bed0c19"
PATHS = (
    "AUTHORS", "COPYING", "linux/include/linux/xz.h", "userspace/xz_config.h",
    "linux/lib/xz/xz_crc32.c", "linux/lib/xz/xz_dec_lzma2.c", "linux/lib/xz/xz_dec_stream.c",
    "linux/lib/xz/xz_lzma2.h", "linux/lib/xz/xz_private.h", "linux/lib/xz/xz_stream.h",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if sha256(args.snapshot.read_bytes()).hexdigest() != UPSTREAM_SHA256:
        raise ValueError("Unexpected XZ Embedded upstream snapshot")
    with tarfile.open(args.snapshot, "r:gz") as source, ZipFile(args.output, "x") as archive:
        for name in sorted(PATHS):
            with source.extractfile(f"xz-embedded-{REVISION}/{name}") as member:
                data = member.read()
            if name.endswith((".c", ".h")) and b"SPDX-License-Identifier: 0BSD" not in data:
                raise ValueError(f"Expected the original 0BSD notice in {name}")
            archive.writestr(ZipInfo(name), data)
    print(sha256(args.output.read_bytes()).hexdigest())


if __name__ == "__main__":
    main()
