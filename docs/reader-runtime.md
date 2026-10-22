# Reader runtime validation

The verified Windows x64 and Linux x86_64 development configurations use
OpenSlide binary SDK 4.0.1.2 (library version 4.0.1), from the
[official release](https://github.com/openslide/openslide-bin/releases/tag/v4.0.1.2).
Archive sizes and SHA-256 identities are pinned in `tools/ci/runtime-assets.json`.
The SDK contains its reader dependencies and their notices. Linux wheels bundle
its `libopenslide.so.1` and full SDK license directory alongside the extension
and native executables; their install RUNPATH is `$ORIGIN`. This avoids relying
on the older Ubuntu system reader. GEOS remains an explicit external SDK for
these Linux development wheels. No manylinux portability acceptance is claimed.

Set `QUPATHXX_BUNDLE_OPENSLIDE=ON` and `OpenSlide_ROOT` to the verified
self-contained Linux SDK to reproduce the bundled configuration. Ordinary
native builds can still choose a system reader with bundling disabled; that
does not grant the system runtime the acceptance of the pinned configuration.

The [comparison record](benchmarks/reader-stack-linux.json) contains direct
C API reads on Ubuntu 24.04 WSL x86_64. Both stacks read the same generated
deflate TIFF bytes: complete tiles, partial right/bottom tiles, and natural
levels smaller than a tile. Ubuntu's OpenSlide 3.4.1 returns 128 mismatched
bytes for the 8 x 4 level and 140 for the 9 x 5 level without an OpenSlide
error; complete larger levels are exact. The pinned library returns exact
RGBA bytes for all nine tested levels. This establishes a reader-stack
difference, not which individual dependency caused the old behavior.
The diagnostic one-call timings are not a performance benchmark.

A relocated Linux wheel is also checked through nanobind and its native region
CLI for all nine levels. The CLI runs without `LD_LIBRARY_PATH`. The extension
loads the relocated bundled ELF, verified through `/proc/self/maps` and its
SHA-256 identity. This checks actual runtime resolution, not only CMake linkage.
Local Python 3.14 focused reader/array/thumbnail/cache-integrity checks pass
66 cases with two optional skips; Windows focused checks pass 17 with the
Linux-only symlink case skipped. Windows CTest passes 34, generated stubs match,
and the installed GUI/workflow probe reports OpenSlide 4.0.1.

CI verifies every extracted SDK file against its pinned archive before building,
including Linux symlink targets. Corrupted files, traversal paths and changed
symlinks have regression tests. Installed Linux workflow checks require the
bundled reader to match the SDK and wheel bytes and be the only mapped
OpenSlide library. They do not add the SDK library directory to
`LD_LIBRARY_PATH`. Version checks also reject accidental system-reader use.

Pixel acceptance remains limited to the synthetic corpus and identified Aperio
fixture. More vendors, damaged slides, axes, ICC, hardware/platform campaigns
and the other M1–M7 gates remain open.
