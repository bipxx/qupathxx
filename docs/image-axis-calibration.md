# Saved physical Z/time calibration

R5-CALIB1 is [locally verified](benchmarks/axis-calibration-r5/README.md) for
recognized saved physical axes, owning Document/history/snapshot state, schema22,
native/direct-v2 import, Python and the quantitative inspector. Broader formats,
3D/temporal analysis, biological accuracy and supported release remain open.
The [capture manifest](../tests/fixtures/qupath-axis-calibration-0.7.0/manifest.json)
binds independent upstream references; the receipt separates original/repaired
native products and observer-only checks.

The [QuPath0.7 calibration source](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core/src/main/java/qupath/lib/images/servers/PixelCalibration.java)
stores regular Z spacing and explicit timestamps with a Java TimeUnit. The direct
saved-data readout covers all seven units, offset and nonuniform/nonmonotonic
times, finite partial prefixes and unknown calibration. An attempted NaN timestamp
is an observed QuPath serialization refusal. Do not replace timestamps with an
average interval, require monotonic order or invent values for an unknown suffix.

The source TIFF has Z spacing1.75um and interval0.25s; saved overrides deliberately
differ. Reassembled project entries can supply source-builder metadata when
reading saved data. `reference.json` retains that observation; only direct
`saved-reference.json` plus independently declared expectations is the saved-state
oracle. Seven valid first-attempt binaries were reused exactly after the NaN
failure. All executed probes and failures retain their own identities.

| Surface | Contract |
| --- | --- |
| `ImageAxisCalibration` | Immutable owning value: optional positive finite `z_spacing_um`, finite `timepoints_seconds` vector of at most65536. Negative offsets, repeated/decreasing times and nonuniform spacing are preserved. Zero is canonicalized; a missing timestamp suffix is unknown |
| `Document.image_axis_calibration` | Optional saved/editable override. Absence means use reader calibration. A present `(None, [])` explicitly means physical axes unknown, even when source calibration exists. Return owning copies |
| `Document.set_image_axis_calibration(value=None)` | One undoable set/clear with no-op detection, bounded history and atomic admission. Validate timestamp count against the source time domain; do not alter XY calibration or plane indices |
| Effective coordinate access | Optional `z_spacing_um`, `z_position_um(z)` and `timepoint_seconds(t)`. Validate source indices. With an override, unknown spacing/suffix returns unknown; without it, use the source regular spacing/interval. Reject nonfinite computed coordinates |
| Persistence | Optional per-image checked binary payload, preserving explicit unknown versus absent override. Reuse transactional store, snapshot ownership and old-version migration. QAC1 is bounded/checksummed/canonical; constructor validation is shared with decoding |
| Native saved data | Parse zSpacing and all seven TimeUnits once; scale timestamps to seconds without integer truncation. Reject malformed types/counts/nonfinite results and unsupported units. Recognized z-slice units mean unknown physical spacing. Missing whole saved calibration keeps source fallback |
| Assisted exchange | Direct import and explicit v2 native entries use the same native state path. Version1 and explicit GeoJSON retain their declared limits/losses. Do not edit frozen executed producers or serialize another native state copy |
| Source adoption | Preserve both current and reachable undo/redo calibration states. Reject a replacement whose time domain cannot represent a retained prefix, together with existing plane compatibility checks; no partial mutation |
| Python and desktop | Thin nanobind value/getter/setter wrappers; existing raw-image inspector shows saved/effective physical values and source provenance. Test real import/inspect/save; no separate UI model or sampler |
| Existing analyses | Reader acquisition metadata and frozen requests/results stay bound to their original source/recipe. This slice does not silently reinterpret historical records or claim new 3D/temporal analysis algorithms |

The receipt binds the exact supported contract: direct/v2 imports and malformed
refusal, immutable values and explicit unknown/source fallback, history budget,
source-domain replacement including reachable history, snapshots and checked
save/reopen, schema21 rollback/upgrade, coordinate overflow, missing-source
inspection, installed Python/stubs/typing and actual desktop inspection/save.
The initial ambiguous missing-time-unit behavior was repaired; a nonempty saved
timestamp array now requires an explicit supported unit. Failed evidence remains.

Unchanged state/persistence component checks retain their first-product scope;
changed decoder imports/refusals run at the repaired source. Two compiler inputs
changed for that repair. Existing raw acquisition metadata and historical
analysis records are preserved, with no new 3D/temporal analysis claim. The
[next task](current-task.md) handles project-entry metadata; version1/GeoJSON
losses and broader saved-data versions/records remain separately tracked.
