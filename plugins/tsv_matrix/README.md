# Typed TSV expression matrices

The optional Windows x64 native provider `org.qupathxx.tsv-matrix` reads wide
UTF-8 expression tables with tab-separated fields and one observation per row.
The header names the required observation ID and feature columns. Select them
explicitly, in the desired feature order. Quote characters are literal text.

```powershell
qupathxx_format_import --format-plugins C:/plugins/tsv/format-plugins.json matrix-project-index org.qupathxx.tsv-matrix expression.tsv positions.tsv study.qpxproj 1 expression.qmx --plugin-options-json '{"observation_id":"spot","feature_columns":["Gene.B","Gene.A"],"value_domain":"normalized-expression"}'
```

Use `raw-counts` for unsigned decimal counts through UInt64. Explicit
continuous domains accept finite binary64 decimal values, including
exponents, signed zero and subnormals. Selected numeric cells are required.
Identifiers are exact UTF-8 through 256 bytes. The provider accepts an
optional UTF-8 BOM, LF or CRLF, and up to 64 columns, 100,000 observations and
4,096 bytes per field within the native file and work limits.

Supply the separate native positions TSV with `observation_id`, `x_px`
and `y_px`. Use `matrix-attach` to save an owned QMX in the project for
offline reopen. The existing `org.qupathxx.counts-tsv` provider remains
available for its count-only ABI1 profile.

Build offline with the included MSVC1944 Release FormatPluginKit:

```powershell
cmake -S source/plugins/tsv_matrix -B build -A x64 -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit
cmake --build build --config Release --target qpx_tsv_matrix_plugin
```

The module uses matrix ABI2 and shares the native CSV table interpreter.
QuPathxx source is AGPL-3.0-only. Each module has its own identity and
corresponding source package.
