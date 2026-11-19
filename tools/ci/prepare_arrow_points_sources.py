"""Extract only the exact admitted library sources from the two upstream tarballs."""
import argparse
import hashlib
from pathlib import Path
import tarfile
import zipfile

UPSTREAM = {
    "nanoarrow-0.9.0": (
        "https://downloads.apache.org/arrow/apache-arrow-nanoarrow-0.9.0/apache-arrow-nanoarrow-0.9.0.tar.gz",
        "801200a0e95e869d5c4bdeb5b535dba58551482bb782b7dc8bd599c8b6e8cacf",
        "apache-arrow-nanoarrow-0.9.0",
    ),
    "lz4-1.10.0": (
        "https://github.com/lz4/lz4/archive/refs/tags/v1.10.0.tar.gz",
        "537512904744b35e232912055ccf8ec66d768639ff3abe5788d90d792ec5f48b",
        "lz4-1.10.0",
    ),
}


def selected(component, name):
    if component == "lz4-1.10.0":
        return name in {
            "lib/" + member for member in ("LICENSE", "lz4.c", "lz4.h", "lz4hc.c", "lz4hc.h",
                                          "lz4frame.c", "lz4frame.h", "lz4frame_static.h",
                                          "xxhash.c", "xxhash.h")
        }
    return (
        name in {"LICENSE.txt", "NOTICE.txt", "thirdparty/flatcc/LICENSE",
                 "src/nanoarrow/nanoarrow_config.h.in", "src/nanoarrow/nanoarrow.h",
                 "src/nanoarrow/nanoarrow_ipc.h"}
        or name.startswith("src/nanoarrow/common/") and name.endswith((".h", ".c"))
        or name in {"src/nanoarrow/ipc/" + member for member in
                    ("decoder.c", "encoder.c", "codecs.c", "flatcc_generated.h")}
        or name.startswith("thirdparty/flatcc/include/") and name.endswith(".h")
        or name.startswith("thirdparty/flatcc/src/runtime/") and name.endswith(".c")
    )


def prepare(component, source, output):
    _, digest, root = UPSTREAM[component]
    assert hashlib.sha256(source.read_bytes()).hexdigest() == digest, source
    with tarfile.open(source, "r:gz") as tar, zipfile.ZipFile(output, "x") as archive:
        for member in sorted(tar, key=lambda item: item.name):
            if member.isfile() and member.name.startswith(root + "/") and selected(
                    component, member.name[len(root) + 1:]):
                archive.writestr(zipfile.ZipInfo(member.name), tar.extractfile(member).read())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nanoarrow", type=Path)
    parser.add_argument("lz4", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    for component, source in zip(UPSTREAM, (args.nanoarrow, args.lz4), strict=True):
        output = args.output / f"{component}-library.zip"
        prepare(component, source, output)
        print(output, hashlib.sha256(output.read_bytes()).hexdigest())


if __name__ == "__main__":
    main()
