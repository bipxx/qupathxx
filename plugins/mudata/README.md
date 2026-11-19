# MuData modality importer

`org.qupathxx.mudata` imports one explicitly named modality from a
MuData 0.1.0 H5MU container. Select `X` or one `layers/NAME` matrix inside
the modality. The plugin reads that AnnData group's own `obs` and `var`
index datasets, including their ordering and exact UTF-8 IDs. Position rows
are joined by those observation IDs through the native QMX importer.
[MuData on-disk format](https://mudata.scverse.org/latest/io/spec.html)

```text
qupathxx_format_import --format-plugins ABS_MANIFEST matrix-index org.qupathxx.mudata STUDY.h5mu POSITIONS.tsv IMAGE OUTPUT.qmx --plugin-options-json '{"modality":"rna","matrix":"X","value_domain":"raw-counts"}'
```

`POSITIONS.tsv` has `observation_id`, `x_px`, and `y_px` columns. Supply
exactly one coordinate per selected observation ID. The value domain is
required. `raw-counts` selects exact unsigned 64-bit counts. Other domain
labels select finite Float64 with precision-loss checks for integer inputs.
The selected matrix may be dense, CSR or CSC. The module requires a format
host with matrix ABI5. For CSR and CSC, the native QMX writer indexes only
stored entries, including explicit zeros. Missing coordinates imply zero.
Sparse entry limits apply to the stored entries, and the requested work
allowance still bounds the provider's in-memory sparse index. Duplicate
sparse entries and out-of-range indices refuse.

The QMX receipt retains the file identity, modality, matrix choice, value
domain, position identity, and plugin identity. Reopening QMX uses its
indexed values and axes. Global modality maps, other modalities, embeddings,
graphs, and metadata columns are outside this single-modality import.
The selected objects must be local HDF5 objects in supported encodings.

The optional Windows Release plugin is AGPL-3.0-only. Its static HDF5 2.2.0
and prefixed zlib 1.3.2 inputs retain their original notices and pinned source.
Build with `QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX`, the retained FP04 SDK as
`CMAKE_PREFIX_PATH`, and `QUPATHXX_MUDATA_DEPENDENCIES` pointing to the
matching offline HDF5/zlib header tree. MuData, AnnData and h5py are
development-only fixture tools.
