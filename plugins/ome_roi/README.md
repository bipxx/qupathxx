# OME ROI XML native plugin

`org.qupathxx.ome-roi` is an optional native annotation provider with contract
`ome-2016-06-vector-mask-roi/4` in version 1.3.0. It uses the existing
annotation ABI5 host to preserve Mask coverage and Locked state. Use the
matching FormatPluginKit to rebuild the module.

Import OME2016-06 XML `Point`, `Label`, `Line`, `Rectangle`, `Ellipse`, `Polygon`
and `Polyline` shapes as individual native annotations. The provider also
accepts the `Mask` profiles below. Shape IDs are exact opaque
source identities; ROI membership, source image ID, text, styles, original plane
attributes, calibration context and referenced structured annotations are retained
as namespaced immutable source facts. Names prefer shape Text, then ROI Name, then
shape ID. Names and source IDs have separate roles. No classes are inferred.
Shapes within an OME Union remain separately identifiable objects; the importer
does not substitute a Boolean union for those source shapes.

Coordinates mean literal level-zero image-pixel XY, interpreted from finite
decimal text as binary64, matching OME model double APIs. Original numeric text
is retained; the schema's `xsd:float` declaration does not trigger a hidden
binary32 round-trip. Point/path affine transforms use OME's six matrix equations.
A transformed rectangle becomes its exact four-corner polygon. Ellipses accept
identity or diagonal/axis-exchange transforms. General rotated/sheared ellipses
fail explicitly. Output uses binary64 arithmetic and existing
native geometry/topology/bounds authorities, without clipping or repair. Labels
retain an anchor and text/font metadata; no text-layout equivalence is claimed.

TheC/TheZ/TheT are zero-based. Absent axes remain source all/unpositioned values;
the native host requires unique Z/T or explicit native target choices. An explicit
TheC with bundled SamplesPerPixel is refused because the profile has no component
mapping. No automatic registration, scaling or calibration conversion is inferred.
The caller must select a target native frame in the same pixel coordinate system;
an OME image ID alone does not prove registration with a different image.

Locked becomes actual native edit-lock state and participates in annotation identity,
undo and persistence. Full signed RGBA/style attributes are retained. Native's
single preferred RGB is set only for an opaque StrokeColor (or FillColor when
StrokeColor is absent); native rendering does not reproduce OME per-stroke/fill
alpha, widths, fonts or dash arrays. Style length units remain metadata, with no
silent physical-to-pixel conversion.

For ROI-only XML, all ROI definitions are selected. With one source Image, only
its ROIRefs are selected. Multiple Images require explicit native provider
options, for example `{"image_id":"Image:1"}`. To select definitions not referenced
by any Image, use `{"unbound_rois":true}`. Selection counts and mode are retained;
no absent association is guessed. These two selections are mutually exclusive.
The CLI forwards them with `--plugin-options-json`; the public SDK has the same
native option argument. Unknown, mistyped, oversized or unresolved choices fail.

## Binary masks

Select an Image with Pixels dimensions equal to the target's level-zero
canvas. Mask X, Y, Width and Height describe an integral pixel crop. An optional
Transform can combine integral translation with an XY axis swap or reflection
to cover all eight grid-preserving orientations. The complete transformed crop
must fit the canvas and every source cell maps to one target cell. Supply one BinData with
canonical base64 and its encoded character count in Length. Compression may be
`none` (the default), `zlib`, or `bzip2`. A compressed value must contain one
complete stream, with no trailing bytes, that decodes to the exact packed crop
size. The supported packing is continuous row-major MSB-first with zero unused
tail bits.
The declared canvas must fit the native 1 MiB packed geometry limit.
Native mask contouring preserves each covered cell, including holes and
disconnected parts. Mask and BinData attributes remain in the saved source
facts, and the document hash binds the source payload.

XML uses the exact OME2016-06 namespace and XML1.0 UTF-8/UTF-16. Namespaced aliases,
XML-defined whitespace/entity normalization and foreign structured-annotation
payloads are supported. DTDs, declared/external entities, malformed encodings,
duplicate IDs/attributes, unknown selected geometry, unresolved references and
unrepresentable profiles fail before atomic publication. No network/schema fetch
occurs. This is a selected ROI profile, not a general XSD validator or image import.
Acquisition/embedded pixel payloads outside selected ROI/image context are not
interpreted as annotations. Original source SHA binds the entire document.

Source metadata values `QOX2` encode normalized XML names, in-scope namespace
bindings (including inherited/rebound/undeclared prefixes), attributes and ordered
mixed text/element content using UTF-8 byte-length-prefixed fields (`N:value`).
An element stores namespace URI, local name, binding count, sorted prefix/URI
pairs, attribute count, each attribute's
URI/name/value, content count, then `T` plus text or `E` plus a child element.
`QOX2A` stores the same binding fields then attribute count and those triples.
QName-valued metadata retains its prefix context. Chunks preserve UTF-8
boundaries. Prefix spelling, comments, processing instructions and lexical entity/
whitespace encodings are covered by the original source SHA. Native limits are
explicit: 64MiB input, 64 levels, 100,000 XML nodes/shapes, 64 attributes per node,
64 in-scope bindings per node (prefix <=256 bytes, URI <=4096 bytes), 8MiB text,
256KiB metadata subtree, 65,000 polygon/path vertices and 1MiB encoded
geometry per shape, plus caller-owned 256KiB–256MiB work and native metadata quotas.
Expat and all retained parser/source/geometry allocations use the fixed work arena;
chunk and callback cancellation release every private handle.

Build offline with Windows x64 MSVC1944/compiler19.44.35225.0 Release `/MD`:

```powershell
cmake -S source/plugins/ome_roi -B ome-build -A x64 `
  -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit `
  -DQUPATHXX_OME_EXPAT_SOURCE_DIR=source/deps/expat `
  -DQUPATHXX_OME_MASK_ZLIB_ARCHIVE=source/third_party/zlib-1.3.2.tar.gz `
  -DQUPATHXX_OME_MASK_BZIP2_ARCHIVE=source/third_party/bzip2-1.0.8.tar.gz
cmake --build ome-build --config Release
```

The plugin is AGPL-3.0-only; corresponding source and obligations remain applicable.
Private unchanged Expat2.10.0 is MIT plus CC0-1.0 (`siphash.h`), with exact selected
source/build hashes in `reference-lock.json` and notices in the package.
Private zlib 1.3.2 (Zlib) and libbzip2 1.0.8 (bzip2-1.0.6) are built from
unmodified archives pinned in `MaskCodecs.cmake`. The package includes both
archives and their original notices. OME's core
schema is CC-BY-4.0. This work is derived in part from the OME specification.
Copyright (C) 2002-2016 Open Microscopy Environment. No upstream image, screenshot,
video, model, test corpus, NC or unknown-terms asset is included or admitted.
