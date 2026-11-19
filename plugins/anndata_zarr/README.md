# Native AnnData Zarr matrix provider

Optional `org.qupathxx.anndata-zarr`, contract `anndata-zarr2-typed/1`, uses the
separately negotiated matrix ABI4 directory-store boundary. No Python/Java
runtime, conversion process, application object or result publisher crosses
the module interface. The native host owns full-store identity/currentness,
coordinate joining, QMX staging and project persistence.

Select `X` or one `layers/NAME`, explicit observation/feature index paths matching
the encoded dataframe `_index` keys, optional feature-name column, and an
explicit counts or continuous domain. Counts require `raw-counts`; continuous
requires another nonempty label. No values, missingness, coordinate frame,
registration or modality association are inferred.

```json
{"matrix":"X","observation_ids":"obs/_index","feature_ids":"var/_index","domain":"counts","value_domain":"raw-counts"}
```

The selected local profile is AnnData0.1.0 in Zarr2, dataframe0.2.0 catalogs,
dense array0.2.0 or CSR/CSC0.1.0 matrices, and string-array0.2.0 IDs/names using
the object `vlen-utf8` byte encoding. All eight integral scalar dtypes and
Float32/64 retain C/F chunk order and byte order. Counts preserve all UInt64
bits; continuous values must be finite and integer conversion exactly
representable. Native IDs retain original UTF8 and order, separately from
display names. Duplicate sparse coordinates refuse; they are never summed.

Integral array fills require in-range JSON integer/null tokens. Floating
integral tokens such as `0.0` refuse in this new declared profile. Finite
Float32/64 fills use their declared scalar interpretation. A null fill permits
present chunks but missing numeric chunks refuse undefined data; missing string
chunks cannot invent IDs. Sparse absent coordinates are measured positive zero;
explicit stored continuous negative zero survives. Unselected attributes,
columns, layers, graphs, embeddings and metadata are omitted from interpretation
but remain part of the host's observed/content-bound store identity.

Work is16..256MiB, with at most1,000,000 elements on each axis and at most
100,000,000 logical cells for dense input. Sparse input uses the host's stored
entry limit and the provider's workspace limit. Metadata files are at most1MiB, with a4MiB fixed strict-JSON arena,
8192 nodes,4096 object members and depth16. Encoded chunks are at most1MiB+64KiB;
decoded chunks at most1MiB. If `.zmetadata` exists, its consolidatedformat1
selected member must agree with the physical JSON before interpretation. It has
the same1MiB/4MiB arena bounds; its enclosing tree permits depth18. Object key
order/JSON escapes do not cause disagreement, while exact numeric token kinds
and spelling are retained. Unselected consolidated entries are not interpreted.
The provider accepts raw, zlib, gzip and Blosc 1 with LZ4 or BloscLZ. This
includes the default LZ4 output of the pinned AnnData/Zarr 2 writer. Blosc
decodes one bounded, validated frame in memory on one thread. The zlib
allocator caps its own allocations at 512 KiB. Zstd, Zarr 3, fixed-Unicode
arrays, categorical ID columns and arbitrary object decoders remain outside
this profile.

Dense values are read in bounded pages and never retained as a whole matrix.
CSR/CSC import builds a sparse coordinate index of exactly16 bytes per stored
entry plus a12MiB fixed provider workspace. It serves sorted row-major pages of
only those stored entries. Missing coordinates occupy no QMX rows. The reservation
must fit the selected allowance before allocation. Sorting and scanning are
cancellable. Valid unsorted source indices are supported, and duplicate
coordinates are refused.
Axis pages are at most256 entries and numeric pages at most4096 values.

Each member opens with a Windows read lease; final opened paths must stay below
the leased local root, with no reparse traversal. Whole-store membership,
byte-content identity, cancellation and publication checks remain native host
authorities. This is not a process sandbox or total-process RSS bound.

The [source lock](source-lock.json) and [notices](THIRD_PARTY_NOTICES.txt) keep
first-party AGPL, linked Zlib and nonexecuted BSD3/CC-BY4/MIT reference terms
distinct. The reused C-Blosc 1.21.6 static decoder retains its separate
[source lock](../blosc-bytes/source-lock.json) and
[notices](../blosc-bytes/THIRD_PARTY_NOTICES.txt). No upstream
scientific asset, restricted material or model is used.
The packet's native/material/deployment receipts determine actual qualified
products and support; this source README does not assert a completed delivery.
