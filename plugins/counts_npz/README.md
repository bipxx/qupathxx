# Native NumPy counts archive provider

`org.qupathxx.counts-npz` implements the native matrix ABI1 and imports an
explicit three-array NumPy archive. Use `numpy.savez` or
`numpy.savez_compressed` with these exact keyword names:

| Array | Required content |
| --- | --- |
| `counts` | Rank-two integer counts in observation-by-feature order |
| `observation_ids` | Rank-one fixed byte or Unicode IDs in row order |
| `feature_ids` | Rank-one fixed byte or Unicode IDs in column order |

Counts preserve unsigned 8-, 16-, 32- and 64-bit values and admit nonnegative
signed integers of the same widths. Both byte orders and C/Fortran storage
orders are supported. IDs preserve exact UTF-8, including leading zeros,
up to 256 bytes. Fixed byte strings are UTF-8 and fixed Unicode strings
are decoded from the NPY code units. NUL padding is removed at the end of
each field. Empty IDs, embedded controls and duplicates refuse. Object,
structured, floating and other unrepresented dtypes require separate profiles.
[NumPy array format](https://numpy.org/doc/stable/reference/generated/numpy.lib.format.html)

The provider admits three stored or Deflate members and checks ZIP metadata,
extents and CRCs. NumPy's local ZIP64 size fields are supported within the
bounded profile. The archive is limited to 64 MiB. Decoded arrays and copied
IDs fit the host's requested work budget. Entries are interpreted in memory.
Native options are empty.

Use `qupathxx_format_import --format-plugins ABS_MANIFEST matrix-index
org.qupathxx.counts-npz ARCHIVE POSITIONS IMAGE NEW_QMX`. `POSITIONS` is the
native `observation_id`, `x_px`, `y_px` TSV and `IMAGE` establishes the
level-zero coordinate frame. The native host validates every ID match,
publishes a new QMX, and retains the source, provider and position identity.
The resulting QMX can reopen without the input archive or plugin.

Build Release with `QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX` and
`QUPATHXX_NPZ_ZLIB_ARCHIVE` pointing to the matching kit and admitted zlib
1.3.2 source archive. The optional module ships with the original Zlib notice.
NumPy is a development-only producer and oracle. The native SDK test is in
`tests`, and `tools/ci/check_counts_npz_plugin.py` exercises the host workflow.
