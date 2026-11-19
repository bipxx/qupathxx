# CSV expression matrices

The optional Windows x64 native provider `org.qupathxx.csv-matrix` reads wide,
comma-delimited UTF-8 tables with one observation per row. Select the ID column,
ordered feature columns and value domain explicitly. Each selected ID is stored
as exact text, including leading zeros. Header names and values may be quoted.
Within a quoted field, two double quotes represent one literal quote.

```powershell
qupathxx_format_import --format-plugins C:/plugins/csv/format-plugins.json matrix-project-index org.qupathxx.csv-matrix expression.csv positions.tsv study.qpxproj 1 expression.qmx --plugin-options-json '{"observation_id":"spot","feature_columns":["Gene.B","Gene.A"],"value_domain":"normalized-expression"}'
```

The positions TSV supplies `observation_id`, `x_px` and `y_px`. The native
host requires a complete ID join and checks the saved image frame. Supply the
saved image reader manifest when applicable. Use `matrix-attach` to keep an
owned QMX that can reopen independently of the input CSV and positions TSV.

Use `raw-counts` for unsigned decimal counts within UInt64. Other explicitly
named domains take finite binary64 values parsed from decimal text, including
exponents. Missing cells, numeric whitespace, decimal commas and nonfinite
values are rejected in selected feature columns. Unselected columns are parsed
for CSV structure and UTF-8 validity. Rows must be rectangular with distinct
nonempty column names. Input supports an optional UTF-8 BOM, LF or CRLF, and an
optional final terminator. Empty rows and multiline fields are outside this
profile. The reader allows up to 64 columns, 100,000 observations, 4,096 bytes
per field and 256 bytes per identifier within the host's file and work limits.

Build offline with the included MSVC1944 Release FormatPluginKit:

```powershell
cmake -S source/plugins/csv_matrix -B build -A x64 -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit
cmake --build build --config Release --target qpx_csv_matrix_plugin
```

The module uses matrix ABI2. QuPathxx source is AGPL-3.0-only. The plugin
requires no third-party parser or codec.
