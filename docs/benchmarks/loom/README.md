# Selected native Loom matrix workflow

The M05 workflow is delivered locally for the selected Loom3 profile, including
native, CLI, public SDK, matching source/package and installed run/removal. The
optional `org.qupathxx.loom` provider uses contract
`loom3-dense-typed/1` and separate matrix ABI2. Existing C++ matrix services own
coordinate joins, domain validation, QMX publication, source identity and project
ownership. Python is optional and supplies no reader or scientific fallback.

This is source-scoped Windows x64 MSVC1944 Release `/MD` evidence. All 284 owned
package/ZIP members pass material admission; 12 deployment checks pass separately
from the 34 native/consumer checks below. The broader formats AND
application/integration catalogue remains open;
this workflow does not establish general Loom, platform, biological or speed
acceptance.

## Explicit source selection

Select the matrix and its axes without filename, orientation or attribute-name
inference:

```json
{"matrix":"/matrix","feature_ids":"/row_attrs/ID","observation_ids":"/col_attrs/ID","feature_names":"/row_attrs/Name","domain":"counts","value_domain":"raw-counts"}
```

| Option | Selected contract |
| --- | --- |
| `matrix` | Exactly `/matrix` or one `/layers/NAME`; a selected layer must match the full main-matrix shape |
| `feature_ids` | One explicitly named one-dimensional variable-length UTF-8 dataset directly below `/row_attrs` |
| `observation_ids` | The corresponding explicit dataset below `/col_attrs` |
| `feature_names` | Optional row-attribute string dataset; absent or empty names use the exact source ID |
| `domain` | Required `counts` or `continuous` |
| `value_domain` | Required `raw-counts` for counts; a nonempty explicit label other than `raw-counts` for continuous data |

On disk, rows are features and columns are observations. ABI pages are dense
observation-major, feature-minor. Source-axis order, leading zeros and UTF-8 IDs
remain exact. Both axes contain 1..1,000,000 elements; IDs are nonempty and native
IDs/names are at most 256 bytes. No normalization, numeric-ID conversion or
automatic class assignment occurs. The native feature type is `feature`.

The file requires `/attrs`, `/row_attrs`, `/col_attrs`, `/row_graphs` and
`/col_graphs`, plus scalar variable-length UTF-8 `/attrs/LOOM_SPEC_VERSION`
equal to `3.0.0`. The pinned writer source declares that literal. The older
pinned prose's `v3.0.0` spelling and legacy root-attribute/string conventions
are outside this profile. The reference writer was inspected, not executed.

Only selected datasets are imported. Graphs, unrelated global/row/column
attributes, unselected layers and unselected main-matrix values are omitted.
Their bytes still belong to the whole-file source fingerprint. This reader
does not validate or retain every object in a Loom file.

## Values, coordinates and retained results

The selected numeric types are signed/unsigned 8/16/32/64-bit integers and IEEE
binary16/32/64, in either byte order. Counts retain exact UInt64 values through
`UINT64_MAX`, including adjacent integers above 2^53. Negative, fractional,
nonfinite and out-of-range count values refuse. Continuous data becomes finite
binary64, preserving signed zero and subnormals; integer conversion refuses
whenever binary64 cannot represent the exact integer. The domain label describes
the supplied values; it does not request a transformation or normalization.
Native QMX keeps explicit zeros and its existing missingness contract.

Supply a position TSV with `observation_id`, `x_px` and `y_px`. Native parsing
requires the complete exact observation-ID set, with no missing, extra or
duplicate positions; row order may differ. The CLI derives the selected image's
native level-zero pixel frame. SDK callers supply the explicit native frame.
Source coordinates are retained without registration, scaling or row-position
guessing.

The same native Writer publishes counts and continuous values as new-only QMX
stores. ABI2 uses profile `external-dense-typed/1`, schema5, with distinct
recipe/source hashes binding the selected kind/label, normalized provider options,
whole source SHA, exact module/dependencies, positions and frame. ABI1 counts
keep schema4 and `external-dense-counts/1` unchanged. An exact frozen-CLI baseline
retains QMX SHA-256
`62676e4f0836b58b0c006ce44ef2360498a17579ae9a9937144c1cff58cef6e7`.

Indexed QMX queries, metadata and owned project associations no longer require
the original Loom/positions files or provider. Recapture requires the intended
original inputs and implementation. Removing a provider never substitutes
another reader or makes a changed source current.
Older hosts that do not implement schema5 refuse it; a C-header replacement alone
does not upgrade their persisted-format support.

## CLI and public SDK

Use an absolute manifest path and the matching host. For example:

```powershell
$options = '{"matrix":"/matrix","feature_ids":"/row_attrs/ID","observation_ids":"/col_attrs/ID","feature_names":"/row_attrs/Name","domain":"counts","value_domain":"raw-counts"}'
qupathxx_format_import --format-plugins C:/ABSOLUTE/loom-format-plugins.json matrix-index org.qupathxx.loom C:/DATA/study.loom C:/DATA/positions.tsv C:/DATA/image.tif C:/RESULTS/new.qmx --plugin-options-json $options
qupathxx_format_import matrix-info C:/RESULTS/new.qmx
```

For continuous data, explicitly select the intended matrix/layer, use
`"domain":"continuous"` and its actual value-domain label. For example,
`"matrix":"/layers/log-expression","value_domain":"log1p-expression"`
only describes values already stored that way. The reader does not compute them.

`matrix-info` reads native QMX metadata and the captured receipt without loading
the provider. Outputs are new-only; native failures are visible JSON and use
exit code 2. The six-argument ABI1 `matrix-index` route keeps its previous behavior and
report fields. ABI2 reports additionally identify domain, value-domain and input
profile.

The [public SDK example](../../../examples/loom_matrix_plugin/main.cpp) uses
`find_package(qpx_loom_matrix CONFIG REQUIRED)` and target
`qupathxx::loom_matrix`. Its executable accepts:

```text
loom_matrix_consumer ABS_MANIFEST ABS_PROJECT IMAGE_ID ABS_LOOM ABS_POSITIONS ABS_NEW_QMX OPTIONS_JSON ABS_NEW_PROJECT
```

It derives the same native image frame, forwards explicit provider options,
indexes and opens verified QMX, keeps an owned association, saves a new project
and checks the retained source through native services. Existing owned retention
creates a managed copy beside an already saved input project; Save As copies it
beside the new project. The input project file remains exact. If a later project
operation fails, a complete indexed output may already exist and is reported;
there is no caller-side rollback or second persistence implementation.

The source-matched addon requires the exact retained A08 application addon
`b0c86a57518c49654359a784f3344a1f15c55e60bf6fa07523ad9a6e7fd94a84`
and frozen FP04 core
`4bf957c9ea6e46294c11b9bfb7d9c2496cf4fca607a9c28ba837768d301eb6bb`.
Those remain separate products. Six affected application units and thin
consumers are new; scientific kernels, the frozen SDK/core, UI, bindings and
wheels were not rebuilt or given new qualification.

## Bounded source handling

The provider streams axis/value pages without retaining complete ID catalogues
or densifying the whole matrix. Independent file and decoder-work allowances
permit streamed files larger than the work buffer; the selected provider accepts
16..256 MiB work and up to 16 GiB input. Native host/storage limits remain separate.

Compact, contiguous and chunked storage, with unique Deflate, shuffle and
Fletcher32 filters, are admitted. Declared decoded chunks are at most 1 MiB;
encoded chunks are bounded by their declared bytes plus 64 KiB. Bounded raw-chunk
validation runs before HDF5 conversion, including compressed string-reference
chunks. A retained Windows read lease denies write/delete and binds the HDF5
handle to that same file; native currentness checks remain independent. Caches,
string allocations and pages are bounded. This is not a process sandbox or a
total-process resource guarantee.

Virtual/external storage, soft/external selected links, unsupported/repeated
filters, nonstandard numeric encodings, invalid UTF-8/IDs and ambiguous options
refuse. ABI2 rejects decoded duplicate JSON keys before canonicalization can
erase them, including manifest defaults. Cancellation polls native work; failed
captures publish no partial QMX. The publication guard's denied-write behavior
and real between-read source changes have separate tests.

## Source-scoped evidence and local handoff

Evidence is under `build/gui-aggregation/catalogue-closure/loom/`:

| Receipt | Proven scope |
| --- | --- |
| [integration03](../../../build/gui-aggregation/catalogue-closure/loom/integration-03/results.json) | Nineteen successful checks: C ABI, actual optional provider, all selected types/codecs/domains, literal numeric/ID/frame expectations, native QMX/project/reference outputs, malformed/refusal/boundary behavior, provider-free and loader-OFF routes, typed callback/file-budget/cancel/currentness controls, duplicate defaults, exact legacy schema4 bytes and ABI1 host/matrix behavior. Its overall controller stopped later because the selected older legacy fixture directory lacked its TIFF; this is not a full passing controller receipt. |
| [integration04](../../../build/gui-aggregation/catalogue-closure/loom/integration-04/results.json) | Fifteen remaining commands pass over byte-matched complete legacy fixtures: seven annotation/import controls, counts/continuous CLI/index/info/existing-output and public SDK/owned-project routes. Native, CLI and SDK QMX bytes/source/frame/receipt identities agree. |
| [legacy baseline](../../../build/gui-aggregation/catalogue-closure/loom/legacy-baseline/results.json) | Frozen A07 CLI and original ABI1 counts module capture the same exact schema4 result before the M05 build. |
| [reference intake](../../../build/gui-aggregation/catalogue-closure/loom/reference-intake.json) | Six pinned source-reference/notice identities and the marker discrepancy, with no upstream writer execution or product Python dependency. |
| [package admission](../../../build/gui-aggregation/catalogue-closure/loom/package-final-admission.json) and [ZIP admission](../../../build/gui-aggregation/catalogue-closure/loom/zip-admission.json) | All 284 source/native/plugin/notice/package and ZIP members pass exact path/material identity checks. The local candidate ZIP has SHA-256 `2c15ce9696a9fa5f19a72fde507384909ea9e166c356e9cb059e05ff9d7e32e3`. |
| [SDK package configuration](../../../build/gui-aggregation/catalogue-closure/loom/sdk-package-configure.json) | The relocated staged public SDK configuration resolves exact headers/library and separate required A08/FP04 companions. The already-compiled matching public consumer is reused without another build. |
| [installed run/removal](../../../build/gui-aggregation/catalogue-closure/loom/deployment-01/results.json) | Nine installed native commands plus three post-removal checks pass. All 284 owned members are removed after path/process/hash checks; 11 user/unowned/managed files remain exact. Retained counts/continuous QMX metadata remains readable, and valid import without the provider refuses. |
| [retention](../../../build/gui-aggregation/catalogue-closure/loom/retention.json) | All 23 before-source snapshots, 15 prior archives, 45 previous inventory groups, 186 observed compiler inputs/16 products and four candidate/failure archives are verified. Only the new `loom_matrix_plugin` inventory group is added. |

The 34 effective process checks qualify this compiled source/profile. The separate
12 deployment checks establish the local install/use/removal outcome. Original
data, prior products, source snapshots and failures remain preserved. The
[consolidated receipt](../../../build/gui-aggregation/catalogue-closure/loom/results.json)
records the complete handoff without widening those scopes.

The [local candidate ZIP](../../../build/gui-aggregation/catalogue-closure/loom/qupathxx-loom-candidate1-windows-x64.zip)
contains 284 owned members and matching source/native/plugin/notices and rebuild
guidance. Keep the executable, manifest, provider and exact runtime DLLs together
in `bin/`. The package
[COMPANIONS.json](../../../build/gui-aggregation/catalogue-closure/loom/package-final/COMPANIONS.json)
identifies the separately required A08 addon, frozen FP04 SDK/core, corresponding
source products and actual HDF5/zlib source archives. Those companions retain
their own identities and obligations. No commit, push, publication, new frozen
product qualification or general platform/full-catalogue acceptance is claimed.

Implementation/helpers retain AGPL-3.0-only. The pinned
[Loom reference](../../../plugins/loom/source-lock.json) at commit
`6177c213ab59110cea1bdaed08ac5a355fc13524` retains BSD-2-Clause terms. The six
reference-code/notice files are provenance inputs, never executed or used as a
runtime implementation; specification prose is referenced but not redistributed.
HDF5 2.2.0, prefixed zlib 1.3.2 and host GEOS/JSON/SQLite/runtime components retain
their actual separate notices, licenses and source obligations. OpenSlide binary
LGPL-2.1-only terms remain distinct from an older aggregate or-later source label.
No upstream dataset, image, model or fixture, restricted derivative or NC
component is admitted by this workflow.
