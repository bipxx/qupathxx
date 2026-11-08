# Offline Mach-O load-command inventory

`tools/ci/macho_dependencies.py` inventories load declarations in little-endian
Mach-O64 Intel/Apple-silicon executables, dylibs, dynamic linkers and bundles.
Big-endian FAT32 and FAT64 containers preserve each supported slice's order,
CPU type/subtype, bounds and SHA-256. Subtype capability bits are retained.
Layouts follow Apple's
[Mach-O load-command definitions](https://github.com/apple-oss-distributions/xnu/blob/main/EXTERNAL_HEADERS/mach-o/loader.h)
and [universal-container definitions](https://github.com/apple-oss-distributions/cctools/blob/main/include/mach-o/fat.h).

The `inspect_macho(bytes)` result includes ordered ordinary/weak/reexport/lazy/
upward dependencies, dylib identity and packed compatibility/current versions,
RPATHs, interpreter declarations, dyld environment strings, sub-framework/library
declarations, UUID, minimum macOS/SDK versions, build-tool records and command IDs.
Dependency strings preserve UTF-8 bytes through decoding without case folding or
path expansion. Timestamps remain declared values. Nothing is loaded or executed.

Input is capped at 256 MiB. A universal container has at most 16 distinct
CPU/subtype pairs with nonoverlapping, aligned slices outside its header. Each
slice permits at most 8,192 commands and 16 MiB command bytes. Names are capped
at 32 KiB each and 4 MiB per slice. Segment sections are capped at 4,096 and
relocations at one million per section; all referenced file ranges must fit.
Command sizes/counts, string offsets/terminators, supported platforms, file and
virtual segment/section bounds, reserved fields and identity correspondence are
checked. Empty relocation ranges and zero-fill sections follow their distinct
file-backing rules. Unknown required-dyld commands and legacy dependency commands
fail. Optional command IDs remain visible.

Memory views avoid copying entire universal slices. Overlap checks precede
slice parsing/hashing; valid slices together cover at most the bounded input.
These component quotas are not a process-memory cap. This reader does not
validate arbitrary relocation/fixup payloads, symbol bindings, signatures or
dyld semantics. Older 32-bit/PowerPC images, swapped wrappers and other platforms
are outside this initial macOS x86-64/arm64 scope.

## Validation and remaining work

Local validation passes 66 Mach-O cases and all 180 inherited Windows/ELF/Linux
manifest cases. Independent byte fixtures cover all declared dependency kinds,
both universal header widths, both architectures, subtype bits, malformed
headers/commands/segments/sections/names, count and name-expansion budgets,
overlap, architecture mismatch and valid zero-fill/empty-relocation behavior.
The retained first run has 60 passes and two incorrect fixture expectations;
those expectations were corrected with explicit valid-range controls.

`tools/probe_macho_dependencies.py` compares trusted native files or wheel bytes
against optional isolated `macholib`. The local `macholib==1.16.3` comparison
agrees on two pinned MarkupSafe 3.0.2 wheels, containing one thin arm64 image and
one Intel/arm64 universal image (three slices total). It checks architecture,
file identity, slice bounds, dependency names/kinds/versions, dylib identity,
RPATH/interpreter, minimum macOS/SDK versions and command IDs. Timestamp, tool,
UUID and segment-payload fields are covered by fixtures, not this independent
comparison. Inputs are downloaded and checked against the
[PyPI artifact digests](https://pypi.org/pypi/MarkupSafe/3.0.2/json), retained locally
and never extracted or loaded. Their binary bytes are not republished here.
The [local receipt](benchmarks/macho-runtime-reader-local-windows.json) records
the inputs, parser/probe identities, observations and scope.

This binary-reader packet does not produce a macOS runtime package. Candidate
150 adds [native/SDK evidence snapshots](macos-native-inventory.md), source-bound
archive reinspection and actual `otool` comparison to the macOS workflow;
its [actual Intel/arm64 hosted validation](benchmarks/macos-native-inventory-macos.json)
is accepted at `82c20ff`. Origin/install-name resolution and
relocation remain required. The current Mac native
build may embed SDK paths; that existing limitation remains explicit. macOS
wheel/GUI/OpenSlide support, installers/signing, runtime/ABI availability,
hardware, biology and licensing remain separate open gates. M0-M7 remain partial.
