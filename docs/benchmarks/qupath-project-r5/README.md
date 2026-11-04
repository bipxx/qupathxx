# R5-QPPROJ1 reviewed QuPath project import

Native source `ee7555e8de6fdc5e182e931e17adb5fa7f8c198c` implements
QuPath0.7.0 project-manifest preview/import using existing native image readers,
loss-aware GeoJSON and atomic native snapshots. Observer/exporter source
`78254e7b9223352779dd5127812dccffdcead713` supplies final tests, generated
stubs and the read-only assisted exporter. [Validation](validation.json) binds
440 compiler inputs, installed native/wheel bytes and retained products with
explicit cache paths. This is local Windows engineering evidence.

The [migration contract](../../annotation-migration.md#review-and-import-a-qupath-project)
describes Python and ImGui use. The [pinned fixture](../../../tests/fixtures/qupath-project-0.7.0/manifest.json)
contains two actual QuPath project entries, saved binary hierarchies and a
hash-bound exchange, using synthetic RGB and uint16 TIFF sources. There is no
patient material or biological acceptance claim. Native import maps source
object identities to new native IDs and preserves the tested names, parent links,
class/color, pixels and anisotropic calibration through native save/reopen.
This delivery uses a one-time QuPath-assisted export for saved objects; native
Java .qpdata decoding and additional builder/property/plane contracts remain open.

Evidence:

- [Before](before.xml): the previous SDK fails the import workflow because the
  API is missing. The fixture-integrity case passes.
- [Model](model.xml): 242 passes and four declared skips across selected project,
  interchange, hierarchy and evidence-observer checks. Two skips are new opt-in
  desktop cases; the other skips retain their original identities in XML.
- [Additional targeted checks](optional-metadata.xml): two passes for the final
  exporter/fixture identity and missing optional metadata plus unknown XY
  calibration. The latter is a synthetic exchange-contract test, not upstream
  unknown-calibration accuracy evidence. It verifies reopen, class and hierarchy.
- [Desktop](desktop.xml): preview-only and existing project-switch/history checks
  pass; the initial apply test fails because it omitted the existing dirty-workspace
  replacement confirmation. [Corrected apply](desktop-final.xml) passes using the
  same installed product. The production protection remains intact. By named
  identity across these files, all23 new cases and three selected desktop cases
  pass. The [inspected capture](import-final.png) shows both imported images and
  nested parent/child outlines. It is a lossless conversion of the retained PPM.
- [Native](native.xml): qupath_project, project and interchange pass with the
  explicit new preview/source-guard/publication/replay marker. The extracted strict
  JSON validator retains existing GeoJSON duplicate/depth rejection behavior.
- [Typing](typing-valid.log), [two intended negative errors](typing-invalid.log)
  and [installed generated-stub check](stubs.log) pass. All62 installed package
  members match the wheel, and every RECORD hash/size verifies. Native bytes are
  unchanged by stub repackaging; there was one final installation.
- [Exporter controls](export-controls.json) and their linked per-case logs show
  actual QuPath read-only and optional-metadata success, corrupt-data and ineligible
  builder refusal. Inputs remain unchanged and no server cache is created.
  The ordinary read-only exchange bytes equal the pinned exchange exactly.
  Optional absent metadata is represented with explicit null bindings.

The archived [proof](prove_qupath_r5.py) was executed from the root checkout's
`build/ci/prove_qupath_r5.py` using
`build/ci/qupath-project-r5-env/Scripts/python.exe -I -X utf8`.
Its paths deliberately describe that retained local layout; copied evidence is
not a claim that binaries/runtime bundles are distributed here. Original outputs
and products remain under `build/qupath-project-r5` at the root checkout.
[Validation commands](validate_qupath_r5.py) and per-mode command JSON retain the
selected check arguments. The two later pytest invocations used the same installed
Python: corrected desktop selected the apply=True case with QUPATHXX_TEST_GUI=1;
additional checks selected the two named cases in optional-metadata.xml with
QUPATHXX_TEST_GUI=0. They used distinct desktop-final/optional-metadata output paths.
No unchanged model/native cohort was replayed for those corrections.

Waste review: the initial [build](build-exit.json) failed for missing ascii helper
includes; the [corrected cached build](build-final-exit.json) succeeded. There were
two attempts and one success, not one build. A small separate public header/binder
avoided changing the shared core header and main bindings unit. Total build time
was not captured. Generated stubs reused staged native bytes and one final install;
previous JPEG archives remain unchanged. The exporter now uses direct throwing
PathIO reads rather than fallback-to-empty data, and avoids ProjectIO source cache
writes. A [null serialization probe](null-json.log) exposed omitted null fields;
serializeNulls applies only to the index. The dirty-workspace test correction ran
only its failed case. Capture conversion first used a Python without Pillow, then
used the existing project environment; no dependency installation or rerender was
needed. Documentation/proof closeout reused completed results without another
native build, reader campaign or full platform matrix.

All M0-M7 and wider R4/R5 requirements remain Partial. Source308 retains its own
platform scope. Next: inventory the pinned .qpdata serialization contract for a
bounded native decoder, reusing these binary fixtures and the existing geometry
and publication authorities. Do not build another assisted exporter or general
Java runtime/plugin layer.
