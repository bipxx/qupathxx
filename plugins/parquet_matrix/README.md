# Parquet expression matrices

The optional Windows x64 native plugin `org.qupathxx.parquet-matrix` 1.1.0 imports
flat Parquet tables of expression values. Select a UTF-8 or signed or unsigned
8-, 16-, 32-, or 64-bit integer column for observation IDs. Integer IDs are
stored as exact decimal text. Use that spelling in the position TSV.
Select the feature columns in the order you want to retain.
The native importer joins the exact IDs to `observation_id`, `x_px` and
`y_px` in a position TSV and checks them against the saved image frame.

```powershell
qupathxx_format_import --format-plugins C:/plugins/parquet/format-plugins.json matrix-project-index org.qupathxx.parquet-matrix counts.parquet positions.tsv study.qpxproj 1 counts.qmx --plugin-options-json '{"observation_id":"spot","feature_columns":["Gene.B","Gene.A"],"value_domain":"raw-counts"}'
```

Pass the saved image reader's absolute `--reader-plugins` manifest as needed.
Use `matrix-attach` to save an owned QMX in the project. Its saved data can
reopen independently of the Parquet file, the position TSV and the external QMX.

The `raw-counts` domain accepts nonnegative integral columns and retains
full UInt64 values. Continuous domains require an explicit name and finite
Float32 or Float64 columns. Null values and duplicate IDs fail import.
The profile supports local unencrypted flat files, dictionary and plain
pages, and uncompressed, Snappy or Zstandard selected columns. The provider
verifies page checksums when present. It limits files to 64 columns, 100,000
observations and 4,096 row groups within the host import work budget.

## Offline build

The bundle includes Arrow 26.0.1 and its pinned native source dependencies
with notices. The decoder is linked statically and the plugin uses matrix ABI2.
Build with PowerShell 7, CMake and the matching Windows x64 MSVC1944
FormatPluginKit:

```powershell
pwsh -File source/tools/ci/build_parquet_decoder.ps1 -Archives source/third_party -BuildRoot decoder-build
cmake -S source/plugins/parquet_matrix -B plugin-build -A x64 -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit -DCMAKE_PREFIX_PATH=decoder-build/decoder-sdk
cmake --build plugin-build --config Release --target qpx_parquet_matrix_plugin
```

QuPathxx code is AGPL-3.0-only. The decoder and its dependencies retain the
licenses and exact hashes listed in `source/plugins/parquet_matrix/decoder-sources.json`.
[Apache Parquet file format](https://parquet.apache.org/docs/file-format/)
