"""Create reproducible decoder source bundles from the pinned upstream releases."""
import argparse
import gzip
import hashlib
from pathlib import Path, PurePosixPath
import tarfile

LIBAVIF = "libavif-1.4.2"
LIBAOM = "libaom-3.14.1"
ORIGINAL_SHA256 = {
    LIBAVIF: "2b645287340ba5a631d268b551dc2d72bd73ac33335962dd36dcdb6d8366921d",
    LIBAOM: "44bf90dbd23e734d50e70a8c41c285193922938bd0d3bc2ee56764d181d55ef5",
}


def selected(component, parts):
    top = parts[0]
    if component == LIBAVIF:
        return (top in {"CMakeLists.txt", "LICENSE", "libavif.pc.cmake", "src", "include", "cmake"} or
                parts[:2] == ("third_party", "libyuv") or
                parts in {("contrib", "CMakeLists.txt"), ("contrib", "gdk-pixbuf", "CMakeLists.txt")})
    return (top in {"CMakeLists.txt", "AUTHORS", "CHANGELOG", "LICENSE", "PATENTS", "aom", "aom_dsp",
                    "aom_mem", "aom_ports", "aom_scale", "aom_util", "av1", "cmake", "common", "apps"} or
            parts in {("test", "test.cmake"), ("test", "test_data_util.cmake")})


def bundle(component, original_dir, output_dir):
    original = original_dir / (component + ".tar.gz")
    if hashlib.sha256(original.read_bytes()).hexdigest() != ORIGINAL_SHA256[component]:
        raise ValueError(f"Unexpected upstream archive for {component}")
    path = output_dir / (component + "-decoder-sources.tar.gz")
    with tarfile.open(original, "r:gz") as archive, path.open("xb") as destination:
        with gzip.GzipFile(fileobj=destination, filename="", mode="wb", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.USTAR_FORMAT) as output:
                for source in sorted(archive.getmembers(), key=lambda member: member.name):
                    parts = PurePosixPath(source.name).parts
                    if len(parts) < 2 or parts[0] != component or not selected(component, parts[1:]):
                        continue
                    if source.isdir():
                        continue
                    if not source.isfile() or ".." in parts:
                        raise ValueError(f"Unexpected selected source member: {source.name}")
                    record = tarfile.TarInfo(source.name)
                    record.size = source.size
                    record.mode = 0o644
                    with archive.extractfile(source) as data:
                        output.addfile(record, data)
    print(hashlib.sha256(path.read_bytes()).hexdigest(), path.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    for component in ORIGINAL_SHA256:
        bundle(component, args.upstream, args.output)


if __name__ == "__main__":
    main()
