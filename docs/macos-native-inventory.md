# macOS native build-host file snapshots

`tools/ci/macos_native_inventory.py` captures both native CLIs, selected GEOS/
OpenCV SDK dylibs and their notice files into a new ZIP. Each file has byte
count, SHA-256 and snapshot mode metadata. CLI modes normalize to `0755`; SDK
file modes are retained. SDK symlink aliases become separate regular byte copies,
and resolved inputs must stay within their declared roots. Source file handles,
paths, sizes and versions must remain stable while reading.

The completed temporary archive is validated before publication. Limits are
10,000 files, 256 MiB/file, 512 MiB expanded, 1 GiB archive bytes, 4 MiB path
bytes and 32 MiB aggregate Mach-O metadata, plus the reader's quotas. Paths,
entry kinds, duplicate/parent collisions, notices, native entry points, binary
types and each binary's requested architecture are checked. Archive publication
uses a same-directory flushed temporary file and a new hard link. Existing
archives/manifests are never replaced. If a later manifest or oracle step fails,
a complete earlier archive can remain as failure evidence; the audit requires
every record before accepting the snapshot.

This is a build-host evidence snapshot. Absolute SDK paths may remain in load
commands. Static SDK `.a` files remain in the existing build-input digest ledger
and are not runtime snapshot payloads. The archive is not a relocated application
or macOS wheel. Its complete file hashes do not establish signing, installed
permissions, prerequisite availability or dependency resolution.

## Independent load-name checks

The producer invokes `/usr/bin/otool -arch ARCH -L` for every captured Mach-O
file, including SDK aliases. Source bytes must still match the snapshot before
and after each command. Raw logs and a separate probe record retain names and
packed compatibility/current versions. The comparison includes dylib install
identity and preserves repeated entries. It compares multisets because the
reader stores install identity separately from ordered dependency commands.
This oracle does not check RPATH expansion, symbol bindings or load mechanisms;
those parser fields retain their separate fixture/reader validation scope.

The macOS CI job uploads the actual archive, canonical inventory, raw logs and
probe alongside the existing 46-suite native/region evidence. The
`accept_macos_native.py --macho-inventory` gate implies all inherited TMA and
native engineering checks. It binds inventory/reader/producer/helper/workflow
bytes to the dispatched Git commit, regenerates the inventory from the uploaded
archive, matches native hashes to the tested CLIs and compares every SDK runtime/
notice hash with the existing build ledger. It independently parses the uploaded
raw logs and checks their names/versions against the archived binary metadata.
Additional fields or changed scope in the oracle record reject acceptance.

## Current evidence and remaining work

Local validation passes 22 snapshot/oracle-format cases, 66 Mach-O reader cases
and all 180 inherited manifest cases (268 total). The shared frozen Python case
contract now requires all five classes in each new engine/portable Python
cohort. [25 altered-evidence controls](benchmarks/macos-native-inventory-controls.json)
reject missing/changed snapshots, file modes/architectures, tested binary/SDK
identities, raw/parsed oracle records, unsupported paths, scope and source claims.
The local auditor positive uses labelled independent synthetic Mach-O/SDK files
and formatted oracle text; it is not actual macOS or `otool` execution.

The [local receipt](benchmarks/macos-native-inventory-local-windows.json) retains
that scope and the initial nine failing snapshot cases caused by Windows
`fsync` on a read-only descriptor. A writable temporary handle fixes publication;
all final cases pass. Exact-source hosted validation on actual Intel and arm64
qupathxx binaries remains pending. Native compiler inputs are unchanged.

Install-name/origin resolution, relocation without build SDKs, macOS Python/
GUI/OpenSlide packaging, runtime/ABI availability, installers/signing, licensing,
hardware and research acceptance remain open. Full M0-M7 completion is not claimed.
