# Reader generations and file associations

Native readers expose an immutable cached `ReaderSource`. Analysis requests
retain that exact snapshot in `AnalysisProvenance.reader_source`; results,
archives, project/recovery files and standalone provenance JSON preserve it.
A separate file observation can be compared with this snapshot without changing
the historical analysis. C++, nanobind and ImGui use the same implementation.

```python
import qupathxx as q

project = q.Project()
image = project.add_image("slide.ppm", reader="ppm")
doc = project.document(image)
source = doc.server.reader_source
print(source.generation, source.scope)

job = q.FileFingerprintJob()
job.request_source(source, "source-association.json")
status = job.wait()  # releases the GIL
assert status.state == "succeeded", status.error
association = job.association
print(association.status)

# Compare another observation, including one retained from an earlier run:
association = q.associate_source_file(source, job.result)
q.save_source_file_association(association, "another-association.json")
```

`FileVersion` contains an absolute UTF-8 path, byte count, identity scheme/token,
write/change stamp scheme/tokens and observation time. The native file hash and
reader observation share the same platform implementation. Windows uses the
volume and 128-bit file ID, with FILETIME write/change stamps. POSIX uses device/
inode and nanosecond mtime/ctime. Historical paths accept either Windows or POSIX
absolute syntax so provenance remains readable after moving projects between OSes.

## Generation and scope

Each native reader has an opaque generation distinct from another reader opened
on the same pathname. The generation is a reader-instance identity, not a content
hash. Document copies sharing the same reader retain the same generation; new
readers, reopening a project and relinking create fresh live generations.
Archived analyses retain their original generations, including after rechecking.

Scopes are explicit:

- `memory`: dense in-memory pixels, no file observation or locator.
- `single_file`: the native PPM decoder's complete source-file observation.
- `primary_file`: the supplied OpenSlide file, with no complete dependency claim.
- `unopened`: a project link with cached metadata that has not opened its reader.

External reader implementations default to no descriptor (`None`). Existing
custom C++ readers can override `reader_source()` with a validated native snapshot.

A regular-file guard observes identity, length and write/change stamps before
PPM decoding or OpenSlide initialization, retains that handle during opening,
and compares both the handle and a fresh pathname lookup afterward. Observable
changes fail the open rather than publishing a mixed opening observation. On
Windows the guard denies ordinary write/delete sharing while it is held; an
incompatible existing writer causes opening to fail. POSIX metadata checks do
not establish a mandatory write lock. The guard closes after initialization;
it is not a lifetime lock on the slide.

For native readers, getting the cached descriptor and capturing a request perform no filesystem
inspection or full-file hashing. Lazy project opening does not eagerly decode
slides to populate provenance. A first request can therefore capture `unopened`;
that historical snapshot stays unknown even if its worker subsequently opens the
reader. Later snapshots publish the observed decoder descriptor under the same
wrapper generation. This distinction avoids retroactively inventing an opening
observation. Missing files can still be inspected and their archived records read.

## Meaning of association

`associate_source_file` copies native values and returns:

| Status | Meaning |
| --- | --- |
| `matching_metadata` | Identity, byte count, write stamp and change stamp agree. |
| `version_changed` | Same filesystem identity, different size or stamps. |
| `different_file` | Different filesystem identity, even at the same pathname. |
| `incompatible_scheme` | Identity/stamp schemes cannot be compared. |
| `unobserved` | The reader snapshot has no opening observation. |

A match compares observations. It does **not** prove that an earlier analysis
read every byte of the fingerprinted file, that metadata cannot be forged, or
that a lazy decoder's future reads remain unchanged. Pixel-stream SHA-256 remains
the digest of actual decoded analysis inputs. A later checksum must not be
substituted for that digest. Dense PPM pixels remain the old decoded pixels if
the file is later replaced; association then detects the different file while
the historical pixel result remains intact.

OpenSlide can use additional files. Its official [format inventory](https://openslide.org/formats/)
and [MIRAX](https://openslide.org/formats/mirax/) / [Hamamatsu](https://openslide.org/formats/hamamatsu/)
descriptions document such layouts. This implementation deliberately labels the
observation `primary_file`; it does not enumerate or hash those dependencies.
Complete dependency manifests and decoder-read continuity remain open plan work.

`request_source` hashes the captured descriptor's locator on the existing native
worker and exposes the immutable `association` alongside `result`. A mismatch is
a successful observation, not an I/O failure; inspect the comparison status.
Generic `request(path)` retains its original fingerprint-only JSON and returns
no association. Accepted new work clears previous result/association pointers.
Previously returned values remain independently valid.

With a destination, associated JSON commits on that same worker. Cancellation,
progress, one-worker/no-backlog behavior and input/hardlink/renamed-alias output
protection match [file fingerprints](file-fingerprints.md). Association outputs
also protect the reader locator and its observed native-identity aliases,
including after the original reader file is renamed. Request/poll/control
calls belong on the owner thread. `request_source` copies the descriptor; no
live reader, its open-time guard handle, Python wrapper or document is retained by the job.
The guard's open-time handle is already closed. The fingerprint worker owns its
own temporary input handle until hashing finishes.

## Storage and desktop

The optional reader descriptor is an additive field in provenance JSON schema 1.
New private QPV version-2 blobs encode it with the existing bounded/checksummed
provenance payload. QPV version 1 remains readable with an unknown generation.
SQLite project schema 7 is unchanged. The existing 4 MiB provenance bound includes
the descriptor and file version; the descriptor itself is bounded to 128 KiB.
Project snapshot budgeting also accounts for newly created lazy descriptors.

In **File checksum**, enable **Associate active image** to hash the active image's
cached source locator and export the association JSON. The manual path control
is disabled in this mode. The result shows comparison status and dependency scope.
`GuiOptions.file_fingerprint_associate_source` configures this mode; the returned
`GuiReport.file_fingerprint_association` preserves the native result. Existing
hash/cancel keyboard commands and FILE_FINGERPRINT_READY replay gate still apply.

Generation capture, association JSON and observation snapshots are building
blocks for portable manifests. Native binary/Python/runtime inventories,
complete multi-file dependency discovery and broader numerical/platform work
remain active; this packet does not complete the full project plan.


## Verification and recorded workload

The final installed GUI wheel passes 1028 Python cases with two existing Windows
file-symlink permission skips in 240.06 seconds. The focused source identity,
fingerprint and provenance selection passes 62 cases, including 17 new cases.
All native suites pass in full/core/minimal configurations (30/29/29).
Coverage includes distinct reader generations, removed/replaced/changed files,
immutable snapshots, lazy opening, archived/recovered source descriptors,
QPV1 compatibility, checksummed malformed descriptors, native worker lifetime,
input and renamed-alias preservation, and desktop match/mismatch JSON exports.
A 1440x1100 capture verifies the full checkbox, digest, comparison and scope.
POSIX implementations are present but execution on other operating systems
remains part of M7 validation.

[Raw measurements](benchmarks/source-identity-view-windows.json) were generated by
`tools/benchmark_source_identity.py --mib 128 --objects 100000 --gui`. The source
has a 512x512 RGB raster and trailing padding accepted/ignored by the current
PPM reader, totaling 134217728 bytes. The hash covers that complete file; it is
checked independently against Python hashlib. Both opening paths are warmed
before 20 timed trials. Cached descriptor and analysis capture use 1000 trials.
The direct image endpoint and project endpoint perform different work: project
opening also creates the catalog/reader/document, so their difference does not
isolate the guard overhead. No opening speedup is claimed.

| Measurement | Recorded value |
| --- | ---: |
| Direct PPM image opening p95 | 0.453 ms |
| Observed project image opening p95 | 1.289 ms |
| Cached reader descriptor p95 | 0.0003 ms |
| Analysis request capture p95 | 0.0018 ms |
| Logical descriptor payload | 774 bytes |
| Hash request | 0.066 ms |
| Maximum sampled poll | 0.048 ms |
| Hash plus association JSON on worker | 84.582 ms |
| Association JSON | 1225 bytes |
| Selected / visible objects | 100001 / 100001 |
| Viewer CPU frame p95 / maximum | 16.874 / 17.115 ms |
| Hash plus association JSON in viewer | 93.365 ms |

The RTX 4090 viewer retains 3675040 bytes of overlay cache and 1048576 bytes of
GPU tiles. This is one warm synthetic workload on one Windows PC, with no vendor,
network filesystem or cross-platform latency claim. Exact native binary SHA-256:
`bb8defd0e0f051aa83aabb52ad0a1975513feb4873ac7bc295353683e8896708`; source tree SHA-256:
`8bcef293323426e0ba37c5f315adb9609e66db904aaa450afba27377de14c327`.
