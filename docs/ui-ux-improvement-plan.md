# UI/UX improvement plan

Status: all five phases implemented and checked in the active integration checkout.
Owner authorized merge and launch; the handoff result is recorded in workspace-root
`build/ui-optimization-merge/results.json`. Focused evidence: workspace-root
`build/ui-export-sampling/results.json`. The prior legacy tight-grid fixture
refusal is tracked separately in R6/R12 and reproduced with the pre-change app.

Baseline: merged UI at `d7db4671`. Claude Fable 5.2 reviewed the original plan
with explicit high effort. The primary agent checked the recommendations against
source and incorporated the decisions below. The steps retain their planning
names; concrete delivered authorities include `export_output.hpp`,
`gui_export.hpp`, `sampling_summary.hpp`, `gui_measurements.hpp` and
`selection_ranges.hpp`. No scientific kernel or frozen binding was changed.

Evidence, relative to the workspace root:

- `build/claude-ui-plan-review-fable-5-2-high/plan-before.md`: immutable original.
- `build/claude-ui-plan-review-fable-5-2-high/review.md` and `review.json`: peer review.
- `build/claude-ui-plan-review-fable-5-2-high/validation.json`: integration decisions
  and documentation checks for this revision.
- `build/claude-ui-review-fable-5-2-high/review.md`: original five UX findings.
- `build/ui-dialog-contract/results-final.json`: existing native UI qualification;
  reuse compatible evidence, not its claim for changed behavior.

The outcome is a safer, clearer path from selecting an image or ROI to running,
reviewing, retaining and exporting results. Keep the current workbench and
Project browser. Extend the [design system](design-system.md) and
[dialog contract](ui-dialog-contract.md). No full QuPath parity, UI rewrite,
scientific feature campaign or pixel-equivalence target is selected.

## Delivery order and ownership

| Phase | User outcome | Implementation ownership |
| --- | --- | --- |
| 1. Export safety | Understand the destination and approve replacement | Native writers/job entry points, one GUI export controller, registered dialog, replay |
| 2. Detection units | Understand units and effective sampling before Run | Existing native request builders/accessors, summary facts, shared controls |
| 3. Overlay visibility | Inspect tissue without dense object outlines | GUI presentation state, shell snapshot/commands/resources, overlay cache |
| 4. Measurements and results | Inspect the selected object and understand Keep/export | Annotations inspector, Workflow results, Tables |
| 5. Object browsing | Find annotations among many detections | Filtered list cache and native ordered-selection helper |

Milestone 1 delivers phases 1 and 2. Milestone 2 delivers phases 3 and 4.
Milestone 3 delivers phase 5. Later phases do not gate the safety correction.
Use the existing integration checkout/cache. These changes overlap `gui.cpp`;
implement sequentially rather than creating panel worktrees. Preserve unfinished
analysis/SDK edits, passing products, fixtures, backups and the frozen bindings.

## Confirmed boundaries and compatibility rules

- C++ remains the authority for validation, sampling, selection, persistence,
  export and job state. GUI formatting may localize native facts; it must not
  reproduce scientific calculations or resolve sampling independently.
- `AtomicExportFile::commit(bool replace=true)` already supplies atomic create-only
  publication through `commit(false)`. Reuse it; do not invent another mechanism.
- Existing bound functions/methods are taken by address. Add distinctly named
  policy entry points; do not overload those names or alter their signatures.
  Existing SDK/CLI defaults continue replacing files as before.
- GUI exports create new files by default. Replacement requires a registered
  confirmation. Protected-source refusal always wins over confirmation.
- Keep retains a native result. Save project persists project state. Do not
  auto-keep, change recipe identity or imply that live-object exports require Keep.
- Overlay visibility and list filtering are presentation state. They must not
  change objects, selection eligibility, scientific results or export scope.
- Preserve public enum numeric values, command IDs, docking and localization.
  Add only identifiers required by the selected milestone. No binding rebuild,
  wheel, Python consumer execution, release or merge is selected by this plan.

## 1. Make import and export explicit and safe

### A. Add native policy without breaking bound names

Ownership: `cpp/include/qupathxx/interchange.hpp`, `cell_tables.hpp`,
`measurement_tables.hpp`, `file_fingerprint.hpp` and their implementations.
Proposed new shared header: `cpp/include/qupathxx/export_policy.hpp`.

1. Define proposed `enum class ExportWritePolicy { CreateNew, ReplaceExisting };`.
   Reuse the existing `WorkflowOutputExists` exception from `workflow.hpp` for
   an existing destination; its inherited runtime-error constructor fits.
2. Add distinctly named `save_geojson_with_policy(...)` and
   `save_object_measurements_csv_with_policy(...)`. Carry the current document,
   path, selection/options, policy and owned protected-path list to the writer.
   Existing `save_geojson` and `save_measurements_csv` forward with replacement
   and retain their existing signature and behavior.
3. Add proposed `request_with_policy(request, path, policy, protected_paths)` to
   `CellTableExportJob` and `MeasurementTableExportJob`. Copy/move protection into
   the worker; never capture GUI references. Existing `request` forwards with
   replacement and no new caller-imposed protection.
4. Pass policy through the private table writers and interchange `atomic_write`.
   Call `commit(policy == ExportWritePolicy::ReplaceExisting)`. Map a create-only
   collision to the existing-destination error; preserve other native errors.
5. Leave `CellTableOptions`, `MeasurementTableOptions`, request identities and
   scientific serialization unchanged. The filesystem policy is not a recipe
   parameter. Do not change `AtomicExportFile` or the SDK viewer-session route.

### B. Enforce protected paths at the native write boundary

These protections are new work for GUI GeoJSON/object-CSV/table routes; an
existing CLI guard does not protect them. Fingerprint output already protects
its own input and companions. Reuse `detail::guard_file_output` from
`cpp/reader_file_guard.hpp` with `detail::export_path` normalization.

Add a shared private collector, proposed `cpp/export_protection.hpp`. Its inputs
must include the project, active document, workspace layout path and the current
owned autosave destination(s), not merely `Project::recovery_source()`. Collect:

- Open project and recovery-source paths, plus their SQLite `-journal`, `-wal`
  and `-shm` sidecars.
- Current owned autosave/recovery destinations and applicable sidecars.
- Every non-embedded project-image source path, without opening each reader.
- Active reader source locator, primary source and companion files. Preserve
  directory-store protection; reject outputs within a protected directory store.
- Workspace layout path.

Check through the same native guard before creating the temporary file and
again immediately before commit. Apply both checks for both policies. Return a
refusal with no Replace offer. Do not hard-protect the editable Import source
field: source/destination separation plus explicit replacement handles that
accident. Preserve stronger fingerprint/source-association protections.

### C. Capture one export intent and validate ownership

Proposed private controller: `cpp/gui_export.hpp`, called by `gui.cpp`. Keep one
pending intent, reject duplicate dispatch and show native progress/errors in the
originating task.

| Captured field | Required meaning |
| --- | --- |
| Operation and scope | Exact route, image/project/selected scope and format |
| Destination | Owned normalized path, independent of subsequent field edits |
| Options and IDs | Copied options and actual exported IDs; never a live selection reference |
| Owner stamp | Project/document identity, active image, relevant revisions and selection revision when selection matters |
| Project scope stamp | Relevant member-document identities/revisions as well as project state; do not check only the active image |
| Protection | Owned path list from the native collector |
| Reviewed destination | Native file-version token when an existing destination is shown |

Use existing identity/revision authorities. Compare owner stamps at confirmation
and dispatch; full project validation belongs there, not in a per-frame project
scan. When context changes, refuse the old intent and require a fresh export.
`detail::workflow_file_version` supplies a metadata/stamp token, not a content
hash or cross-process compare-and-swap guarantee.

Controller transitions:

1. `Idle -> Preparing`: normalize/validate path, collect/check protection and
   capture ownership/options.
2. Construct a native table request to surface eligibility/currentness errors
   before asking about replacement. This may snapshot substantial data. Do it
   once per intent, never while merely drawing Tables.
3. An unused path dispatches `CreateNew`. An existing file enters
   `AwaitingReplacement`. Release the table snapshot before leaving a modal open;
   keep its summary/stamp, then rebuild and revalidate at dispatch.
4. Cancel/Escape dismisses with no write. Confirm rechecks protection, owner and
   destination token. A difference refuses dispatch and asks for a new review.
5. Dispatch exactly once, then `Running -> Succeeded | Failed | Cancelled`.
   Synchronous interchange uses the same controller and reports its terminal
   state; it does not gain asynchronous cancellation semantics.
6. A create-only collision during publication is a failure: report that nothing
   was replaced and require another export intent. Never auto-upgrade to Replace
   or automatically reopen a confirmation after the failure.

### D. Register the confirmation and define cancellation precisely

Append proposed `ReplaceExport` to `QPX_UI_DIALOGS` in
`cpp/gui_dialog_contract.hpp`; use the shared renderer in `cpp/gui_dialogs.hpp`.
Declare one destructive Confirm action labelled Replace file. Keep Cancel-first
order, default focus on Cancel, owner/busy locks and nested-popup Escape behavior.
Show wrapped full destination path, operation, scope and a clear replacement
warning. Do not rely on an ellipsized context line for the path.

Confirm returns success only after dispatch is accepted; validation/native errors
remain visible. A busy job cannot dispatch twice. Cancel before the atomic publish
preserves existing bytes and cleans the temporary file. The existing rename/link
is the commit point; cancellation arriving after it may honestly report success.
Replacement is unconditional at that point. Destination-version rechecking
narrows mistakes but does not promise protection against another process changing
the file between that check and publication. No backup feature is added.

### E. Route every actual entry point through the controller

| Route | Existing surface to change |
| --- | --- |
| A. GeoJSON | Annotations export button and F6; synchronous |
| B. Object measurement CSV | Annotations export button; synchronous |
| C. Saved cell-run table | Cells action and Ctrl+Alt+X |
| D. Image live-cell table | Tables action and Ctrl+Alt+X |
| E. Project cell table | Ctrl+Shift+X |
| F. Image/project ROI or tissue table | Tables actions and Ctrl+Alt+Q / Ctrl+Shift+Q |
| G. Optional fingerprint/source JSON | Tables fingerprint actions; create-only GUI publication |

Hoist shortcut intent dispatch out of individual pane rendering. Use existing
text/modal/editing gates and non-repeating `IsKeyPressed(key, false)`. Preserve
chords and replay kinds, distinguish shared chords by the active task, and make
one F6 tap from another pane behave consistently. Remove duplicate inline export
checks. Verify F7 import remains gated and uses the import field. There is no
current export menubar route; do not add one just to satisfy an inventory.

In `cpp/gui_text_buffers.hpp`, add a separate import-source buffer and retain
`interchange_path` as the export destination. Seed both from existing
`GuiOptions::interchange_path`, with no new public options field. Subsequent edits
are independent. Use full-width fields with visible translated labels, explicit
Export GeoJSON / Import GeoJSON / Export measurements CSV actions and a native
scope caption. Keep compact utility icons. Changed visible labels use stable
`###` IDs. Add strings in `cpp/gui_i18n_catalog.inc`; report errors through the
originating interchange/table/fingerprint status, not a competing global error.

Fingerprint scope decision: GUI JSON publication is create-only, including the
source-association writer reached by the source fingerprint route. Add distinctly
named policy job entry points as needed; old methods keep their behavior. Refuse
an existing destination before hashing, and retain native create-only enforcement
for files appearing later. The current job assigns its result only after JSON
publication, so failed publication does not retain the computed fingerprint.
Keep that failure contract, explain retry/new-path behavior, and do not promise
retry-writing without rehashing or silently retry the job. Replacement and result
retention improvements are deferred; create-only safety is not deferred.

### F. Update replay compatibility and prove the contract

Append proposed `ConfirmReplaceExport` and `CancelReplaceExport` to
`GuiInputTarget` in `cpp/include/qupathxx/core.hpp`; never insert/renumber values.
Mark the registered actions for native replay. In the same change, update both
the `target_seen` storage bound in `gui.cpp` and TargetReady range validation in
`cpp/gui_replay.hpp`, currently bounded by `SpatialExclusions`. Prefer a shared
private max/count helper. Test new target acceptance and unknown-value rejection.
Do not add unused phase-3 public identifiers in milestone 1.

Frozen bindings do not know the new enum values. Record the consumer updates for
the next authorized binding build: existing sentinel/byte-limit tests in
`tests/test_gui_cell_tables.py`, `test_gui_project_cell_tables.py` and
`test_measurement_tables.py` must confirm replacement before asserting writer
failure. The shared-path export/import test in `test_gui_tiles.py` must still
work when both fields are seeded from the existing option. Do not rebuild or run
those consumers now; native replay proves the new behavior.

Decisive native cases:

1. Create-only writes to an existing sentinel refuse without changing its exact
   bytes; cover GeoJSON, object CSV, both table jobs and fingerprint JSON routes.
2. Test `AtomicExportFile` create-only publication with a sentinel created between
   preparation and commit. Combine this with writer/job policy-propagation tests.
   Use the natural controller preparation/dispatch split for an intent-level race
   test; do not add a public test-only callback or claim cross-process CAS.
3. Confirmed replacement equals native serialized output exactly. Pre-commit
   failure/cancellation preserves the old file and leaves no temporary remnants;
   post-commit cancellation follows the existing committed-success contract.
4. Both policies refuse project, source, owned autosave/recovery, companion,
   directory-store and layout destinations. Confirmation cannot bypass guards.
5. Pure owner-stamp tests cover changed image/document/selection/project-member
   state and changed destination. Modal input blocking is not a substitute.
6. GUI replay covers new-file F6 with no modal, existing-file F6 then Escape/Cancel,
   explicit Confirm, one dispatch, cross-pane single tap, held key, text editing,
   busy state and import-source independence. Extend dialog registry/source guards
   and `docs/ui-dialog-contract.md` alongside the modal.

## 2. Explain detection units and effective sampling

### A. Share the request builders, not a second resolving algorithm

Extract the existing Tissue and Cells request construction from `cpp/gui.cpp`
into shared private builders, including feature/connectivity, nucleus, area limits,
statistics and sampled/grid/nominal options. Caption and Run call those builders.
Run constructs a fresh native request; never execute an old cached request.
Factories validate the selected area ROI/display plane and capture provenance.
They do not read image regions, but ROI copying, geometry validation and Auto
level trials have a cost.

The private native `cpp/sampling_summary.hpp` adapts request accessors
to facts only: mode, resolved level, per-axis level-zero sampling steps, optional
physical steps, grid dimensions and work estimate where the request exposes them.
For Tissue, use request provenance/calibration and sampling accessors. CellRequest
has no provenance accessor; pass calibration captured with the request, or add a
private adapter using that same capture. Reuse the native calibrated-product
helper in `cpp/calibrated_cell_numeric.hpp` and existing rounding/overflow rules.
Do not multiply scientific scale in GUI code or add a new resolution query.

Keep the localized caption formatter in the GUI layer. Show level/downsample or
grid dimensions and per-axis steps; show physical sampling only with valid native
calibration. Use Calibration unknown when unavailable. Place one compact caption
under essentials, above Advanced, without deleting useful after-run evidence.
No valid area ROI or a native validation error means Sampling unavailable plus
the native message, with no invented numbers.

### B. Cache by all input values

Use one result-or-error cache per task. Compare a value key every frame while the
task is drawn; compute only when it changes. Include server/document/image
identity, document/calibration revision, selected ROI identity, sampling mode,
every TissueParameters/CellParameters/GridCellParameters field, feature,
connectivity, nucleus choice, min/max limits, statistics flag and grid/nominal
values. Explicitly include all arguments consumed by the shared builder.
Recheck can restore saved parameters without setting a changed flag, so a
changed-flag-only cache is incorrect. Clear caches on image/source switches.

Do not build a request every frame or cache a table-like payload just to display
text. Geometry/Auto cost for extremely large inputs remains an implementation
risk: inspect the existing constructor path and measure only if a concrete stall
appears. Reuse native validation limits; do not weaken them or launch analysis
as a preview.

### C. Apply units per mode

| Control / mode | Truthful label or explanation |
| --- | --- |
| Sampled-cell sigma, expansion, peak radius, spacing | sampled px |
| Sampled-cell nucleus min/max | sampled-pixel count; counts, not lengths |
| Sampled-cell context | sampled pixels |
| Grid/nominal-cell distance parameters | level-0 px or um, from GridCellUnits |
| Grid/nominal-cell areas | squared selected length unit |
| Grid dimensions | samples |
| Nominal downsample | level-0 px per sample |
| Nominal context | output samples |
| Tissue sigma and morphology radii, including explicit grid | sampled px, meaning pixels of the sampled analysis array |
| Tissue min/max/hole size | sampled-pixel count |
| Threshold | selected signal-feature units; never a length unit |

`GridCellUnits` explicitly excludes sampled-array pixels. The tissue grid kernel
applies sigma/morphology to the sampled array; do not relabel these as level-zero
pixels. Preserve every value/default and recipe identity. Apply the same labels
in `cpp/gui_batch.hpp`; batch controls have no single resolved image/ROI, so add
no fabricated batch sampling caption. Use stable `###` widget identities and
localized labels; harmonize with the existing ASCII um convention.

Decisive checks: native summary equals the shared builder's request accessors
for Auto/manual, anisotropic grid and nominal modes, with/without calibration.
Compare against existing small-fixture result sampling where relevant. A counting
ImageServer proves zero region reads for construction plus summary. Exercise
input-key changes, restored parameters and errors. Reuse unchanged science and
identity assertions. Capture only changed controls in calibrated/uncalibrated,
narrow 200% and one translated view to verify readable units/errors.

## 3. Add a quick object-overlay visibility control

Ownership: `cpp/gui.cpp`, `gui_overlay.hpp`, `gui_workspace.hpp`, `gui_shell.hpp`,
`gui_shell.cpp`, `gui_icons.hpp`, `gui_icon_coverage.hpp` and
`assets/ui/workspace.rml`. This is milestone 2; do not pre-build it in milestone 1.

Use a GUI-only `objects_visible` boolean, default true. Do not route it through
`Document::object_visible`: that is plane visibility and selection synchronization
prunes invisible objects. Include the boolean in `OverlayKey`; hide published
object outlines/nuclei/labels without altering document state or cached identity.
Existing tissue/cell preview toggles keep their separate meaning.

All toolbar/menu/shortcut routes use one guarded dispatcher. Disable the toggle
during drawing, marquee, vertex drag or a non-empty draft. While hidden, canvas
picking and marquee leave selection unchanged. The first Delete restores objects
without deleting. Activating a non-Select drawing/editing tool, or applying a
geometry edit from a sidebar, restores visibility before interaction; block action
if that transition is not safe. Sidebar inspection/selection remains available.

Append the private shell command inside the appropriate toolbar range and update
all positional command/ID/icon arrays, range logic and coverage. Extend the typed
snapshot with defaulted presentation fields, not runtime/document pointers. Add a
smooth Eye icon through the existing vector-stroke/coverage system. Bump the RML
schema to 3: schemas 1-2 may omit the new control; schema 3 requires it. Keep
legacy Settings handling and resource-reload errors explicit.

Persist a workspace header marker with default-visible behavior for old files.
Show pressed state plus Objects hidden in every status-bar branch, including
errors, so hidden status cannot disappear. Bare O is a proposed shortcut: source
scan found Ctrl+O, but recheck tool-window collisions during implementation.
Require no text input, no modifier conflict, no modal/edit block and no gesture.

Checks: toggle/reload/image switch/pan/zoom repaint immediately; camera, selection,
object counts/revisions, project dirty state, exported/saved bytes and image RGB
remain unchanged. Native replay covers hidden pick/marquee/Delete/tool transition,
busy gestures and focus gates. Shell tests cover schema 1/2/3 and array alignment.
Add any public replay target only when this milestone actually needs it, updating
both replay bounds as in phase 1.

## 4. Bring measurements and result retention into the task

Reuse the existing `Document::shape_measurements` authority and its cached
object/calibration revision behavior. Extract one read-only GUI renderer and use
it beneath Selected object in Annotations and at the existing Workflow location.
Label primary-object values when several objects are selected. Show available
area/length/nuclear fields with honest pixel/physical units and unclipped values.
Do not recalculate measurements, add editable numeric results or invent values
for unsupported shapes. Copy values is optional, not a milestone dependency.

Clarify existing Keep actions beside results: retain the result for later use or
export; Save project persists it when a project is open. Without a project, say
retained in current-image memory; do not claim caller-owned Document data vanishes
when the window closes. Tables can also export live cell objects: avoid wording
that says every export requires a kept run. No auto-keep or persistence change.

Tables shows cheap per-image retained counts from `analysis_record_count()`,
`tissue_record_count()` and `cell_record_count()`, labelled kept runs rather than
eligible rows. Exact `record_count()` / `row_count()` comes from a native request
prepared for an export intent. Request construction may walk a project and
snapshot up to its native budget; do not call it per frame or create a new live
project-wide eligibility API. Preserve native require-current errors. Verify the
zero-run/header-only behavior with a small native case before wording its notice.

Go to tables uses existing `focus_task(Task::Tables)` and keeps the active image.
Do not add run-identity transfer or another retained-results state machine.
Checks cover primary/multi-selection, calibrated/uncalibrated and unsupported
measurements, kept versus live objects, stale-record refusal, navigation and
existing Save/reopen persistence. Reuse native measurement/export tests; add only
the missing changed-flow checks.

## 5. Make object browsing annotation-first

Add Annotations / All objects as one compact filter above the virtualized object
list. Rebuild its ordered IDs on `(object_visibility_revision, filter)` changes.
Read kind/class/appearance from the live object per visible row; do not retain
stale row labels or break virtualization.

Default a fresh docked workbench to Annotations. Persist an explicit workspace
marker; a loaded legacy layout without it and the compact API viewer retain All
objects. Tests set their intended preference explicitly and separately exercise
the fresh default. Never change production defaults based on whether replay
inputs happen to be present. Existing position-based replay fixtures with
detections must be updated deliberately.

Use aligned kind icons, readable names and a small appearance cue. If the cue
follows object override > class > default, describe it as appearance and disclose
class/override in its tooltip; do not falsely label an override as class color.
Replace changing-label row IDs with a stable private `###Object/<native-id>`
identity. A `##id` suffix alone is not stable when the visible prefix changes.
This is an explicit private widget-ID migration; public object/replay identities
remain unchanged. Verify rename, keyboard focus and selection after the migration.

Add proposed private native `cpp/selection_ranges.hpp` ordered-range helper over
`ObjectSelection::ids()` and `replace(ids, primary)`. Do not overload the bound
`select_range`, which selects the whole document ID interval. For a filtered
Shift click, select only IDs between anchor and target in the shown order;
Ctrl+Shift unions with existing selection. If the anchor is missing, use the
single-target fallback with the same additive rule. Target becomes primary;
normal click/ctrl-click keep their existing behavior.

Keep selection independent of filtering. Canvas cell selection does not switch
the list back to All automatically. Show N selected, M not shown by filter with
an explicit Show all action. Preserve existing global Ctrl+A semantics; do not
silently redefine it as filtered-only selection.

Checks: mixed annotations/detections, non-monotonic displayed order, missing anchor,
additive/nonadditive ranges, hidden selected objects, rename/class edits, image
switch and loaded/fresh preferences. Verify no filtered-out detections enter a
range unless already present in an additive selection. Document objects, native
selection authority, export scope and scientific state must remain correct.

## Implementation and validation sequence

1. Read the current task/checkpoint and inspect the active cache before building.
   Preserve concurrent analysis/SDK source and frozen binding objects. Record the
   changed inputs, unresolved risk and decisive check in the existing task card.
2. Settle milestone-1 native policy signatures, protections, controller, modal
   registry/actions, replay enum/bounds, summary adapter and tests before the
   combined cached build. Do not add future-milestone enums speculatively.
3. Run cheap source/dialog guards first. `core.hpp` enum additions cause broad
   native dependency rebuilds; name that cost. If native exporter changes are
   ready independently, run their affected targets first; otherwise combine the
   settled frontend/core inputs once. Never build ALL_BUILD by habit.
4. Build only explicitly affected native tests and GUI/app targets in the existing
   cache. Even if QUPATHXX_BUILD_PYTHON is ON, do not target bindings, stubs or
   packaging. Preserve passing binding objects and compatible prior evidence.
5. Run the focused native/replay cases below and inspect only changed surfaces.
   Do not replay full corpus/platform qualification, rebuild wheels or recapture
   unchanged menus/themes. Repeat only for a diagnosed failure or new change.
6. Update the current task card with decisive results, actual correction/waste,
   omissions and next action. Keep milestone scope and unresolved evidence clear.

| Changed area | Existing test location / native target |
| --- | --- |
| GeoJSON/object CSV policy | `cpp/tests/interchange_tests.cpp`; `qupathxx_interchange_tests` |
| Cell table policy/cancellation | `cpp/tests/cell_table_tests.cpp`; `qupathxx_cell_table_tests` |
| ROI/tissue tables | `cpp/tests/measurement_table_tests.cpp`; `qupathxx_measurement_table_tests` |
| Fingerprint JSON publication | `cpp/tests/file_fingerprint_tests.cpp`; `qupathxx_file_fingerprint_tests` |
| Sampling facts | Existing cell/tissue and grid tests; `qupathxx_cell_tests`, `qupathxx_tissue_tests` and affected grid targets |
| Dialog owner/actions/replay | `cpp/tests/gui_dialog_contract_checks.hpp`, `gui_dialog_workflow_checks.hpp`; native replay/organization targets |
| Shell schema/command mapping | `cpp/tests/gui_shell_tests.cpp`; native shell target |
| Ordered selection | `cpp/tests/selection_tests.cpp`; its existing native test target |
| Source enforcement | `tests/test_gui_dialog_contract.py` source-only checks and relevant design-system guards |

Milestone 1 is complete when routes A-F never replace without the registered
confirmation, G publishes create-only, protected outputs refuse under both
policies, and exact-byte/race/cancel/owner/replay cases pass. Native sampling facts
must match the request used by Run with zero region reads, correct per-mode units
and readable changed controls. Existing science/identity assertions stay intact.
Record frozen-binding consumer updates and absent observed-user validation as
remaining scope; passing source/replay checks do not prove pathologist usability.

Milestone 2 adds immediate presentation-only visibility with safe hidden input,
compatible shell reload and integrated measurements/retention captions. Milestone
3 adds filtered browsing without selection/export side effects. For each, walk
only its changed part of the selected journey: open image, choose ROI, understand
scale, run, inspect, retain, export safely and Save/reopen. A later observed
pathologist/researcher walkthrough is user validation, not evidence already
provided by this plan.

The later owner implementation request authorized these five phases. Native
exporter safety, sampling facts, selection, shell compatibility and actual GUI
workflows now have focused evidence in `build/ui-export-sampling`. Keep later
feature expansion, binding migration and observed-user validation separate.
