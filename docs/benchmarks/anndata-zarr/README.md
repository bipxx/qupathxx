# Native AnnData Zarr matrix import

The Windows workflow imports a selected AnnData Zarr 2 matrix and complete
position TSV into QMX. The public SDK example retains that QMX in a new project.
The original image supplies the level-zero coordinate frame. The matrix reader
uses explicit observation and feature IDs, and counts and continuous values
have distinct domains. See the [provider profile](../../../plugins/anndata_zarr/README.md).

The store content hash covers sorted relative names, lengths and complete file
bytes. The filesystem observation also binds the directory location and file
versions. A repeat import from the same unchanged store has identical QMX bytes
and project association identity. Copying a byte-identical store to a different
location preserves the content hash and creates a distinct capture and project
association. An already saved QMX or owned project still opens independently
of the original store and provider.

Directory ABI3 and ABI4 and QMX schema 6 carry this contract. Retained schema 4 and 5 baselines
are compared byte for byte on the same original source paths. Counts retain
exact UInt64 values, including values above 2^53. Continuous values retain
finite Float64 values, including signed zero.

The CLI takes the primary metadata key explicitly. For example, in PowerShell
7 a counts store with the default AnnData index names can use:

```powershell
qupathxx_format_import --format-plugins ABS_MANIFEST matrix-store-index `
  org.qupathxx.anndata-zarr ABS_STORE .zattrs ABS_POSITIONS ABS_IMAGE ABS_NEW_QMX `
  --plugin-options-json '{"matrix":"X","observation_ids":"obs/_index","feature_ids":"var/_index","domain":"counts","value_domain":"raw-counts"}'
qupathxx_format_import matrix-info ABS_NEW_QMX
```

The [public SDK example](../../../examples/anndata_zarr_plugin/main.cpp) builds
against the matching native `qupathxx` SDK. Its native index call derives the
frame from the input project and protects both destinations from AnnData and
linked project sources. The subsequent attachment writes the managed QMX
beside the new project and leaves the input template untouched. If project
publication fails after indexing, retry from the completed QMX with
`matrix-attach INPUT_PROJECT IMAGE_ID QMX FILE_SHA256 NEW_PROJECT`.

A synthetic store written by AnnData 0.12.7 and zarr-python 2.18.7 imported
exact values and names with `anndata.io.write_zarr(store, adata,
compressor=Zlib(level=1), convert_strings_to_categoricals=False)`. Provider
1.1.0 also reads the pinned writer's default Blosc/LZ4 output and explicit
BloscLZ. Exact counts and names, native SDK save and reopen after moving the
source and standalone QMX, genuinely compressed chunks and truncated-frame
refusal passed in the [32-command receipt](../../../build/gui-aggregation/catalogue-closure/anndata-zarr/integration-blosc-01/results.json).
If `.zmetadata` is present,
the selected entries must agree with their physical metadata files. A store
with disagreeing shape metadata was rejected before output publication.

Provider 1.2.0 uses ABI4 to import only stored CSR and CSC entries. The QMX
profile `external-sparse-store/1` preserves explicit zeros and keeps absent
coordinates implicit. The 100-million logical-cell cap still applies to dense
input. Sparse input keeps axis, stored-entry and workspace limits. Temporary
SQLite and publication files and managed copies also use disk space.
The [original dense import receipt](../../../build/gui-aggregation/catalogue-closure/anndata-zarr/integration-01/results.json)
measured 9.3 seconds, 22,810,624 QMX bytes and 45,621,248 peak sampled output
bytes for a 1,024 × 1,024 CSR matrix with 1,024 stored entries.
The [ABI4 integration run](../../../build/gui-aggregation/catalogue-closure/anndata-zarr/integration-sparse-02/results.json)
measured 2.50 seconds and 331,776 QMX bytes on that matrix. The 10,240 ×
10,240 CSC fixture imported all 10,240 stored entries across three pages. The
same run verified exact ABI3 QMX bytes with the retained plugin, signed-zero
bits in sparse continuous input, and `matrix-attach` recovery without the source
store or provider. Timings are single-run local observations.

The [Windows delivery receipt](../../../build/gui-aggregation/catalogue-closure/anndata-zarr/results.json)
records the admitted 285-member package and ZIP, relocated SDK configuration,
and installed use and removal. Removal preserved nine user files, including
the two new projects and their owned QMX files. The package lists its exact
Loom, NGFF label and native SDK companions and their source obligations.
