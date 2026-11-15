# Native dense counts TSV provider

`org.qupathxx.counts-tsv` version1.0.0 implements the matrix-family C ABI,
format `counts-tsv`, contract `dense-observation-feature-uint64-tsv/1`.
It is an independently authored native parser with no application-core, codec,
Python or Java dependency. It requires the matching FP04 host and FormatPluginKit;
the frozen image-provider host does not acquire this new matrix boundary.

## Selected schema

The first row is the exact text `observation_id`, followed by one or more feature
IDs, separated by TAB. Every following row contains its observation ID followed
by exactly one unsigned decimal UInt64 count per feature. At least one
observation is required. The example contains literal TAB separators:

```text
observation_id	0002	2
0001	9007199254740993	18446744073709551615
1	0	0007
```

Feature and observation IDs are separate catalogs. Each catalog requires unique,
nonempty UTF-8 IDs of at most4096 bytes. IDs retain their original bytes, including
leading zeros, spaces and literal quote characters. Display names are absent;
there is no gene-symbol, feature-type or observation alias inference.

Counts are exact integers through `UINT64_MAX`; they never pass through double.
Leading-zero decimal counts are valid. Empty, signed, fractional, nondecimal and
overflowing counts refuse. There are no implicit zeros, missing-value spellings,
duplicate sums, optional columns, comments or trailer records.

Only simple TAB tables are accepted. LF, CRLF and mixed line endings work; a
final line need not end in a newline. Blank rows, lone CR, quoting/escaping
conventions, a byte-order mark and additional fields refuse. Quotes are ordinary
label characters and cannot protect a TAB or newline. No text is trimmed or
normalized. NUL, ASCII controls and Unicode C1 controls are forbidden inside
tokens. Every token is bounded to4096 bytes and the complete first row, including
its line ending when present, to64KiB. Native options must be empty.

Spatial positions stay outside this DLL. The matching native host joins a
separate explicit position TSV through its existing authority, requires exact
observation-ID coverage and publishes the typed data through its QMX writer.
This provider does not assign coordinates or write a result file.

## Resource and ABI contract

This is a bounded full-input profile, not an arbitrary-size streaming importer.
The entire source plus exact-sized decoded UInt64 values, ID-span arrays,
temporary duplicate-check index, every array's bookkeeping and conservative
path/handle allowance must fit the caller's work budget, at most256MiB. Dense
expansion can therefore refuse a source much smaller than256MiB. Duplicate checks
use an in-place iterative heap sort with no additional sort allocation.

Absolute UTF-8 source paths are required. Open/read/parse/duplicate checks and
caller-owned pages check synchronous cancellation at bounded byte/token spans.
No callback, request or caller buffer pointer survives a call. Only an opaque
module-owned handle crosses the ABI; callers destroy it through this module.
Pages copy exact typed values or ID bytes into caller buffers. Short buffers
report required rows/bytes without writing payload. Counts use observation-major,
feature-minor order; catalog display-name fields are empty.

Wrong exact headers report unsupported; malformed selected tables report I/O
error. Invalid requests, work budgets and page ranges report invalid request;
allocation failure reports internal error and cancellation reports cancelled.
The host independently validates plugin output, binds source/provider/module/
options identity and owns currentness and atomic native publication.

## Offline build and rights

Windows x64, MSVC1944/compiler19.44.35225.0, Release `/MD`:

```powershell
cmake -S plugins/counts_tsv -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=KIT"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

Keep `plugins/format-common` beside `plugins/counts_tsv` when relocating source.
The kit supplies only ABI headers, the exact compiler/profile record and license;
configuration performs no dependency fetch, Python discovery or core SDK lookup.
The output is `qpx_counts_tsv_plugin.dll` and its hash-bound
`format-plugins.json`. Build/install providers in separate directories before
the host's package staging combines their manifests; installing two individual
manifests into one prefix would overwrite the first manifest.

The declared schema, independently authored parser and shared source helpers
retain the repository's [AGPL-3.0-only license](../../LICENSE). No third-party
reader, fixture, model, codec or reference runtime is included. Product source
and matching license must accompany distribution under those actual terms.
Native host/SDK qualification, source identities and package admission belong to
the enclosing FP04 receipt; this README does not qualify changed module bytes.
