# ASAP XML native annotation plugin

Optional provider `org.qupathxx.asap-xml1.0.0`, contract
`asap-level-zero-xml/1`, uses annotation ABI3 and the existing C++ geometry,
frame/class/plane, preview/apply, undo and project authorities. It links private
static Expat2.10.0 and needs no ASAP, Qt, pugixml, Java or Python runtime.

The selected encoding is the upstream public `XmlRepository` at the exact commit
in `reference-lock.json`. It is unnamespaced `ASAP_Annotations`, with Annotations
and an optional AnnotationGroups container. Dot, PointSet, Polygon, canonical
four-corner axis-aligned Rectangle and two-endpoint Measurement become native
points/polygon/rectangle/polyline values. Coordinate Order must be a complete
zero-based permutation; XML element order does not replace it. No points, shapes
or invalid topology are repaired/dropped. Spline, None/unknown types, transformed
or rotated rectangles, nonstandard units/planes/negative-space extensions and
noncanonical color encodings explicitly refuse. A group called "negative" remains
an ordinary source class, not an inferred hole. This is not an Aperio XML reader.

Coordinates are literal level-zero image-pixel XY. The numeric contract matches
upstream XML decimal-double -> Point(float): native values are the exact promoted
IEEE binary32 values, with original decimal text retained in source facts.
The plugin requires SSE round-to-nearest, preserved subnormals and masked
floating-point exceptions, and does not
change the caller control modes. Overflow, nonzero-to-zero underflow, nonfinite or
negative values refuse. Signed zero and nonzero subnormals remain exact. Native
Roi validation owns bounds/topology and atomic target-frame publication. The
source contains no image identity/calibration; the caller must select the same
pixel frame. No filename, scale, alignment, C/Z/T or physical calibration is
inferred. Absent planes use existing native unique-plane/explicit-choice rules.

Names may be empty or repeated. Exact source IDs use document position
`annotation/0`, `annotation/1`, etc.; display names are independent and use the
source name or generated ID. IDs are bound to the entire source SHA and recipe.
Groups require unique nonempty names (<=256 UTF-8 bytes, reserved None excluded),
resolved parents and acyclic ancestry <=64. Source class labels are leaf group
names; explicit native class mapping is required. No native classes or taxonomy
are created automatically. Group membership, hierarchy, arbitrary group Attribute
values, names, types, numeric text and namespace contexts are retained as native
immutable source facts. QName-valued metadata retains its in-scope bindings.

Native preferred RGB follows the upstream drawing authority: use the topmost
ancestor group color when grouped, otherwise the annotation's own color. Missing
or empty colors use upstream's #F4FA58. Only canonical #RRGGBB (either case) is
admitted; named/alpha/other Qt color forms refuse. Original annotation and group
colors remain source facts even when a root group's color takes precedence.
Measurement imports its two exact endpoints; it does not invent a source physical
measurement or replace native calibrated measurement services.

Parsing reuses bounded fixed-arena XML/QOX2 source support shared with OME ROI.
XML1.0 UTF-8/UTF-16 only; DTDs, declared/external entities, unknown structure,
malformed input, ambiguous references and quotas fail before publication. No
network/schema fetch. Limits include 64MiB input,64 XML levels,100,000 nodes,
64 attributes/in-scope bindings per node,8MiB text,4096 groups,65,000 coordinates
and1MiB geometry per object,256KiB metadata subtree and native fact/work quotas.
Caller work is256KiB–256MiB; budget exhaustion and cancellation retain no partial
handle, preview, project or edited target. This is bounded full-input parsing.

Build independently offline on Windows x64 MSVC1944/compiler19.44.35225.0,
Release `/MD`, against the matching FormatPluginKit:

```powershell
cmake -S source/plugins/asap_xml -B asap-build -A x64 `
  -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit `
  -DQUPATHXX_ASAP_EXPAT_SOURCE_DIR=source/deps/expat
cmake --build asap-build --config Release
```

The independently authored parser/shared helpers/fixtures are AGPL-3.0-only.
Expat is MIT, except SipHash CC0-1.0; exact sources/notices remain separate.
ASAP's GPL-2.0-or-later source and author/license notice are development encoding
references, with hashes/URLs retained; upstream code is not linked/copied as the
implementation or bundled as a runtime. No upstream image, model, dataset/test
corpus, NC/research-only or unknown-terms material is admitted. Per-component
notices and complete matching first-party/dependency source accompany packages.
