# Native image-pixel point TSV provider

`org.qupathxx.point-tsv` version1.0.0 implements the annotation-family C ABI,
format `point-tsv`, contract `pixel-point-annotation-tsv/1`.
The independently authored parser has no application-core, codec, Python or Java
dependency. It requires the matching FP04 native host and FormatPluginKit.

## Selected schema

The exact first row is `id`, `x`, `y`, `c`, `z`, `t`, `class`, separated by TAB.
Every following row has exactly those seven fields; at least one point is
required. The example contains literal TAB separators:

```text
id	x	y	c	z	t	class
0001	12.5	0	-1	0	0	Tumor
1	1e2	20.25	0	2	0	Normal
```

Source IDs are exact, unique, nonempty UTF-8 strings; leading zeros and spaces
retain their original meaning. Class labels are opaque UTF-8 bytes. Empty class
means unclassified. The host explicitly maps nonempty source labels to native
classes; this DLL receives no Document, class catalog or mutation authority.

X and Y are finite, nonnegative decimal doubles in level-zero image pixels.
Decimal exponents are accepted; leading plus, whitespace, hexadecimal, NaN and
infinity refuse. There is no unit conversion, scale estimation, registration,
rounding to an integer lattice or automatic coordinate transform. Channel C is
`-1` for unspecified/all or an unsigned decimal index through `INT32_MAX`.
Z and T are explicit unsigned decimal indices through `INT32_MAX`; `-1` does
not select an active plane. Unsupported geometry types or extra metadata have
no implicit representation in this schema.

LF, CRLF and mixed line endings are accepted. The final line may be unterminated,
including an empty final class field. Blank rows, lone CR, a byte-order mark,
extra columns, comments and trailers refuse. There is no quoting or escaping;
quote characters are literal label bytes. No text is trimmed or normalized.
NUL, ASCII controls and Unicode C1 controls are forbidden inside tokens. Each
token, source ID and class label is bounded to4096 bytes; the first row, including
its line ending when present, is bounded to64KiB. Options must be empty.

## Resource and native boundary

This bounded full-input profile retains one owned source byte array and one
exact-sized typed point array. A temporary source-ID index detects duplicates
with an in-place iterative heap sort. Source, points, index, array bookkeeping
and conservative path/handle allowance must all fit the caller's budget, at
most256MiB. It makes no scalable arbitrary-size import claim.

Paths are absolute UTF-8. Synchronous file reading, parsing, duplicate checks
and point pages check cancellation at bounded byte/token spans. A page copies
typed coordinates, plane indices, exact source IDs and class bytes into bounded
caller-owned buffers. Short buffers report required sizes without writing
payload. Requests, cancellation callbacks and output pointers are never retained;
the caller destroys the opaque handle through this module. No STL object,
exception or allocator ownership crosses the DLL boundary.

Wrong exact headers report unsupported; malformed selected rows, labels,
coordinates or plane indices report I/O error. Invalid structures, page ranges
and work budgets report invalid request; actual allocation failure reports
internal error and cancellation reports cancelled. The matching host validates
the typed output, binds provider/source/options identity, previews explicit
mapping and applies one guarded native append/undo transaction. This provider
does not publish project objects or modify classes.

## Offline build and rights

Windows x64, MSVC1944/compiler19.44.35225.0, Release `/MD`:

```powershell
cmake -S plugins/point_tsv -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=KIT"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

Relocated source retains `plugins/format-common` beside `plugins/point_tsv`.
The kit supplies only ABI headers, exact compiler/profile information and license;
configuration fetches no dependencies and discovers no Python or core SDK.
Output is `qpx_point_tsv_plugin.dll` plus its hash-bound `format-plugins.json`.
Use a separate install prefix per provider before combined package staging,
because the standalone manifest has that same filename in both providers.

The declared schema, independently authored parser and shared helpers retain
the repository's [AGPL-3.0-only license](../../LICENSE). No third-party schema
implementation, fixture, model, codec or reference runtime is included. Supply
the corresponding source and matching license under those actual terms.
Qualification and exact source/module/package identities belong to the enclosing
FP04 receipt; documentation alone does not qualify changed code.
