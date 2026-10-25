# Annotation migration and hierarchy

The pinned QuPath 0.7.0 [hierarchy corpus](../tests/fixtures/qupath-hierarchy-0.7.0/manifest.json)
is produced by an actually executed [Groovy generator](../tools/upstream/qupath_hierarchy_reference.groovy).
It contains six synthetic objects, two roots, four parent links and three object
levels, including a cell with an independent nucleus, a detection, points,
Unicode names and a flat class/color. No real patient images or QuPath runtime
files are included.

The current migration route uses existing GeoJSON and native project storage:

1. Export the intended objects from QuPath. Ordinary `exportObjectsAsGeoJSON`
   flattens hierarchy. To retain parents, the generator uses QuPath's public
   hierarchy serializer and wraps the serialized root's `childObjects` as a
   GeoJSON FeatureCollection. This is an explicit Groovy export recipe; it is
   not a claim that QuPath's ordinary export includes those links.
2. In **Import / export**, choose the file, enable **Assume image pixels** for
   untagged QuPath output, and import with F7. Keep **Allow lossy conversion**
   disabled unless the reported losses are acceptable. Image coordinates do
   not supply physical calibration; set or preserve calibration separately.
3. Review the imported objects, edit, undo/redo, then save a native `.qpxproj`.
   Native project restart preserves the tested parent links, IDs, class/color,
   body/nucleus geometry and anisotropic calibration.
4. For a return to QuPath, enable **QuPath hierarchy (nested export)** before
   F6. This writes the existing GeoJSON geometry with nested `childObjects`.
   Actual pinned QuPath imports retain all six tested objects and four parent
   links, including the cell nucleus and point coordinates. Python/C++ use the
   same exporter with `nested_hierarchy=True`. The default remains flat native
   GeoJSON; QuPath ignores its `qupathxx.parentId` extensions. The UI explains
   both choices before export. Never guess parents from containment.

| Field | QuPath nested → native → native project/reimport | Native flat GeoJSON → QuPath |
| --- | --- | --- |
| Rectangle/point body geometry and cell nucleus | Preserved for the committed corpus | Preserved for the committed corpus |
| Names and flat class/RGB | Preserved in the data | Preserved |
| Object kind | Annotation, detection and cell preserved | Preserved |
| Parent links | Four links preserved, including three object levels | Four links lost; native extensions are ignored |
| UUID/native IDs | Source UUIDs reported; new native IDs allocated and retained by native project | Native numeric IDs replaced by QuPath UUIDs |
| Object color, lock, measurements | Reject by default; explicit opt-in drops three properties and reports `properties_dropped` count 3 | These fields are absent from the native export |
| Metadata | Pinned QuPath raw export repeats the `metadata` member; native import rejects the duplicate even with loss opt-in | Native object metadata is not represented |
| Calibration | Set/preserved in native project, not inferred from pixel-coordinate GeoJSON | Not conveyed by this object export |
| Unused class definitions | Native complete collections retain them; this upstream corpus does not exercise unused definitions | Not qualified by this corpus |
| Holes, multipart and ellipse precision | Covered separately by the existing [shape comparison corpus](qupath-comparisons.md) | Do not infer additional hierarchy coverage from those shape tests |

The raw `metadata-raw.geojson` is retained exactly. QuPath's pinned serializer
writes identical metadata twice in this path; rewriting the file through a JSON
dictionary would hide the invalid duplicate. Strict rejection preserves the
native duplicate-member contract. General metadata preservation needs a separate
object-model contract.

## Evidence and remaining work

The nested option is additive; omitting it keeps the previous flat output
byte-for-byte for the committed corpus. It retains selected objects only. A
missing selected parent requires `allow_losses=True` and reports
`parent_not_exported`; the descendant becomes a root, including when a more
remote ancestor is selected. No implicit ancestor expansion or reparenting occurs.
Nested output is bounded at one million objects, 32 object levels and 256 MiB.
The iterative writer avoids recursive subtree copies. The 32-level boundary is
native-tested; the actual upstream corpus has three levels. Conversion and depth
errors occur before file publication and preserve an existing destination.

The [current local receipt](benchmarks/annotation-migration-recovery-local-windows.json)
identifies the installed engine at compiled source
`df0733c3782f99518e89d7e2f40787dbb2be3094`, wheel/module hashes and all 268
compiler inputs. Fifty native suites and 305 installed cases pass. Three host
capability skips remain explicit: one OpenSlide fixture case and two file-symlink
cases unavailable on this Windows host. The existing hierarchy/interchange cases,
four desktop migration cases, project save/recovery and additional recovery
maintenance/retention/UI checks are included. Ten long-path cases also pass after
adding portable filename-limit assertions. Packaged stubs, four positive typing
files and all 13 expected negative errors pass. Two actual pinned QuPath processes
verify Python and GUI exports separately, including point coordinates and parents.

The earlier [partial integration receipt](benchmarks/annotation-migration-nested-local-windows.json)
retains the Windows recovery staging failure. The fix reuses the batch engine's
physical-path policy through `cpp/filesystem_access.hpp`. Physical work paths,
SQLite connections and recovery filesystem operations use extended Windows paths;
public project/base/image paths and recovery listings stay logical. Owned work
names/markers, writer leases, inventory checks, non-overwriting rescue and checked
deletion policy are unchanged. Tests cover 230/270-character checkpoint paths,
longer derived work/snapshot/rescue paths, reopening and discovery, changed-since-
review rejection, and live-session retention followed by release. Drive/UNC
conversion is native-tested; a real network share is not qualified by that test.

The old wheel fails all five original long-path reproductions. The corrected
engine passes the broader gate at the same 79-character absolute root length;
it does not hide the failure by shortening the test directory. Raw failures and
successful XML/logs are retained separately. The original
[main154 preparation receipt](benchmarks/annotation-migration-local-windows.json)
is still bound to its historical source `15f5888`.

The inspected final nested-export capture has readable mode/depth text and
`Exported 6 objects`, with body/nucleus/points visible. It is byte-identical to the
inspected initial capture. The default font displays the alpha in the parent
label as `?`; Unicode data remains exact. Unicode glyph delivery remains open.
Source-bound hierarchy/recovery qualification cohorts, broader hierarchy/property
contracts, direct `.qpproj`/`.qpdata`, full platform gates and supported-release requirements remain in the full plan.

Run the installed tests from the repository root:

```powershell
python -I -m pytest tests/test_qupath_hierarchy_reference.py tests/test_geojson_hierarchy_export.py tests/test_hierarchy_comparison.py -q
$env:QUPATHXX_TEST_GUI = '1'
python -I -m pytest tests/test_gui_hierarchy_migration.py -q
```

To regenerate with the verified Windows runtime, use `tools/fetch_qupath.py`,
then invoke the returned executable with `script --args NEW_OUTPUT` and the
generator path. A second invocation accepts `--args NATIVE_EXPORT` before the
generator path. The generator requires a new output directory. Preserve raw
logs and compare the declared contract before deliberately replacing fixtures;
tests never regenerate them implicitly. The comparison is image-object migration,
not Java serialization compatibility or a supported release claim.

The live checker verifies the complete pinned runtime and never updates fixtures:

```powershell
python -I tools/compare_qupath_hierarchy.py --output build/hierarchy-roundtrip-new
python -I tools/compare_qupath_hierarchy.py --output build/hierarchy-gui-new --native-input path/to/gui-export.geojson
```

Outputs must be new directories. The GUI-input mode records the checker engine;
its producer identity belongs in the originating capture receipt. The current
receipt binds both to the same installed module. This comparison qualifies the
committed object corpus, not arbitrary geometry, research workflows or releases.
