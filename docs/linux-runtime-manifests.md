# Linux wheel file and static runtime manifests

`tools/ci/linux_runtime_manifest.py` inventories every regular file in supported
headless Linux x86-64 wheels. Each file records its byte count, SHA-256 and ZIP
permission bits. The archive itself is hashed before and after inspection;
file-handle/path versions must each remain stable. Names are case sensitive.
The [ELF reader](elf-runtime-inventory.md) inventories every ELF image regardless
of filename, plus all expected native-library and entry-point names.

The initial filename policy accepts matching CPython ABI tags and
`linux_x86_64` or `manylinux_2_28_x86_64`. The extension and both native CLIs must
exist with the expected image identities; CLIs must carry executable mode bits.
These recorded bits do not establish installed permissions. Bundled system or
CPython runtimes are rejected by filename or SONAME.

## Dependency policy

Bundled dependencies resolve by exact, case-sensitive filename through each
image's direct origin-relative RUNPATH. `$ORIGIN` and `${ORIGIN}` are supported,
including normalized internal parent/subdirectory references. Escape from the
wheel root, absolute paths, empty/current-directory search components and other
environment substitutions fail. Search order is preserved, and a malformed first
candidate cannot silently fall through to another copy. RPATH is retained in the
binary inventory; RUNPATH takes precedence when both exist. RPATH-only inherited
search, filter/auxiliary/audit loading and path-bearing NEEDED entries require
separate reviewed policies and fail in this packaging contract.

The system-runtime allowlist is explicit in `SYSTEM`. It declares glibc,
libstdc++, libgcc, zlib and interpreter prerequisites; it does not prove their
availability or a Linux distribution/ABI support matrix. GNU version requirements
are attached to the exact NEEDED edges, retaining names, hashes, flags and
indices. Required-export availability is not inferred. Cyclic bundled graphs
are traversed through a source-edge index. Each entry point gets its own closure,
and both native executables must remain transitively Python independent.

## Bounds and publication

The archive limits are 10,000 files, 256 MiB/file, 512 MiB expanded, 1 GiB archive
bytes, 4 MiB entry names, 100,000 dependency edges, 32 MiB aggregate ELF metadata
and 64 MiB output JSON, plus the ELF reader's component quotas. These are not a
process-memory cap. Noncanonical/duplicate paths, parent-file collisions,
symlinks, encrypted entries, foreign binaries, malformed ELF and unresolved
dependencies fail. File hashes include Python metadata, notices and ordinary
data files. No package binaries are extracted or loaded.

Generate a new manifest with:

```text
python tools/ci/linux_runtime_manifest.py --wheel PACKAGE.whl --output NEW.json
python tools/ci/linux_runtime_manifest.py --wheel PACKAGE.whl --verify NEW.json
```

Publication uses the existing same-directory temporary-file/fsync/new-link
mechanism and never overwrites an earlier output or the wheel. Verification
recomputes the complete canonical JSON and rejects duplicate keys or altered
numeric/Boolean types. Hashes identify content and do not authenticate publishers.

## CI and evidence

The Linux engine job generates and verifies the wheel manifest before installed
validation. Portable builders additionally generate and verify the qualified
wheel manifest. Fresh consumers verify the downloaded builder record and
generate their own manifest. Existing relocation and manylinux ABI checks remain
separate requirements.

Both artifact auditors have a `--linux-runtime-manifest` gate, which implies the
TMA cohort and earlier engineering gates. The engine audit also retains the
separate Windows manifest flag. Source-bound reinspection checks the actual
uploaded wheel and exact Git bytes for the inventory/parser/CI tools, workflow
and frozen 180-case contract. Every one of the 55 Windows manifest, 75 ELF and
50 Linux manifest cases must pass in each required Python cohort; missing,
renamed, duplicated, failed and skipped cases reject acceptance. Portable
builder/consumer manifest bytes must agree.

Local validation passes all 180 cases and
[28 altered-evidence controls](benchmarks/linux-runtime-manifest-controls.json).
The three previously accepted Linux wheels each inventory 46 files/six binaries;
their binary records agree with the independent ELF oracle and all CLI
verifications/source-bound helper positives pass. The
[local receipt](benchmarks/linux-runtime-manifest-local-windows.json) distinguishes
new tooling from input packages compiled at accepted source `15ee0e8`. Native
compiler inputs are unchanged. New exact-source hosted qualification remains
pending. This does not accept arbitrary loader behavior, process memory,
hardware, Mach-O, installers/signing, license or a supported release; M0-M7
remain partial.
