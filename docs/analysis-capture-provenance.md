# Capture-time analysis provenance

ROI analysis, tissue detection and cell detection now retain immutable native
source/build facts from request creation. Changing calibration, closing a reader,
relinking an image, reopening a project or rechecking old pixels does not rewrite
those historical facts. Existing parameters, transforms, ROIs and sampled-pixel
SHA-256 digests remain the numerical replay contract.

```python
request = q.AnalysisRequest(doc, object_id, transform)
result = q.analyze(request)
provenance = result.provenance
print(provenance.effective_pixel_width_um, provenance.environment.compiler)
q.save_provenance(provenance, "capture.json")
```

`AnalysisRequest.provenance`, `AnalysisResult.provenance` and
`TissueResult.provenance` expose read-only `AnalysisProvenance` values. Cell
nucleus/body rasters share the same native provenance object, available through
`result.nuclei.provenance` and `result.cells.provenance`. Python receives copies
of mutable metadata collections. Workers, record snapshots and archived values
retain no Python ownership through this new data.

## Recorded source facts

Each capture records source object/revision/document IDs, nucleus selection and
UTC Unix milliseconds from the wall clock. Timestamps can repeat and are not
canonical revision counters. The source snapshot contains all pyramid levels,
level-zero dimensions, vendor, device RGBA8 pixel contract and original source
calibration. Effective document calibration is recorded separately, in micrometers
per pixel, including explicit unknown values. Later document overrides therefore
do not make old measurements appear calibrated with new values.

Native file readers provide a reader descriptor and original absolute locator.
OpenSlide captures its resolved open path. Native PPM file readers preserve the
PPM descriptor while retaining ordinary owned dense-buffer behavior. Lazy linked
readers report their configured reader/path without forcing a decode at request
capture. Images explicitly constructed as RGB values have `qpx.dense-rgb/1` and
an empty locator. External native server implementations default to `external`
and an empty locator; they may override the optional descriptor methods.
Locators are historical paths and are not rewritten when projects move or relink.
A descriptor is not a whole-file content fingerprint.

## Recorded build facts

`build_environment()` returns immutable native facts shared by captures within
that build: application version, compiler/version, configured flags/configuration,
build target platform, CMake version, enabled GUI/Python/interchange capabilities,
and actual linked GEOS/OpenSlide/SQLite runtime versions when enabled. Empty
dependency versions identify disabled features. These describe the native build,
not a complete inventory of the Python process or operating-system environment.

CMake fingerprints the checked-out source bytes: recursively selected `.cpp` and
`.hpp` files under `cpp/`, `.cmake` and `.in` files under `cmake/`, `.py` files under
`src/`, plus `CMakeLists.txt` and `pyproject.toml`. Relative paths sort lexically;
each entry is `relative_path=lowercase_sha256\n`, and the UTF-8 entry sequence is
hashed with SHA-256. This includes native test source and excludes generated build
files. Input changes trigger reconfiguration and rebuilding the generated native
fingerprint. Python tests independently reconstruct it from actual file bytes.
Line-ending changes can therefore change the fingerprint.

The configured Git HEAD is a navigation hint; `+dirty` marks a non-clean checkout.
It does not identify uncommitted source by itself. The source byte fingerprint
records those actual source inputs even before committing. Runtime library
versions are not binary library hashes, and the build fingerprint is not a hash
of the linked executable. [Standalone file observations](file-fingerprints.md) now provide bounded native
whole-file hashes. Their association with captured reader/request generations,
complete multi-file slide inventories, native dependency/executable fingerprints,
Python package inventories and portable replay manifests remain open work. No environment is inferred from legacy data.

## Storage, validation and rechecking

Schema 7 adds `analysis_provenance`, keyed by image and canonical archive record
ID across ROI/tissue/cell records. A bounded `QPV` version-1 little-endian blob
stores source/build facts with an independent SHA-256 checksum. The loader checks
lengths, format/boolean fields, strict UTF-8 text, finite positive calibration, pyramid metadata,
source extents, record identity, source fingerprint syntax and complete consumption.
Orphan metadata and cross-archive record collisions reject the unpublished
candidate. The checksum detects corruption, not authorship or deliberate rewriting.

Both logical provenance payload and encoded blob are limited to 4 MiB per capture.
Archive and save-snapshot accounting charge this retained payload conservatively,
including repeated aliases/shared environments. Existing global archive quotas
remain in force. Metadata is bounded independently from the analysis pixel-work
budget; allocator and OS overhead are not a process RSS guarantee.

Schemas 1-6 remain readable with `provenance=None`. A transactional save upgrades
them without inventing missing source/build history. Rechecks capture their own
fresh provenance and compare numerical inputs/outputs under the existing archive
contract; they leave the stored provenance unchanged, including legacy `None`.
A successful numeric recheck does not authenticate a captured compiler, timestamp
or locator. Explicit retention of the new result creates a new historical record.
Save and recovery snapshots preserve the captured object even after live removal.

`provenance_json(p)` emits deterministic UTF-8 JSON for that captured value with
schema `qpx.analysis-provenance/1`, exact integer IDs, round-trip finite doubles,
JSON escaping and null unknown calibrations. `save_provenance` releases the GIL
and uses the common atomic file replacement helper, bounded to 16 MiB output.
It exports the provenance component; parameters/input digests still belong to
the associated result/archive. The GUI shows recorded application version and
captured calibration, or explicitly unavailable legacy history.

## Validation and reproduction

Tests cover capture-before-calibration changes, every archive kind, immutable
Python/native lifetime, exact build-source reconstruction, native descriptors,
coarse pyramid metadata, JSON/file output, schema-6 migration/rollback, deliberate
checksum-preserving corruption and failed-write retry. Native tests exercise the
codec and all kernels with optional geometry/readers/storage enabled and disabled.

```powershell
python tools/benchmark_provenance.py --records 100000 --output build/provenance-100000.json
python tools/benchmark_provenance.py --objects 100000 --gui --output build/provenance-view.json
```

The benchmark repeatedly retains one ROI result to measure conservative shared-
archive accounting and serialization. It verifies historical calibration/JSON
through reopen before reporting latency. It is not 100000 independent analyses.

The installed Windows GUI wheel passes 939 Python cases, with two file-symlink
cases skipped because this account lacks permission (the junction case passes).
Native full GUI/core/minimal configurations pass 27/26/26 suites. Desktop tests
exercise stored captures alongside the existing archive inspector; visual review
checks that captured calibration and its units remain readable after live changes.
Recovery replay now waits for the actual reviewed-dialog button before confirming
or testing blocked Undo, rather than assuming background review finishes by a
fixed frame. Timestamp tests allow Windows clock API precision differences.

Recorded on one Windows 11 PC: retaining 100000 aliases takes 146 ms, save capture
0.0245 ms, worker write 878 ms and validated reopen 632 ms. Encoded provenance is
69,700,000 bytes in a 109,051,904-byte project. Warm request capture is 0.0012 ms
p95 with 1130 logical retained bytes per capture. A separate 100001-object
selected/all-visible scene with 1000 records measures 16.89 ms p95 and 17.55 ms
maximum CPU frame work. These synthetic warm observations exclude whole-file
hashing and do not establish vendor-slide throughput or process memory bounds.
Raw evidence: [archive workload](benchmarks/provenance-100000-windows.json) and
[viewer workload](benchmarks/provenance-view-100001-windows.json), with native
binary and selected-source fingerprints.
