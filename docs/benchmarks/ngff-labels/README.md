# Selected native NGFF label-mask workflow

Candidate2 passes the selected native import, original-raw measurement,
publication, persistence, replay, CSV and lossless QPLM1 workflow. The
[workflow guide](../../label-analysis.md#open-ngff-label-masks) gives the CLI route;
the [optional provider](../../../plugins/ngff_labels/README.md) declares its decoder
profile. This is A08/R15, not general NGFF, Zarr3, QuPath, biological, speed or
platform acceptance. The broader formats AND application goal remains open.

The provider preserves original property-object spans, including precise decimal
and finite wide-integer text. Integer-mask fills reject implicit float conversion.
Candidate1's passing products and source are retained separately; its qualification
is not transferred to the changed module. Matching source/material/deployment scope
is recorded in the consolidated [delivery receipt](../../../build/gui-aggregation/catalogue-closure/ngff-labels/results.json).

## Selected contract

`org.qupathxx.ngff-labels` uses contract `ngff04-zarr2-integral-labels/1` through
the existing native image-plugin ABI and separate STORE/LABEL_DESCRIPTION
extensions. The provider reuses the matching native OME-Zarr decoder; shared C++
services own frame checks, exact code normalization, measurements, contours,
publication, persistence and exports. Python is optional. No model is executed.

The original level-zero lattice accepts all eight integral sample types:
UInt8/16/32/64 and Int8/16/32/64. Source IDs retain their exact mathematical values,
including INT64_MIN, UINT64_MAX and adjacent integers above 2^53. Zero is
background; distinct nonzero codes become dense native IDs in increasing integer
order. Disconnected parts of one source ID remain one multipart instance, and
background holes remain holes. Declared properties and RGBA are metadata; they
do not create classes. Raw measurements use original target-image channels.

The selected decoder profile handles uncompressed, zlib and gzip chunks,
including partial edge chunks. Label-array `fill_value` is an in-range JSON
integer or `null`; floating tokens such as `0.0` and decimal-to-integer fill
conversion refuse. A missing chunk with null fill refuses instead of supplying
background. Axes follow the admitted ordered subset of
`t,c,z,y,x`; labels have one channel, while reference images may have multiple
channels. The caller selects a source Z/T plane and level 0. For `labels/cells`,
`image-label.source.image: "../../"`, or its recorded normative default, links
to the store root. The explicit expected reference group must agree; CLI `.`
means the root. References remain within the same local store. Remote URLs,
resampling, automatic registration and inferred class/plane mappings are outside
this profile.

Native capture binds the target image, ROI crop, raw channel block/plane,
calibration and original raw samples to the declared input SHA-256. Reference
full dimensions and effective XY calibration must match that target. Present
reference Z/T axes also require matching native spacing/time calibration. Source
and reference units convert only through the admitted physical unit vocabulary.
Equal XY sampling/calibration uses relative tolerance `1e-12`; integer XY offsets
and selected Z/T coordinates use `1e-9` reference-index tolerance. The complete
crop must lie inside the label array, without padding. A 2D label may explicitly
attach to a declared higher reference plane; an absent axis never chooses it.
These geometry tolerances do not relax exact stored IDs, original metadata,
source/store observations, decoder/model identity or replay checks.

Authored masks require an author and retain no fabricated producer/model identity.
Producer masks require producer, version and exact model/recipe SHA-256. Both
origins require the native target-input SHA-256. Import records retain original
attribute text, exact source-code maps and decoder/source identities. Published
objects retain original IDs, properties, RGBA metadata and attribution; CSV
preserves those IDs as decimal text. NGFF raster export is the separate QPLM1
bundle: dense raster, exact code map, source description, target frame/plane and
identity context. Legacy TIFF/PNG records, CSV and QPL1 bytes keep their existing
contracts.

Each original property object is retained as raw JSON text, including whitespace,
member order, long decimal tokens and finite unknown integer tokens beyond the
semantic Int64/UInt64 label-ID domain. These opaque property tokens are not
converted to measurement values. Semantic IDs, plane indices and transforms
retain their own exact ranges and validation; unknown metadata does not relax
those contracts.

Whole-store currentness observes recursive regular-file membership and native file versions, including
non-primary chunks and unrelated members:
additions, deletions or changes invalidate keep, publication and export. Native
project save/save_new, CSV and bundle destinations anywhere inside that protected
mask store refuse without replacing owner files. Projects retain readable metadata
without the optional provider and can be saved elsewhere; exact replay/export
requires the captured provider and unchanged inputs.
The observed digest is not a content fingerprint or a hash of empty-directory timestamps.

## Local evidence and compiled scope

Evidence is under `build/gui-aggregation/catalogue-closure/ngff-labels/`:

| Receipt | Decisive scope |
| --- | --- |
| `integration-01/results.json` | Preserved unchanged controls pass: exact frame/descriptor/QPLM1 tests, ABI description admission/fault controls, actual optional-provider workflow, missing-provider metadata/replay, standalone verify/export and retained TIFF/PNG identities. Independently authored native stores cover all eight types, extremes, zero-only masks, repeated/disconnected IDs, codecs, selected planes, shifted crops, original raw means, contours, undo, persistence, tampering and protected outputs. |
| `standalone-02/results.json` | Candidate2: twelve fresh-process creation/input/verify/export/protection/missing-provider routes pass. Native and CLI record IDs, CSV and QPLM1 bytes agree. Processes use the recorded native-only environment. |
| `integration-02/results.json` | Candidate2 generic parser, exact opaque properties, integer/null fill, primary and host-decoder-OFF actual workflow checks pass. Unchanged controls are reused only after exact binary checks. The same actual workflow assertions pass with the host built-in NGFF decoder linked to OFF stubs. The optional provider retains its matching native SDK decoder. This is a selected boundary check, not a general registry/platform qualification. |
| `sdk-consumer-02/results.json` | Public SDK consumer binds the new module/manifest and unchanged addon library; exact native CSV/QPLM1 and property tokens agree. |
| `foundation-results.json` | Prerequisite tests/products retained. Its header/context ZIP is explicitly partial; the corrected final collector includes translation units and headers. No complete old source snapshot or full NGFF delivery is inferred. |
| `package-final-admission.json` and `zip-admission.json` | All364 matching source/native/plugin/notice/package and ZIP members pass exact material admission. Candidate2 ZIP SHA-256 is `4c47cac415212d30505fa671723671ac4ac21076f7b7bb7414c58fa2279fa830`. |
| `deployment-01/results.json` | Sixteen installed native CLI/SDK routes preserve exact record/CSV/QPLM1/property tokens. All364 owned members are removed after path/process/hash checks;33 user projects/results/notes/logs remain. Saved metadata reopens provider-free; unavailable replay refuses. |
| `sdk-package-configure.json` | The staged relocatable SDK config discovers its exact addon/core and header paths. The already-built source-identical public consumer is reused; no additional native build. |
| `source-availability.json` and `retention.json` | Exact273 compiled inputs/50 products, five scientific objects,25 original source snapshots, nine prior archives, four foundation/candidate1 archives,44 prior inventory groups and A07 primary/offline routes are checked. Separate frozen core/full source and fifteen dependency source archives remain available with their original terms. |
| `candidate2/results.json` | All50 candidate2 test/products are independently frozen before future cache reuse. This private retention archive is separate from the admitted distributable source/native/plugin ZIP. |

The local compiled context is Windows x64 Release, C++17, Visual Studio 2022/MSVC
toolset 14.44.35207, using the retained matching native SDK/runtime and cached
affected targets. The active products are in the reused
`build/gui-aggregation/catalogue-closure/ome-roi/cache/Release/` directory. The
frozen core, existing SDK/wheels, Python bindings and desktop UI are not rebuilt
or given new coverage by these checks. Original target data and authored fixture
bytes remain separate from exported results.

## SDK and local handoff

The public add-on consumer contract is `find_package(qpx_ngff_labels CONFIG REQUIRED)`
with target `qupathxx::ngff_labels` above the matching frozen SDK. Its example
source destination is `examples/ngff_label_plugin`. This add-on exposes the same
typed native workflow; it does not move application logic into Python.

The local candidate2 handoff is complete. The
[source/native/plugin ZIP](../../../build/gui-aggregation/catalogue-closure/ngff-labels/qupathxx-ngff-labels-candidate2-windows-x64.zip)
contains364 admitted owned members, notices, public SDK addon, CLI/provider,
matching sources and rebuild guidance. The exact frozen FP04 SDK/core and its
full first-party/dependency source products remain separately required and
identified; they are not relabeled or newly qualified by this packet. Component
licenses, notices and source obligations remain attached to actual package
members. The OpenSlide binary's LGPL-2.1-only terms remain distinct from an older
aggregate source-inventory or-later label. The generated external SDK ABI header
is described through its exact template/recipe derivation rather than copied
around an invalid older inventory ancestry.

Keep the executable, manifest, provider and exact runtime DLLs together in
`bin/`. The package README describes the matching compiler/CRT profile, SDK use,
source rebuild and owned-file removal. Install/run/removal is local Windows
evidence; no publication or broader platform qualification is claimed.
Only locally authored test stores/images are used here; no upstream NGFF dataset,
model, restricted asset or vendor-format admission follows from this result.

Other qualified formats and application/integration gaps remain in the existing
catalogue. This label provider rejects floating integral fills before opening.
The separate [I22 native source candidate](../ngff-fill/README.md) now corrects
precision-losing fills in the image authority while retaining exact valid image
inputs. Frozen image/label delivery binaries and this packet's original evidence
remain unchanged; no SDK/kernel rebuild belongs to this closeout.
