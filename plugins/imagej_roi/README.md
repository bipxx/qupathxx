# ImageJ ROI and ZIP annotation plugin

Independent optional C++ provider `org.qupathxx.imagej-roi` 1.1.0, annotation ABI3,
contract `imagej-pixel-roi/1`. It requires a matching ABI3 host/kit. The earlier
SDK/core/wheels remain separate identities. Windows x64 MSVC1944 Release `/MD`
is the selected build profile; no Java, Python, core or bindings link is needed.

The selected public ImageJ binary versions217–229 carry literal image-pixel
rectangles, ellipses, straight lines, integer/subpixel polygons, free/traced
polygons, polylines, free lines, three-vertex angles and points. Linear composite
contours use the host's shared even-odd topology authority. Native geometry
validation may refuse degeneracy, crossing/touching contours or unrepresentable
topology. Freehand subtype 5 describes a rotated rectangle by two binary32
centerline endpoints and a positive width. It becomes one polygon with four
corners, using ImageJ's binary32 corner and coordinate rounding. The original
parameters and redundant vertex bytes remain source facts. The same profile
works in `.roi` files and ZIP members. Curved composites, splines, rounded
rectangles, arrows, rotated ellipses, text, embedded images and other subtypes
are refused. No clamping, topology
repair, curve flattening or ImageJ pixel-mask/measurement equivalence is claimed.

Source names are independent of stable IDs (`roi` for a standalone source,
`zip:<exact decoded entry name>` for a member). Point sets become individual
points with `#point/<zero-based index>` IDs, retaining category and position.
Groups/counters produce opaque class labels `group/N`, `counter/N` or
`group/N/counter/N`; importing classified objects requires an explicit mapping
to existing native classes. There is no inferred class creation.

The native importer resolves 1-based source C/Z/T, zero meaning unpositioned.
Unpositioned Z/T requires a unique target plane or explicit typed choices.
Legacy stack positions, including point positions, require a named axis.
Source plane declarations survive separately from resolved native planes.
Per-point counter positions supersede ROI-wide legacy positions, including
zero ('all'). A positive per-point flat index combined with positive ROI C/Z/T
constraints refuses rather than guessing a target-specific hyperstack mapping.

UTF-16BE names/properties become validated UTF-8 without normalization. Raw
properties are chunked at Unicode boundaries into4096-byte native metadata
values. Names, original header bytes, source options, ARGB styles, group and
point facts persist in namespaced native properties. Only opaque source stroke
RGB becomes a preferred native object color; native rendering uses its own
style semantics. Source styles remain available for inspection/replay.

ZIP accepts stored and raw Deflate, central/local records, optional data
descriptors, UTF-8/CP437 names and bounded ZIP64 fields. All members must be ROI
files or empty directories. No extraction occurs. Unsafe paths, duplicate names,
CRC/size/header mismatches, trailing compressed data, special files, encryption,
other codecs, multi-disk and unexplained local-record gaps/prefixes are refused.
Raw archive/entry comments, extra fields and headers are preserved as hex facts.
The input is limited to64 MiB, each ROI to8 MiB, coordinate count to65535,
collection to100000 output objects, native geometry to1 MiB/object, and all
native text/property limits still apply. Supplied work reservation256 KiB–256 MiB
owns one fixed parse arena, including zlib's reusable bounded scratch allocation.
These are admission limits, not a timing/RSS guarantee.

Plugin code and shared helpers are AGPL-3.0-only. ImageJ schema/reference evidence
is pinned in `reference-lock.json`; NIH's license declaration is retained in
`licenses/ImageJ-LICENSE.txt`. No ImageJ runtime/code is linked. The private
prefixed static Deflate implementation is exact zlib1.3.2 (`Zlib` license), with
all source members checked by `zlib-source-lock.json`. Retain its license notice
and accurately identify modifications; this build uses unchanged upstream code.
The PKWARE6.3.10 specification is referenced, not redistributed. No proprietary
or noncommercial feature, codec or asset is admitted by these references.

Current source uses the shared annotation producer/value support introduced for
[WKT/WKB](../../docs/benchmarks/wkt-wkb/README.md). That packet records its new
ImageJ module/source identity and regressions. The earlier185-member ImageJ
delivery retains its frozen binaries, sources and exact separate receipt.

Build with a matching FormatPluginKit and admitted local zlib source:

```powershell
cmake -S plugins/imagej_roi -B <cache> -A x64 `
  -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=<ABI3-kit> `
  -DQUPATHXX_IMAGEJ_ZLIB_SOURCE_DIR=<verified-zlib-source>
cmake --build <cache> --config Release --parallel 2
cmake --install <cache> --config Release --prefix <plugin-directory>
```

The selected local profile passes26 authored positive and37 refused profiles,
plus shared ABI3/legacy native controls. See the source-scoped
[current-source receipt](../../docs/benchmarks/wkt-wkb/README.md) and current task for exact
build/deployment evidence. This does not establish general corpus qualification.
