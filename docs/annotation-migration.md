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
4. F6 exports native GeoJSON. Reimporting that file into qupathxx retains the
   tested hierarchy. QuPath ignores native `qupathxx.parentId` extensions and
   imports the six objects without parent links. Do not treat that route as a
   hierarchy-preserving return to QuPath or guess parents from containment.

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

Twelve captured-reference/native persistence cases and three real ImGui cases
pass locally using the accepted main154 full-feature Windows wheel, compiled at
`f2e8c2ff5b044fd530c959c6d9c3b874d37a31b6`. The interchange implementation/header
are unchanged in this preparation branch. This is explicit reuse of an accepted
engine for a new comparison, not an installed candidate158 qualification.
The [receipt](benchmarks/annotation-migration-local-windows.json) identifies the
wheel, extension, fixtures, results and captures. Both captures were inspected:
the six imported objects and nucleus are visible; loss opt-in and the counted
loss message are readable. The default font displays the alpha in the parent
label as `?`; Unicode data remains exact. Unicode glyph delivery remains open.

The workflow is still partial. A researcher returning a hierarchy to QuPath
needs a supported nested export path with clear target/loss semantics. The
existing export UI does not warn that another application ignores native parent
extensions. Resolve that user-facing gap before claiming bidirectional migration
completion. Broader hierarchy edge cases, direct `.qpproj`/`.qpdata`, unsupported
properties and platform qualification remain tracked by the full project plan.

Run the installed tests from the repository root:

```powershell
python -I -m pytest tests/test_qupath_hierarchy_reference.py -q
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
