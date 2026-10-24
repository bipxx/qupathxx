# Native macOS relocation planning and staging

`macos_relocation_plan.py` derives a deterministic package plan from a validated
[native/SDK snapshot](macos-native-inventory.md). Source roots are explicit,
canonical, nonoverlapping POSIX paths for `native`, `geos` and `opencv`. They bind
source-location references; the planner does not simulate the original dyld
search stack or read files at those logical locations.

Captured install identities select providers. Explicit SDK paths and loader/
executable-relative references can select captured source locations; unambiguous
RPATH aliases are also supported. Byte-identical dylib aliases coalesce into one
package library. Colliding target names (including case collisions), ambiguous
providers, missing dependencies of any load kind (including weak), non-two-level
or flat namespaces, environment/subimage loading and unsupported interpreters
fail. System runtime declarations use an explicit allowlist. The provider's
declared current version must satisfy the dependency's declared compatibility
floor; that metadata rule does not establish symbol/ABI availability.

The planned layout has two CLIs under `bin/`, canonical dylibs under `lib/` and
unchanged notices under `licenses/`. Native references become
`@executable_path/../lib/NAME`; dylib references become `@loader_path/NAME`.
Install identities use `@rpath/NAME`, while all source RPATH commands are removed;
package dependency loads use the direct paths above. Dependency order, load kind,
versions and timestamps remain unchanged. Root-specific closures and declared
minimum macOS metadata are retained, without older-OS or system-runtime support
claims. Lookup uses indexes and each root traverses only outgoing edges.

These path substitutions follow Apple's
[dyld documentation](https://github.com/apple-oss-distributions/dyld/blob/main/doc/man/man1/dyld.1).
The planner writes only a new JSON plan and never changes package bytes.

## Staged execution

`macos_relocate.py` requires the requested native macOS host. It recomputes the
plan, copies hash-checked payloads into a new work directory and invokes Apple's
`install_name_tool` on those copies. Raw arguments, exit codes and stdout/stderr
logs are retained. Existing evidence is never replaced and original snapshot
bytes must remain unchanged. Tool failures retain the stage/logs and do not
publish a completed package.

The executor applies ad-hoc signatures and strict builder-side integrity checks
after editing, then binds the verified binary hashes into the archived payload.
Ad-hoc signing does not authenticate a publisher and `signing_verified` remains
false. Every edited binary is reinspected: architecture, kind, flags, minimum
OS/SDK, UUID, interpreter/subimage metadata and remaining load-command order must
match; only planned identity/dependency names, removed RPATHs and signature
commands may differ. Notices retain their source hashes. Complete archive file
cohorts, modes, sizes and hashes are checked before atomic new-file publication.

Example commands on a trusted native Mac builder:

```text
python tools/ci/macos_relocation_plan.py --archive macos-native-files.zip --architecture arm64 --source-roots roots.json --output new-plan.json
python tools/ci/macos_relocate.py --archive macos-native-files.zip --architecture arm64 --source-roots roots.json --work new-relocation-work
```

`roots.json` contains the three original source roots. Libraries are selected
only from captured archive bytes; these paths do not authorize host-file copies.
Component bounds inherit the snapshot/reader quotas, add 100,000 dependency edges
and use the existing 64 MiB canonical JSON limit. They are not a process-memory
cap or a guarantee that every bounded input fits Apple tool argument/header limits.

## Evidence and remaining work

All 313 focused cases pass, including 34 planner and 11 verifier/orchestration
cases. They cover alias/identity/location selection, all dependency kinds,
version floors, ambiguity, cycles, source immutability, protected metadata,
archive cohorts, failed publication, environment cleanup and retained tool
failures. Apple-tool orchestration tests use labelled simulated commands; no
Mac binary executes in the local Windows tests. The shared frozen Python cohort
now includes all seven classes for future installed validation.

The planner also accepts both actual qupathxx snapshots from accepted Mac source
`82c20ff`. Each architecture plans 11 files/four binaries from 14 captured files/
seven binary records and retains 13 dependency/interpreter edges. All notices
remain; three byte-identical runtime aliases coalesce. The
[local receipt](benchmarks/macos-relocation-local-windows.json) retains real-input
plan observations and explicitly distinguishes them from actual relocation.

Candidate 152 connects actual Apple editing/signature execution, source-bound
archive CI audits and [fresh native region/batch/GEOS runtime/image-mapping probes](macos-relocated-runtime.md).
Its new exact-source hosted qualification remains pending; this local planning
receipt does not accept actual relocation.
Python/GUI/OpenSlide macOS packaging, runtime/ABI availability, installers/
distribution signing, license, hardware and scientific/QuPath acceptance remain
open. Full M0-M7 completion is not claimed.
