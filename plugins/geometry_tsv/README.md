# Native pixel geometry TSV plugin

`org.qupathxx.geometry-tsv` version1.0.0 implements
`pixel-geometry-annotation-tsv/1` through the separate annotation ABI2 entry in
[the typed C contract](../../cpp/include/qupathxx/format_plugin.h). The schema,
independently authored C++ encoder, shared TSV helper, fixtures and kit retain
[AGPL-3.0-only](../../LICENSE), including applicable source/notice obligations.
No third-party parser, codec, asset, model, Java or Python runtime is required.
Commercial use follows the actual license terms.

## File contract

UTF-8 TSV has exactly these seven columns, in this order:

```text
id<TAB>kind<TAB>geometry<TAB>c<TAB>z<TAB>t<TAB>class
```

`<TAB>` means one literal tab. LF/CRLF and an unterminated final row are accepted.
There is no CSV quoting/escaping, whitespace trimming, BOM, comment or missing
value syntax. Quotes inside IDs/classes are literal. Fields are bounded to4096
UTF-8 bytes. Empty class is unclassified. Exact unique source IDs remain strings;
the native target additionally requires IDs through256 bytes. Leading zeros,
Unicode and IDs above integer precision remain exact.

| kind | geometry field |
| --- | --- |
| `rectangle` | `x,y,width,height` |
| `ellipse` | `x,y,width,height` |
| `points` | One or more `x,y` pairs separated by `;` |
| `polyline` | At least two `x,y` pairs separated by `;` |
| `polygon` | Shell vertices separated by `;`, then optional hole rings separated by `\|` |
| `multipolygon` | At least two polygon parts separated by `/`; each part uses the polygon grammar |

Numbers must be complete finite nonnegative decimal values, with optional decimal
exponents; leading `+`, hexadecimal, NaN/infinity and surrounding whitespace
refuse. Width/height are positive. Polygon rings need at least three vertices.
The native geometry authority owns topology validation. Shells and holes retain
their roles; invalid or overlapping parts refuse, with no repair. Polygon rings
represent closed boundaries; a matching final vertex is omitted from the native
path. The exact source QRO transport digest remains in the receipt.

Coordinates are level-zero image pixels in the current native image frame.
No scale, affine transform, geographic CRS or registration is inferred. C=-1
means every channel; C>=0 selects a channel and requires a quantitative target.
Z/T are explicit nonnegative indices. The native target validates its actual
planes and image bounds. Acquisition calibration is neither inferred nor changed.

Map each nonempty source class label to an existing native string class key.
The plugin cannot create classes or mutate a Document. It supplies flat native
annotations; hierarchy, measurements and styles are outside this named schema.

## Native import and identity

The provider emits bounded host-owned rows/text and the existing non-executable
QRO1 geometry encoding. Shared native code decodes geometry, checks IDs, planes,
quotas, class mappings, source/module identity and target currentness, then owns
preview/apply, persistence and undo. A successful apply is one atomic append and
returns exact source/native ID mappings. Stale, cancelled or invalid apply leaves
the document and preview unconsumed. Outside-image shapes remain literal in the
preview and refuse native append without clamping or consuming IDs.

ABI2 object receipts, recipe/row/preview identities use version2. They retain the
exact geometry transport SHA, source SHA, module/dependency identity, options,
image frame, source ID/class, mapped class and plane. Saved geometry/receipts can
be reopened without the provider or original TSV. Changed module bytes, including
the same version string, retain a distinct implementation identity. Existing
matrix/point ABI1 layouts, receipt bytes and point preview identities are unchanged.

The module uses bounded full-input parsing with charged input/row/geometry/sort
storage. Native host quotas separately reserve the DLL allowance, bounded pages,
decode scratch and retained objects. Current ABI2 pages contain one object and
at most1MiB of QRO bytes; `max_geometry_bytes` bounds the exact total. The existing
`max_points` option bounds object rows for both ABIs. These logical work limits
are not a whole-process RSS or topology latency guarantee. Cancellation polls
parsing, pages, source checks and staging; native geometry/atomic append retain
their existing synchronous contracts. Directory-store targets require a separate
atomic source-membership contract and currently refuse.

## Build and use

Use the matching annotation ABI2 FormatPluginKit, Windows x64 MSVC1944,
compiler19.44.35225.0, Release `/MD`:

```powershell
cmake -S plugins/geometry_tsv -B BUILD -G "Visual Studio 17 2022" -A x64 `
  "-DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=ABS_KIT"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

Keep `plugins/format-common` beside this source directory. Module builds need no
SDK/core or dependency downloads. The source-matched native host/CLI uses the
existing C++ [preview/apply API](../../cpp/include/qupathxx/format_import.hpp):

```text
qupathxx_format_import --format-plugins ABS_MANIFEST catalog
qupathxx_format_import --format-plugins ABS_MANIFEST annotations-import org.qupathxx.geometry-tsv INPUT.qpproj IMAGE_ID SHAPES.tsv NEW.qpproj --class-map-json CLASS_MAP_JSON
```

`CLASS_MAP_JSON` is a flat string-to-string object; for example
`{"Tumor":"Existing native tumor class"}`. Outputs are new-only, with protected
input/project/image paths. `points-import` remains a compatible command alias.
An older frozen host does not acquire ABI2 by installing a DLL; use the matching
native consumer/source receipt. Modules remain process-pinned: close their users
before removing package files, and preserve images/projects/results/notes.
The [delivery evidence](../../docs/benchmarks/annotation-geometry/README.md)
records exact code/binary identities and the tested local support scope.
