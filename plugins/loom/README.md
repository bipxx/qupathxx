# Native Loom matrix provider

`org.qupathxx.loom` is an optional native matrix provider, version1.0.0,
contract `loom3-dense-typed/1`, using the separately negotiated matrix ABI2.
This selected profile reads a local Loom3 dense matrix or one explicitly named
layer into the existing native QMX import authority. It requires no Python,
Java, service, model or language-runtime fallback. Source and build qualification
remain separate from the frozen SDK/core and earlier format plugins.

Pass explicit interpretation options; filenames and attribute names choose no
orientation, IDs, domain, coordinate frame or registration:

```json
{"matrix":"/matrix","feature_ids":"/row_attrs/ID","observation_ids":"/col_attrs/ID","feature_names":"/row_attrs/Name","domain":"counts","value_domain":"raw-counts"}
```

`matrix` is exactly `/matrix` or `/layers/NAME`. Feature/observation IDs select
one dataset immediately below `/row_attrs` or `/col_attrs`; `feature_names` is
optional and selects one row attribute. IDs and names are one-dimensional
variable-length UTF-8 strings, at most256 bytes per value; IDs are nonempty,
and empty names use the existing native ID fallback. There is no normalization,
numeric-ID conversion or positional join. The matrix is features x observations
on disk; typed pages are observation-major, feature-minor. A selected layer must
match the main matrix's full shape. Both axes contain1..1,000,000 elements.

The file must contain `/attrs`, `/row_attrs`, `/col_attrs`, `/row_graphs`,
`/col_graphs`, and the scalar variable-length UTF-8 dataset
`/attrs/LOOM_SPEC_VERSION` equal to `3.0.0`. This matches the pinned reference
writer's version literal. The older pinned documentation's `v3.0.0` spelling is
refused; legacy root-attribute marker/string conventions are outside this profile.

Numeric input accepts the eight full-width signed/unsigned8/16/32/64-bit integer
types and IEEE binary16/32/64 in either byte order. Counts require explicit
`domain: "counts"`, `value_domain: "raw-counts"`: negative, nonfinite,
fractional or out-of-UInt64 values refuse. Integer inputs retain every bit,
including `UINT64_MAX` and adjacent values above2^53. Continuous input requires
`domain: "continuous"` and an explicit nonempty value-domain label of at most128
UTF-8 bytes other than `raw-counts`. It returns finite binary64, preserving signed
zero and subnormals; integer-to-continuous conversion refuses whenever it cannot
represent the exact integer. This declares interpretation, not a transformation.
Native QMX retains its existing dense-zero and missingness contracts.

Only selected datasets are decoded. Graphs, unrelated row/column/global attributes,
unselected layers and the unselected main matrix's values are omitted. This is
an explicit selected-data reader, not a validator/importer of every Loom object.
Native host services own the exact complete-ID coordinate-TSV join, frame/domain
validation, source/implementation receipt, QMX staging and protected atomic
publication. A QMX result remains independently usable without the original
file, positions or optional provider; recapture binds the original inputs and
exact provider interpretation/implementation.

The provider streams axis pages of up to256 records and value pages of up to65536
elements through fixed scratch, never densifying the complete matrix or retaining
the full ID catalogs. Requests supply independent16..256MiB work and1byte..16GiB
file allowances. Selected declared decoded chunks are at most1MiB; encoded chunks
are at most their declared bytes plus64KiB. Compact/contiguous/chunked storage
and unique Deflate, shuffle and Fletcher32 filters are admitted. Virtual/external
storage, soft/external selected links, unknown/repeated filters, nonstandard
numeric encodings and unsupported interpretation keys refuse.

Before normal HDF5 reads, bounded native raw-chunk inspection reverses the admitted
filter pipeline with capped zlib output/work buffers and the exact pinned HDF5
Fletcher32 authority. It requires the resulting byte count to equal the declared
on-disk chunk layout, including variable-string heap-reference chunks. It does
not reconstruct strings or perform scientific conversion; HDF5 retains those
authorities. A fixed64-entry chunk cache is valid only under a retained Windows
read lease denying write/delete; the HDF5 local handle must identify that same
file. Source replacement/mutation is still checked independently by the host.
HDF5 raw/metadata caches, string allocations and application pages are bounded;
these checks are not a process sandbox or a total-process resource guarantee.

The implementation/helper are AGPL-3.0-only. The exact privately linked HDF52.2.0
and prefixed zlib1.3.2 retain their own full notices, corresponding sources and
terms; the source lock records their identities. HDF5's original license/notice
file identifies its3-clause BSD terms and additional notices; zlib uses Zlib.
The schema reference is loompy3.0.6 commit
`6177c213ab59110cea1bdaed08ac5a355fc13524`, under its BSD-2-Clause notice.
Pinned BSD2 Python source is retained separately as nonexecuted reference
context; no Python is imported, executed or required by the native provider.
No upstream image/data/model/fixture or restricted derivative is admitted.

Configure with the matching format kit, frozen native SDK and the three explicit
header roots `QUPATHXX_LOOM_HDF5_INCLUDE`,
`QUPATHXX_LOOM_HDF5_GENERATED_INCLUDE`, `QUPATHXX_LOOM_ZLIB_INCLUDE`.
The module target is `qpx_loom_plugin`; its manifest is
`loom-format-plugins.json`. No dependency fetch or frozen library rebuild occurs.
Matching module/source/material and native/CLI/SDK/deployment evidence is required
before claiming the selected workflow delivered.
