# WKT and WKB image-pixel geometry plugin

Optional independent provider `org.qupathxx.wkt-wkb`1.0.0, annotation ABI3,
contract `image-pixel-wkt-wkb/1`. The selected profile reads a single2D geometry
value as WKT, raw WKB, or hexadecimal WKB. It covers Point, LineString, Polygon,
MultiPoint, MultiLineString, MultiPolygon and nested GeometryCollection. The
source representation is not an intrinsic classification or object-ID catalogue.
Selecting this provider explicitly declares image-pixel XY units; it is not a
general geographic/physical-coordinate import route.

Coordinates are literal finite nonnegative image-pixel XY binary64 values;
decimal WKT uses locale-independent conversion and refuses overflow/underflow.
The caller selects the native target image-pixel frame and explicit Z/T choices
when the image has more than one plane. No geographic CRS, scale, origin,
registration, classification or Z/T plane is inferred. WKT/WKB with Z, M, ZM,
SRID/EWKT/EWKB flags, curved/surface extension types and empty members refuse.
An invalid member refuses the entire source. No dimension/row loss, coordinate
clamping, implied ring closure or topology repair is performed.

Both binary byte orders are supported, independently on every member. All bytes
must be consumed. Hex WKB accepts ASCII whitespace, optional UTF-8 BOM and an
optional `0x`/`0X` or `\x` transport prefix; it must contain complete hex bytes.
WKT accepts ASCII case-insensitive keywords/whitespace, optional UTF-8 BOM,
ordinary decimal/scientific notation and either consistent MultiPoint spelling
(`(x y,...)` or `((x y),...)`). Polygon rings require at least four coordinates
with an explicit repeated closing point. Native Roi owns topology and image
bounds validation, so otherwise valid geographic geometry can be unsuitable for
this image-pixel destination.

Point/MultiPoint becomes native Points; LineString becomes native Polyline;
Polygon/MultiPolygon preserves explicit shell/hole/part roles. Native singleton
MultiPolygon may have Polygon kind; the original container type remains a source
fact. MultiLineString and heterogeneous collections become ordered native objects
with exact IDs `geometry` or `geometry/<zero-based member path>`. Display names
use the source filename stem independently of IDs. Source facts retain encoding,
original type, member paths and each enclosing container's type/path/child count.
Original transport bytes remain distinguished by the native source SHA receipt.
This is geometric/native-project replay, not reconstruction of original textual
spelling/byte order. Imported objects have no inferred source class or color.

All retained input, decode tree, facts and output buffers share the fixed native
arena and ABI3 producer support used by ImageJ. Supplied work reservation256 KiB–
256 MiB includes64 KiB handle/platform overhead plus eight bytes per UTF-8
path byte for native path conversion/copies. Input is capped at64 MiB,
nesting at32 levels, nodes/objects at100000, source coordinates at1000000,
numeric text tokens at256 bytes and native geometry at1 MiB/object. Native ID,
name/property/page/capture quotas additionally apply. These are admission limits,
not an out-of-core or timing/RSS guarantee.

Parser/shared helpers/kit/authored fixtures are AGPL-3.0-only; complete matching
source and notices accompany a distribution. The optional DLL links no external
geometry reader, codec, Python, Java or core library. Its host still uses the
separately licensed native Roi/geometry authorities. OGC06-103r4/v1.2.1 and its
copyright implementation terms are pinned in `reference-lock.json`; the standard
is referenced, not redistributed. No standards certification or patent grant is
asserted. No upstream code, image or reference dataset is a required asset.

Build against the matching ABI3 FormatPluginKit (Windows x64 MSVC1944 Release
`/MD`); the build downloads nothing:

```powershell
cmake -S plugins/wkt_wkb -B <cache> -A x64 `
  -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=<matching-kit>
cmake --build <cache> --config Release --parallel 2
cmake --install <cache> --config Release --prefix <plugin-directory>
```

Use the existing native `annotations-import`/SDK services with this provider and
its exact manifest. Keep module/manifest together and preserve the separate frozen
native SDK/runtime. Implementation qualification and deployment evidence are in
progress; the current task owns the delivery claim.
