# Windows package file and static runtime manifests (candidate 145)

The verifier inventories every file in a Windows qupathxx wheel or standalone
candidate ZIP without extracting or loading package binaries. It records byte
sizes, SHA-256 identities, PE32+ x64 dependencies and the dependency closure of
the native entry points. This is one M7 component. OS/runtime availability,
dynamic loading, ABI behavior, other platforms, signing, the project license
and supported releases remain separate requirements.

## Usage

```powershell
python -I tools/ci/runtime_manifest.py --wheel qupathxx-0.1.0-cp312-cp312-win_amd64.whl --gui --output wheel-manifest.json
python -I tools/ci/runtime_manifest.py --wheel qupathxx-0.1.0-cp312-cp312-win_amd64.whl --gui --verify wheel-manifest.json
python -I tools/ci/runtime_manifest.py --native-archive qupathxx-windows-x64-candidate.zip --gui --output archive-manifest.json
```

Omit `--gui` for a headless wheel or native archive. Archive layout and required
entry points are checked for the selected mode. A native archive admits neither
Python extensions nor an interpreter dependency. A wheel's extension must use
the CPython runtime identified by its ABI tag. Existing output files and package
artifacts are preserved. A new manifest is published using a same-directory
hard link after a complete write and flush; filesystems without this operation
fail explicitly. Verification recomputes the complete inventory and graph.

## Recorded evidence and limits

The `qpx.windows-package-manifest/1` model includes the archive hash, all bundled
file hashes, native image architecture, ordinary/delayed imports and export
forwarders. Every DLL reference must resolve to a bundled sibling or an explicit
external prerequisite. The native GUI and CLI dependency closures must exclude
Python. External Windows/API-set, CPython-host and Microsoft C++ runtime entries
are declarations: the manifest does not assert that they are installed.

The reader follows the [Microsoft PE/COFF layouts](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
It bounds header/section/RVA access and dependency tables and rejects malformed
or unsupported data. Archive paths follow a conservative subset of
[Windows naming rules](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file),
including reserved names, alternate streams, separator aliases and case
collisions. Symlinks and encrypted entries are rejected. Ordinary data files
are also hashed, including notices and Python metadata.

Default limits are 10,000 files, 256 MiB per file, 512 MiB of expanded files,
1 GiB of archive bytes, 4 MiB of entry names, 100,000 dependency edges and
64 MiB of manifest JSON. These are component quotas, not a process-memory cap.
The open archive is hashed before and after inspection. Path and handle file
versions are each compared with their own initial observation, since Windows
may report different ctime meanings through those APIs. No source is modified.

Manifest JSON is deterministic for the same archive bytes and policy. Duplicate
JSON keys and changes to numeric/Boolean types are rejected during verification.
Hashes identify bytes; they do not authenticate their publisher. Static graph
coverage does not prove arbitrary Windows-loader or exported-function validity.

## CI integration and validation status

Windows GUI/headless jobs generate and verify a wheel manifest. GUI jobs also
generate and verify the standalone ZIP manifest. A separate
`probe_pe_forwarders.py` command compiles only owned fixture C functions, writes
export records into explicitly allocated fixture data, and asks Windows to
resolve the forwarding call. Package binaries are never loaded by that probe.
The new `--windows-runtime-manifest` engine-auditor gate reinspects uploaded
archives, binds the tooling to the dispatched Git source and requires the native
probe record. It implies the inherited TMA cohort and earlier engineering gates.
Its fixture execution and binary digests are CI-generated evidence; fixture
binary bytes are not uploaded.

Local validation passes 54 corruption/layout/dependency/publication tests. The
current accepted cohort wheel contains 57 files and seven native images; all
131 ordinary import edges agree with MSVC `dumpbin`, and machine headers agree.
The accepted standalone GUI archive contains 48 files and six native images.
Both forms generate and verify successfully. The owned native forwarding probe
returns the independently specified constant 123. Initial metadata/fixture
issues and exploratory fixture builds remain in the ignored build evidence.

The [local validation receipt](benchmarks/windows-runtime-manifest-local-windows.json)
binds the 54 passing cases, actual wheel/ZIP observations and owned native probe.
Twenty-one [altered-evidence controls](benchmarks/windows-runtime-manifest-controls.json)
reject missing files/edges, wrong architecture or mode, altered hashes/results,
stale tooling, JSON changes and a standalone archive paired with a different
otherwise-valid wheel. Both GUI and headless helper positives pass against
previously accepted artifacts. Native compiler inputs are unchanged from the
accepted cohort source. New exact-source CI execution and promotion remain
pending. This candidate does not close M7 or the broader M0-M7 objective.
