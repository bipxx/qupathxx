# Optional native NGFF integral-label provider

`org.qupathxx.ngff-labels`, version1.0.0, contract
`ngff04-zarr2-integral-labels/1`. Windows x64, matching MSVC compiler, Release `/MD`.
Its ABI1 table is accompanied by the separate STORE and LABEL_DESCRIPTION
extensions. Capabilities are RGBA | RAW | SOURCE_STORE | LABEL_DESCRIPTION (27).

The module reuses the existing native `OmeZarrSource` decoder and its original
NGFF0.4/Zarr2 null/zlib/gzip profile. It adds label metadata/selection handling;
it does not implement another array, chunk, dtype, transform or compression parser.
The frozen matching native SDK is required to build and link this provider.
Python, Java and network downloads are unnecessary.

Pass an absolute store root and these exact provider-option keys:

```json
{"schema":"qpx.ngff-label-selection/1","label_group":"labels/cells","reference_group":"","level":0,"z":0,"t":0}
```

Group keys are explicit normalized slash-separated paths inside the root; the
empty reference key names the root itself. Level is0. Selected Z/T must exist.
The label group must contain `image-label.version`0.4, one integral channel and
the existing native multiscales metadata. All eight signed/unsigned integral
sample types retain exact raw bits. Reference images may have multiple channels.
Label-array `fill_value` must be a JSON integer token or null; floating tokens
(including `0.0`) refuse before provider open succeeds or any raw read. The
native dtype authority discovers datasets and validates the integer range.
Reference arrays retain their native fill profile.
The label/reference dataset-series counts match the pinned NGFF0.4 requirement.
The projected reader exposes the selected label plane as one channel, one level,
one Z and one T; raw reader request Z/T are0 and map to the selected source plane.

`image-label.source.image` is interpreted inside `image-label`, as in the primary
OME implementation. The pinned specification's normative `../../` default is
applied when absent and recorded as a default, independently of explicit text.
The normalized reference must remain inside the same store and equal the caller's
expected reference group. Root-sibling `source`, external/absolute/escaping/link
references and unsupported source-reference members refuse. No referenced data
outside the store is fetched or read.

The immutable neutral description is `qpx.label-mask-description/1`, with format
`ome-ngff-labels-0.4-zarr2/1`. It includes original label/reference attribute text,
explicit/default source linkage, selected plane, native axes/units/extents/effective
scale/translation, and sorted exact per-label property/color records. Property
IDs are canonical mathematical integer strings, retaining Int64 minimum and UInt64
maximum. Duplicate JSON keys, duplicate color/property codes, floating label IDs,
invalid RGBA, unsupported schema and exhausted bounds refuse. Unseen labels need
no metadata entry; zero metadata is retained. No classes or measurements are
inferred from arbitrary source properties.
Opaque property objects retain their exact source JSON value text, including
large integer/precise decimal/exponent spelling, field order and interior
whitespace. Interpreted label IDs still require Int64/UInt64 integers. Opaque
numbers must remain within the frozen native decoder's accepted finite JSON
parser range; metadata outside that range refuses. Original attribute text is
also preserved independently.

The shared native workflow owns target reference/frame/plane/crop checks, exact
source-code normalization, measurements, contours, publication, persistence and
replay. This provider does not resample or register labels. RGBA is a derived
preview: zero/out-of-bounds is transparent, declared colors retain their alpha,
and uncolored nonzero codes receive deterministic colors. Raw samples are separate.

Resource profile: private decoder work16..256MiB; individual metadata files<=24576
UTF-8 bytes, JSON depth32/nodes16384/object members16384, object keys<=256 UTF-8
bytes, numeric tokens<=1024 bytes, complete description<=65536 bytes. The shared
lexical parser uses an8MiB fixed PMR arena with no upstream allocations, charged
alongside live JSON/metadata/property storage and retired before native source
allocation. Existing annotation callers retain their64-member default. Both native
source inventories, their re-observations, JSON/path/worker scratch, metadata and
raw intermediate/chunk storage are reserved. Full metadata refuses rather than
being truncated. Native operations run in joined workers; the synchronous calling
thread polls the C cancellation callback and forwards only an atomic cancellation
flag. No worker, callback, caller pointer or output buffer survives a call.
The unchanged ABI1 level/channel-name/RGBA methods carry no cancellation callback;
level/names read cached metadata, and derived RGBA decoding is bounded/synchronous.

Build with the matching ReaderPluginKit, frozen native SDK prefix and admitted
nlohmann-json3.12.0 header tree:

```powershell
cmake -S source/plugins/ngff_labels -B cache -G "Visual Studio 17 2022" -A x64 -T version=14.44.35207 `
  -DCMAKE_PREFIX_PATH=C:/matching-sdk `
  -DQUPATHXX_READER_PLUGIN_KIT_PREFIX=C:/matching-kit `
  -DQUPATHXX_NGFF_LABELS_JSON_INCLUDE=C:/admitted-json/include
cmake --build cache --config Release --target qpx_ngff_labels_plugin
```

The unique manifest is `ngff-label-reader-plugins.json`. A provider-local writer
records only graph-reachable matching SDK runtime DLLs and refuses to replace an
existing DLL with different bytes. The host executable and those exact DLLs must
share the admitted runtime paths; duplicate-path preloaded dependencies are refused
by the existing loader. Packaging must include actual component notices and source
availability for the native SDK/runtime and JSON, without relabeling their terms.

New provider/adapter code is AGPL-3.0-only. The NGFF specification and OME reference
implementation are semantic references, not shipped decoder dependencies or data.
No upstream images, example datasets, model assets or restricted derivatives are
included. This source is not evidence of completed workflow or platform qualification.
