# R5-PROP1 retained object metadata and measurements

ObjectProperties now owns imported string metadata and named binary64 measurements
through native annotations, commands/history, snapshots, project save/reopen and
GeoJSON/CSV export. Ordinary geometry/calibration edits preserve these historical
facts without treating them as recomputed or verified analysis. Explicit replacement
and clearing are undoable. ImGui's selected-object inspector shows the retained
facts separately, labelled historical. Unsupported fields/locks/planes retain
refusal or explicit loss reports; this does not complete all foreign properties.

The [contract](../../object-properties.md) defines bounds, special values, ownership,
merge refusal and schema16/QOP1 migration. The pinned actual QuPath0.7.0 saved-data
fixture now retains `note=saved note` and `Synthetic=3.5` without their prior loss
codes; remaining color/lock/settings/history losses still require explicit admission.
No Java runtime/code execution or new library dependency is introduced.

[Validation](validation.json) binds final product0effd30d, native-test source3095e6f8
and stub/benchmark observerac6b3089 separately. All449 compiler inputs and all62
installed members/RECORD verify. The only compiler-input difference after native
tests is the missing map-caster include in bindings.cpp. The five tested native
executables were retained with hashes before the corrected source relinked them.

- [Original model run](model.xml):724 passes/twelve declared skips and two
  descriptor-benchmark failures. All25 new non-GUI property cases pass: detached
  Python dictionaries, retained owners, exact special values/signed zero, history,
  atomic command/import refusal, bounds, GeoJSON/CSV, migration/rollback, snapshot
  isolation/budgets and malformed/orphan rows.
- The stale benchmark required schema14. It now reads the exact native schema
  declaration. The first [observer retry](model-final.xml) matched a second batch
  constant; anchoring the name fixes that. Only the [two failed cases](model-corrected.xml)
  were rerun and pass:726 distinct selected model passes total, twelve skips.
- [Native suites](native-first.xml):qupath_project, project, interchange,
  spatial_records and file_fingerprint pass, including the new immutable-property,
  special-number, checksum, history, native persistence and exchange markers.
- [Desktop](desktop.xml):imported-property inspection/actual Ctrl+S/reopen and the
  existing native saved-data import both pass. The [capture](object-properties.png)
  visibly shows retained metadata and Synthetic3.5 beside historical-state wording.
- [Generated stubs](stubs.log), [positive typing](typing-valid.log) and
  [four intended bad calls](typing-invalid.log) pass their respective contracts.
  The [54 pre-build auditor controls](observer-before-build.xml) remain in their
  tool-only scope; two new final-schema omission/failure controls run in model.xml.

The [before checks](before.xml) fail on the previous product for missing native
properties and explicit GeoJSON loss. The first build succeeds in953.27s, but its
[pre-install import probe](candidate-probe.log) fails with bad cast: the new map
arguments lacked nanobind/stl/map.h. No invalid product was installed. The build
finished before an attempted reviewed stop could target its old process; no process
was stopped. A binding-only cached correction takes457.69s. Both builds and the
failed probe are retained. One exact-product stub generation, byte-preserving
repackage and final installation follow. The [proof](prove_props_r5.py) checks the
native bytes were unchanged by stub packaging and every actual package member.
A guessed extra wheel member was removed from proof: this wheel does not ship C++
headers. No build or healthy test rerun was needed for that observer correction.

Waste review: check STL caster includes with new container arguments before a
build; main bindings remain an expensive compiler unit. Reuse compiled core and
five native results after the include-only fix; rerun only failed benchmark cases,
not724 passing cases or the desktop/typing flows. Derive the benchmark schema from
the existing declaration rather than scheduling another literal repair at every
migration. Preserve exact product/observer sources and failures. No reader, science,
full-platform or performance campaign was run. Keep whole-export resource bounds
under R12; these per-object limits do not establish whole-slide memory budgets.

Original helpers ran from root build/ci; products/wheels stay local under
build/object-properties-r5. Final scope is local Windows engineering. All M0-M7
remain Partial. Next preserve the remaining imported editing state (locks, colors,
class probability) through the existing command/renderer/storage authorities;
non-default planes and other representation versions retain separate contracts.
