# R5-EDITSTATE1 object locks, color and reported probability

ObjectProperties now retains typed ROI locks, per-object24-bit colors and reported
class probabilities through native/Python commands, history, snapshots, schema17,
GeoJSON/CSV and ImGui. Editing state shares immutable metadata/measurement maps.
The lock is an interactive geometry hint: the UI blocks geometry edits until
explicit unlock, while scripted native geometry operations remain available.
Selection/classification/name/property edits and explicit deletion remain usable.
Reclassification clears stale reported probability and, for a named class, color
override; taxonomy rename preserves state. Undo/redo restores it exactly.

The [contract](../../object-properties.md#object-editing-state-editstate1-locally-verified)
defines absent/NaN state, raw reported values, color precedence and extension limits.
QOP1 remains canonical without new state; QOP2 encodes typed flags/color/probability.
The schema16 table is retained, with transactional upgrade to17. Schema16 rejects
QOP2. Native GeoJSON stores probability in a declared extension that foreign
readers may omit; it does not establish a QuPath probability roundtrip. Existing
settings/history/class-hierarchy losses and unsupported planes remain explicit.

[Validation](validation.json) binds final native585d6ba2 and observer9e72413b:
all451 compiler inputs, all62 installed members/RECORD and source stubs match.
The only source changes after candidateeb3c8f0 are the private classification
history header/implementation. No public API/UI correction or second installation
was needed. Exact final native products are retained locally with hashes.

- [Model](model.xml):764 passes/sixteen declared skips; all27 new non-GUI cases
  pass. Cases cover immutable state/script hints, five classification routes,
  taxonomy rename, atomic failed assignments, invalid colors, reported nonfinite/
  signed-zero semantics, flat/nested exchange, CSV, schema16 rollback, canonical
  checksum-valid malformed payload refusal and optimized snapshot ownership.
  The actual pinned saved-data fixture retains lock/color; a narrowly patched
  serialized probability word independently tests exact0.875 retention.
- [Native](native.xml):object_classifier, file_fingerprint, qupath_project,
  spatial_records, selection, project and interchange pass. The new codec marker
  checks version2/state and version1 refusal; native project persistence checks
  exact typed facts. All16 legacy schemas retain their migration assertions.
- [Desktop](desktop.xml):seven passes, including four new flows. A locked vertex
  drag leaves geometry unchanged; explicit unlock enables it and Ctrl+S/reopen
  retains the edit. Clear/unlock plus undo/redo retains exact state. An actual
  canvas pixel check proves the unselected color override and overlay-cache hits.
  Existing unsupported/duplicate/nested exchange flows retain their contracts;
  the refusal fixture adds a real unknown field to a temporary copy because the
  original lock/color/measurement fields are now supported.
- [Locked inspector](locked-inspector.png) and [color rendering](object-color.png)
  were visually inspected. Selection highlighting still takes precedence.
- [Stubs](stubs.log), [valid typing](typing-valid.log) and [nine intended bad calls](typing-invalid.log)
  pass. [58 pre-build auditor controls](observer-before-build.xml) retain tool-only scope.

Waste review and retained failures:

- [Before API checks](before.xml) fail twice on the preceding installed product,
  which lacks editing-state arguments. The first full cached build succeeds in
 681.98s. The initial staged probe used the root development interpreter; its
  editable hook loaded a wrong source/module and raised RawTmaContext import
  failure. [That log](history-budget-first.log) proves no new-product behavior.
  Use a dedicated test interpreter and assert actual compiled source before probes.
- The corrected uninstalled stage [budget witness](history-budget-staged.xml)
  fails a1000-object/64KiB history case that [already passed](history-budget-before.xml)
  on the preceding product. Unconditional property pointers enlarged every
  classification row. The sparse patch retains compact class-only rows and stores
  owners only for changed editing state. The exact witness now passes in model.xml.
- Private transaction/provenance rebuild and relink take56.56s, reusing API/UI
  compilation. Only the settled final product is installed, once. Stubs are
  generated once and repackaged without changing native bytes. All probe failures
  and both builds remain; no healthy runtime cohort had to be rerun.
- The property definition is forward-declared by core.hpp and its bindings live
  in a small module with explicit casters. C++ source clients inspecting the owner
  include object_properties.hpp; the pre-release C++ extension contract remains
  R15. Lock toggles share facts instead of copying maps. This is structural work
  reduction, not a general timing/performance claim.
- CSV now admits at most4096 selected property columns before materializing rows;
  the independent4097-column case refuses. Whole-export RSS/latency and wider R12
  resource/accuracy-matched speed claims remain open.

The [proof](prove_editstate_r5.py) ran from root build/ci with the dedicated
object-edit-state-r5-env interpreter. Archived helpers describe original local
paths under build/object-edit-state-r5. Wheels/native products remain local.
This is local Windows engineering, with no platform/science/release campaign.
Next deliver a source/view/analysis-aware non-default plane workflow; merely
storing plane labels cannot complete that contract. All M0-M7 remain Partial.
