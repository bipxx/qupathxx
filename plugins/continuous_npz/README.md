# Native NumPy continuous archive provider

`org.qupathxx.continuous-npz` implements the native matrix ABI2. Use
`numpy.savez` or `numpy.savez_compressed` with these exact keyword names.

| Array | Required content |
| --- | --- |
| `values` | Rank-two finite Float32 or Float64 values in observation-by-feature order |
| `observation_ids` | Rank-one fixed byte or Unicode IDs in row order |
| `feature_ids` | Rank-one fixed byte or Unicode IDs in column order |

Both byte orders and C/Fortran storage orders are supported. Float32 values
widen exactly to Float64, including negative zero and subnormal values. IDs
retain exact UTF-8, including leading zeros, up to 256 bytes. Empty IDs,
embedded controls, duplicate IDs, mismatched shapes, nonfinite values and
other array names cause import to fail.
[NumPy array format](https://numpy.org/doc/stable/reference/generated/numpy.lib.format.html)

The three ZIP members may be stored or Deflate-compressed, including NumPy's
local ZIP64 size fields within this profile. The reader checks member metadata,
extents and CRCs. The archive must fit both 64 MiB and the host file allowance.
Decoded arrays and copied IDs must fit the requested work budget.

```text
qupathxx_format_import --format-plugins ABS_MANIFEST matrix-index org.qupathxx.continuous-npz ARCHIVE POSITIONS IMAGE NEW_QMX --plugin-options-json '{"value_domain":"normalized-expression"}'
```

Set `value_domain` to the source's interpretation, an explicit UTF-8 label of
1–128 bytes other than `raw-counts`. Values retain their supplied scale.
`POSITIONS` is an `observation_id`, `x_px`, `y_px` TSV with one position per ID.
`IMAGE` establishes the level-zero coordinate frame. The native position join
and QMX writer retain source, interpretation, coordinates and provider identity.
The QMX can reopen without its input files or plugin.

Build Release with `QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX` set to a matching ABI2
kit and `QUPATHXX_NPZ_ZLIB_ARCHIVE` set to the admitted zlib 1.3.2 source archive.
The AGPL-3.0-only plugin links private prefixed zlib and ships its original
source and notice. NumPy is the development-only producer and oracle.
`tools/ci/check_continuous_npz_plugin.py` exercises the installed workflow.
