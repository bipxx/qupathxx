# Native matrix and annotation plugins

The FP04 host adds two optional, independent C++ import providers:
[dense counts TSV](../plugins/counts_tsv/README.md) and
[pixel point TSV](../plugins/point_tsv/README.md). Both use the
[typed C ABI](../cpp/include/qupathxx/format_plugin.h). The public
[native services](../cpp/include/qupathxx/format_import.hpp) own validation,
publication, class mapping, persistence and undo. The previously delivered image
ABI remains separate. Use a matching FP04 host/SDK; frozen earlier hosts do not
implement these new entry points.

Configure one absolute `qpx.format-plugins/1` manifest before querying family
providers. Each entry binds domain, provider/format IDs, display name, version,
contract, compiler/CRT profile, module SHA-256 and exact dependency paths/hashes.
Paths stay within the package. The native loader verifies the PE/import graph,
callback ownership and descriptor agreement. A refused entry produces a native
diagnostic; it does not acquire import capabilities. Modules remain pinned for
the process lifetime. Close the host before replacing/removing module files.

The initial modules have no third-party parser or codec dependency. Each module,
its independently authored format definition, helpers and test controls retain
[AGPL-3.0-only](../LICENSE). Commercial use follows those terms, including source
and notice obligations. No NC/research-only, proprietary or unknown material is
admitted by this workflow. Container support does not grant rights to its contents.

## Counts and positions

Select `org.qupathxx.counts-tsv`. The exact header starts with `observation_id`,
followed by unique feature IDs. Every data row contains its unique observation
ID and exactly one unsigned decimal UInt64 per feature. Counts, leading zeros
in IDs, UTF-8 bytes, axis order and explicit zeros remain exact. No floating
conversion, duplicate aggregation, trimming or missing-value inference occurs.

Supply a separate position TSV with exact `observation_id`, `x_px`, `y_px`
columns. The existing native position parser joins by the complete exact ID set;
position row order may differ. Supply an explicit native `CoordinateFrame` in the
SDK; the CLI derives the level-zero pixel frame from its selected image.
This records source coordinates and does not infer registration.

The DLL accepts text fields through4096 UTF-8 bytes; the existing QMX/native
catalog requires IDs/display names through256 bytes. Longer target identifiers
refuse. Feature display names use their source name or exact ID when empty;
feature type is the neutral string `feature`. Observation display names must be
empty because this QMX target has no separate observation-name field.

The native Writer publishes a complete new-only QMX store, profile
`external-dense-counts/1`, schema4. Receipts bind the complete provider/module/
dependency identity, actual normalized options, counts source SHA, position
source SHA/columns and coordinate frame. Legacy schemas1–3 retain their own
identity rules. Once indexed, queries/reopen use the verified QMX store and need
neither the original TSVs nor the provider DLL. Reimporting changed source/module
bytes creates a distinct identity even when provider ID/version are unchanged.

## Point annotations

Select `org.qupathxx.point-tsv`. The exact columns are `id`, `x`, `y`, `c`, `z`,
`t`, `class`. X/Y are finite nonnegative level-zero image pixels; C=-1 means all
channels, and Z/T are explicit nonnegative indices. The native target validates
each plane. Its image-bound geometry contract applies at atomic append;
outside-image points refuse without clamping or partial publication. Source IDs are
unique exact strings, subject to the native256-byte target limit.

Empty class means unclassified. Map each nonempty opaque source class label to
an existing native **string class key**. The provider cannot create classes or
receive a mutable Document. Mixed row planes are permitted when the target
supports them. Coordinates are not rounded, clamped or silently rescaled.

`preview_annotation_plugin` owns reviewable points, receipts and a deterministic
preview identity. `apply_annotation_plugin` checks the original live document,
revision, image server/source/frame, class registry and input currentness. A stale,
cancelled or invalid preview leaves the document/history unchanged. A successful
apply returns exact source-ID/native-ID mappings and appends one undoable batch.
Existing object properties persist per-row receipts through QOP/project save and
reopen; no DLL is required to read the saved result. Preview is consumed only by
a successful apply. Cancellation polls parsing, pages, source checks and staging;
the final native append is an atomic transaction, without a fixed cancellation
latency claim inside that transaction.

The selected target scope is a guarded regular-file/file-set image or immutable
native RGB image. Directory-store targets explicitly refuse until an atomic
membership/chunk guard contract is selected.

## Native CLI and SDK

The CLI uses the same native services:

```text
qupathxx_format_import --format-plugins ABS_MANIFEST catalog
qupathxx_format_import --format-plugins ABS_MANIFEST matrix-index org.qupathxx.counts-tsv COUNTS.tsv POSITIONS.tsv IMAGE OUTPUT.qmx
qupathxx_format_import --format-plugins ABS_MANIFEST points-import org.qupathxx.point-tsv INPUT.qpproj IMAGE_ID POINTS.tsv OUTPUT.qpproj --class-map-json CLASS_MAP_JSON
```

`CLASS_MAP_JSON` is a flat string-to-string object, for example
`{"Tumor":"Existing native tumor class"}`. Project/image IDs are native IDs.
Outputs are new-only; input files and project/image sources are protected against
aliasing. Successful JSON reports include receipts, source identities and, for
points, the source/native mappings. Native failures remain visible. Optional
`--reader-plugins ABS_MANIFEST` configures the separate image-provider catalog.

Plain CMake keeps Python OFF. Enable `QUPATHXX_BUILD_FORMAT_PLUGINS=ON` for the
Windows x64 MSVC Release `/MD` host. Host configuration needs no prebuilt kit.
Install component `FormatPluginKit` for the two C headers, compiler profile and
license. Build either provider separately against that kit, with no core/Python
or dependency downloads. Keep `plugins/format-common` beside each provider's
source directory. The NativeSDK owns the C++ import/receipt APIs and matching
static host services; it is distinct from the standalone C development kit.

The initial providers use bounded full-input parsing, not a scalable out-of-core
claim. The DLL work allowance is64KiB–256MiB and charges its source, decoded
arrays/catalogs and sorting/bookkeeping. Native host limits separately charge
the full assigned DLL allowance, bounded ABI workspace, retained positions/
points and native publication. Headers are bounded to64KiB; unsupported headers,
encodings, options, domains or numeric forms refuse explicitly. LF/CRLF and a
final unterminated row are supported; quotes are literal, not CSV quoting.

The [current task](current-task.md) records validation/delivery state. Additional
scientific formats, matrix stores, annotation geometries, writers, remote IO,
UI and Python bindings remain selected separately under the
[format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14).
