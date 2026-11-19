# Arrow IPC expression matrices

The optional Windows x64 native module `org.qupathxx.arrow-matrix` 1.3.0 imports a
wide Arrow IPC V5 or Feather v2 expression table. Each row represents one
observation. Select the observation ID column and provide the feature columns
in the order you want saved. Column names become feature IDs. Observation IDs
can be UTF-8 text or signed or unsigned 8-, 16-, 32-, or 64-bit integers.
Integer IDs are stored as exact decimal text. Use that spelling in the position TSV.

Use the existing image-position TSV with `observation_id`, `x_px`, and `y_px`
headers. The native host joins exact IDs and checks coordinates against the saved
image before publishing a new QMX. Attach that QMX through `matrix-attach` to
keep an owned copy in the project. The project can reopen the copy after the
original Arrow file, positions and external QMX are removed.

```powershell
qupathxx_format_import --format-plugins C:/plugins/arrow-matrix/format-plugins.json matrix-project-index org.qupathxx.arrow-matrix counts.feather positions.tsv study.qpxproj 1 counts.qmx --plugin-options-json '{"observation_id":"spot","feature_columns":["Gene.B","Gene.A"],"value_domain":"raw-counts"}'
```

Pass `--reader-plugins` with the saved image reader's absolute manifest path when
applicable. For count matrices, use `raw-counts` and nonnegative integral
columns. All UInt64 bits, including zero, are preserved. For continuous
matrices, supply an explicit label such as `normalized-expression` and use
finite Float32 or Float64 columns. Float32 widens exactly. Null IDs, duplicate IDs,
null values and mixed numeric domains fail import.

The profile supports flat, little-endian files with up to 64 columns, 4,096
record batches and 100,000 observations, subject to the host's file and work
budgets. UTF-8 and integer observation IDs, integer count columns, and Float32
and Float64 continuous columns may use dictionary encoding, including ordered
deltas. Arrow buffers may be uncompressed, LZ4 frame or Zstandard. Parquet,
streams and nested matrix encodings require different profiles.

## Offline build

From the package root, use the matching MSVC1944 Windows x64 FormatPluginKit.

```powershell
cmake -S source/plugins/arrow_matrix -B build -A x64 -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit -DQUPATHXX_NANOARROW_ARCHIVE=source/third_party/nanoarrow-0.9.0-library.zip -DQUPATHXX_ARROW_LZ4_ARCHIVE=source/third_party/lz4-1.10.0-library.zip -DQUPATHXX_ARROW_ZSTD_ARCHIVE=source/third_party/zstd-1.5.7.tar.gz
cmake --build build --config Release --target qpx_arrow_matrix_plugin
```

The plugin uses matrix ABI2 and shares the native point provider's Arrow
decoder. QuPathxx code is AGPL-3.0-only. Nanoarrow 0.9.0 and bundled FlatCC
are Apache-2.0. LZ4 1.10.0 is BSD-2-Clause and the Zstandard 1.5.7 decoder
uses its BSD-3-Clause option. The archive includes exact source, decoder patches,
notices and hashes. PyArrow is used by the independent validation writer.
[Arrow IPC file specification](https://arrow.apache.org/docs/format/Columnar.html#ipc-file-format)
