# File fingerprints

Native file fingerprints stream a chosen file through SHA-256 with bounded
buffer storage, identity/metadata consistency checks, progress and cancellation.
Python and ImGui use the same C++ implementation. Source-file hashing is explicit:
request construction, image opening and ordinary analysis do not hash whole slides.

```python
import qupathxx as q

options = q.FileFingerprintOptions()
options.byte_limit = 1 << 40  # Default: 1 TiB maximum admitted file size.
options.buffer_bytes = 1024 * 1024
job = q.FileFingerprintJob()
job.request(doc.server.source_locator, "file-observation.json", options)
status = job.wait()  # Releases the GIL.
if status.state != "succeeded":
    raise RuntimeError(status.error or status.state)
print(job.result.sha256, job.result.bytes)
```

Use an explicit path for in-memory images or readers without a locator.
`fingerprint_file(path, options)` is the synchronous GIL-releasing equivalent.
`file_fingerprint_json(result)` returns deterministic UTF-8 JSON for that
immutable observation; `save_file_fingerprint(result, path)` atomically writes
it. Python result fields are read-only, retain no open file or Python source
owner, and survive later file changes/removal. No JSON reader is provided yet.

## What is observed

A successful value describes the raw bytes of one opened regular file, including
headers and all its stored pixel/pyramid payload. Its SHA-256 is a standard file
digest, distinct from an analysis's canonical sampled-tile input digest.
Metadata records the requested absolute lexical path, full byte length, filesystem
identity and modification/change tokens, wall-clock start/finish UTC Unix
milliseconds and steady-clock elapsed milliseconds. Clock adjustment can make
wall-clock timestamps non-monotonic; elapsed duration uses a separate clock.

| Field | Meaning |
| --- | --- |
| `sha256`, `bytes` | Lowercase SHA-256 of the complete observed byte stream and its exact length |
| `path` | Absolute UTF-8 requested locator, without resolving aliases during UI capture |
| `identity_scheme`, `identity` | Platform-local file identity; an opaque comparison token |
| `stamp_scheme`, `modified_token`, `change_token` | Platform-specific data-write and metadata-change observations |
| `started_unix_ms`, `finished_unix_ms`, `elapsed_ms` | Observation range and steady-clock duration |

Windows uses the volume serial and 128-bit file ID (`windows.volume-file-id128/1`).
The identity token encodes volume, the first eight ID bytes and the last eight ID
bytes as colon-separated unsigned decimal values; ID chunks use little-endian
byte interpretation. Stamps use native FILETIME ticks (`windows.filetime-100ns/1`),
including last-write and metadata-change time. Unsupported/unusable identities
reject the observation. POSIX code uses device/inode and signed seconds/nanoseconds
for modification/status-change time. These metadata tokens are not portable
content hashes, creation times or globally durable identities.

The worker reads from one handle, checks EOF against the admitted size and then
compares identity, length, modification and change tokens on that same handle
and on a fresh lookup of the requested path. Truncation, growth, replacement and
observable metadata changes reject the candidate. It never returns a partial
digest as a successful result. Windows denies ordinary write/delete sharing
while the hashing handle is open; an already-open incompatible writer causes
failure. POSIX does not provide that mandatory sharing policy.

These checks require a quiescent file for a reliable file-version observation.
They are not an adversarial filesystem proof or transactional snapshot: timestamp
resolution, memory-mapped changes and filesystem behavior can limit concurrent-
change detection. The digest accurately describes bytes read; metadata checks
help reject changes during that read. File/identity checks do not authenticate
who supplied the file or its medical/scientific correctness.

## Relationship to analysis history

This is an explicit observation at hashing time. It does not retroactively prove
that an already-open image reader or an archived analysis consumed that exact
file version. A historical locator may now refer to different content or may no
longer exist. [Captured analysis provenance](analysis-capture-provenance.md) and
sampled input digests remain unchanged. No source-file hash is invented for dense
images, legacy records or unavailable paths.

A multi-file slide may depend on files beside its primary locator. Hashing that
locator alone does not fingerprint all associated data. Dependency enumeration,
association with reader/request generations, native executable/library inventories,
Python/runtime inventories and complete portable replay manifests remain open
work. The file observation is a reusable component for those manifests, not a
complete manifest or replacement for numerical rechecking.

## Limits, worker and file publication

Options admit file sizes from zero to `byte_limit`, defaulting to 1 TiB and
configurable through signed 64-bit maximum. A zero limit admits only empty files.
The worker rejects an oversized file after metadata lookup and before reading
its contents. Buffer size is 64 KiB through 16 MiB, default 1 MiB; allocation is
no larger than the smaller of that buffer and the file's size (one byte for an
empty-file EOF probe). Metadata/path/hash state adds bounded storage; the buffer
limit is not a process RSS or filesystem-cache guarantee.

A request validates/copies options and normalizes input/output paths without
opening the file. One native worker has no backlog. `poll()` reports state,
bytes/total bytes and completion count; `wait()` finishes the current operation.
The job exposes a new immutable `result` only on success and clears it when a
new request is accepted. Previously retained result objects remain unchanged.
Failed/cancelled operations do not increment `completed`. Job destruction requests
cancellation and joins the worker. Control operations belong on the owner thread;
the worker retains only native strings/options/state and no document/reader/Python
ownership.

Cancellation is cooperative at read/hash/publication boundaries. It cannot impose
a hard deadline on a blocked OS/filesystem call. Hash progress counts payload
bytes; metadata and an EOF validation probe are separate checks. An optional JSON
output uses bounded atomic sibling-file replacement. Failure or cancellation
before replacement preserves the old destination and removes the temporary file.
Cancellation after a completed replacement reports success.

The input path, current aliases and aliases of the observed identity are rejected
as JSON destinations. Checks run before file reading, before output creation and
before publication. Results saved later still refuse to overwrite the original
input pathname or its observed file identity. Ordinary hash operations open the
input read-only. JSON output is capped at 1 MiB, sufficient for the bounded
metadata representation.

## Desktop workflow

Open **File checksum**, enter an input or choose **Use image file**, and optionally
enter a JSON destination. **Hash file (Ctrl+Shift+F)** runs the native worker.
Progress, terminal errors, the observed path and the full digest are displayed;
the digest is split into two 32-character lines for the narrow sidebar.
**Cancel file hash** or **Ctrl+Alt+Shift+F** cancels an active operation.
The selected annotation and project dirty state are not changed by hashing.

`GuiOptions.file_fingerprint_path`, `file_fingerprint_output`,
`file_fingerprint_byte_limit` and `file_fingerprint_buffer_bytes` configure this
workflow. Replay provides `FINGERPRINT_FILE_KEY`, `CANCEL_FINGERPRINT_FILE_KEY`
and the `FILE_FINGERPRINT_READY` success gate. Finite GUI runs wait for a durable
result; interactive window closure requests cancellation before joining.
`GuiReport.file_fingerprint_state`, `file_fingerprint_error`,
`file_fingerprint_result`, `file_fingerprint_bytes` and `file_fingerprint_ms`
report the outcome.

## Validation and reproduction

Tests compare native output against independent `hashlib` digests for empty,
small and chunk-boundary inputs, verify Unicode JSON, immutable observations,
limits, worker progress, GIL release, cancellation/reuse, file-type failures,
writer conflicts, hardlink/renamed aliases, input preservation, atomic replacement,
subprocess lifetime and desktop success/failure without a selected annotation.
Native suites run the file API with optional geometry/readers/storage disabled.

```powershell
python tools/benchmark_file_fingerprint.py --mib 256 --output build/file-fingerprint.json
python tools/benchmark_file_fingerprint.py --mib 1024 --objects 100000 --gui --output build/file-fingerprint-view.json
```

The benchmark generates its source with bounded writes, verifies every digest
against an independent streaming generator and measures warm native hashing,
worker submit/poll latency and optional cached dense-scene rendering. It does not
establish cold-storage, network-filesystem or vendor-bundle throughput.

Windows metadata interpretation follows Microsoft's
[FILE_ID_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_id_info)
and [FILE_BASIC_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_basic_info)
contracts. The POSIX open path uses nonblocking opening before regular-file
validation, following the [POSIX open contract](https://pubs.opengroup.org/onlinepubs/9799919799/functions/open.html).
Platform implementation outside the verified Windows builds still needs delivery
and runtime validation under M7.


The final installed Windows GUI wheel passes 984 Python cases, including 24 new
file/desktop cases, with two file-symlink cases skipped (the junction passes).
Native full/core/minimal suites pass 28/27/27. Visual review confirms readable
input/output controls, terminal status, full observed path and all 64 digest
characters. A desktop cancellation check stopped a 256 MiB read after 65 MiB and
preserved the old JSON destination.

The [256 MiB recording](benchmarks/file-fingerprint-256-windows.json) measures
176.43 ms native observation time (1451 MiB/s), 0.078 ms submission and 0.035 ms
maximum sampled poll. The [1 GiB recording](benchmarks/file-fingerprint-view-1024-windows.json)
measures 658.47 ms (1555 MiB/s), 0.074 ms submission and 0.091 ms maximum poll.
During hashing, its 100000-object selected/all-visible viewer records 16.88 ms
p95 and 17.43 ms maximum CPU frame work; the background hash/JSON operation takes
786.91 ms alongside rendering. The native read buffer remains 1 MiB. These warm
synthetic observations are scoped to one Windows PC and filesystem.
