# UI workflow safety — milestone 1

Milestone 1 review fix 1 is implemented with sandbox validation concerns.
All13 review findings, five unverified items and controller additions A/B are
resolved; see the [review-fix report](../../../build/ui-workflow-safety/m1/review-fix1-report.md).
Passes A/B are retained; this receipt closes Pass C (1.9–1.13) on
`feature/ui-workflow-safety`, based on `7c981bf9`. All work is uncommitted.
At that Milestone 1 handoff, Milestone 2 and optional native follow-ups were not started.

[Plan](../../ui-workflow-safety-plan.md),
[Pass C report](../../../build/ui-workflow-safety/m1/pass-c-report.md),
[machine results](../../../build/ui-workflow-safety/m1/results.json).
Evidence paths below are relative to `build/ui-workflow-safety/m1/`.

| Finding | Milestone 1 result | Evidence |
| --- | --- | --- |
| 3: duplicate/rechecked/removal records | Rechecks preserve form inputs and cannot Keep; live raw/stream Keep once; registered removal confirmation covers added families | `pass-a-report.md` (including fix round 1), `pass-c-final-organization-headless.log`, `pass-c-final-workflow-safety.log` (all record groups pass before export boundary) |
| 4: shared inputs | Recheck keeps the analysis inputs; completed classifier previews survive selection changes. Broader shared-input changes remain milestone 5 | `pass-c-final-workflow-safety.log`, `pass-c-final-organization-headless.log` |
| 5: draft/preview loss | Stop cancels busy jobs; browser X only hides; polygon survives resize; registration keeps same-plane pairs/source/selection, invalidates fit/preview, recaptures the dataset and restores per-plane drafts | `pass-c-final-fluorescence.log`, `pass-c-final-registration.log`, `pass-c-final-workflow-safety.log` |
| 6: keyboard hazards | Shift merge/Boolean, open-only Batch chord, non-repeating Add, guarded text/modal input and corrected captions | `pass-a-report.md`, `pass-b-report.md`, `pass-c-final-workflow-safety.log`, `pass-c-final-replay.log`, `pass-c-final-header-grouping.log` |
| 7: reachability/dead ends | Raw channels accepts TIFF/Zarr via one predicate; ordered fluorescence/Batch reasons; tiled guidance replaces unavailable Add/Keep; Density retains checked cohorts after refused Run | `pass-c-final-zarr.log` (route passes before save denial), `pass-c-final-fluorescence.log`, `pass-c-final-organization-headless.log`, `pass-c-final-simplification-tools.log` (regression passes before publication denial) |
| 8: implicit scope | TMA affected counts/guarded selection; Tables current-image and cached project totals with bounded non-active revision refresh; empty Cells export focuses Tables field without writing | `pass-c-final-organization-headless.log`, `pass-c-final-workflow-safety.log`, `pass-c-visual-normal.png` |
| 9: feedback | Absolute dispatch destinations and row/object/import feedback; errors remain visible across tasks. Export-success assertions are retained for controller execution | `pass-c-final-workflow-safety.log`, `pass-c-final-simplification.log`, `pass-c-path-probe.log`; implementation in GUI/TMA/Spatial presenters |
| 10: misleading display | Shared class palette, original/corrected/derived domains, all-channel labels and outlined/scaled scale bar | `pass-b-report.md`, `pass-c-final-marker-rules.log`, `pass-c-final-dark-flat.log`, `pass-c-final-linear-unmixing.log`, `pass-c-final-image-inspector.log`, `pass-c-complex-boundary.png` |
| Copy/contract coverage | All new milestone captions have 15 translations; native catalogue checks conversions and stable identities; 22 source contracts pass | `pass-c-final-i18n.log`, `pass-c-final-design.log`, `pass-c-final-source-contracts.log` |

Pre-review milestone validation: the owner reports outside-sandbox controller
completion of all seven previously limited commands (including workflow-safety
with an absolute output directory) and22/22 uv source contracts. Those logs and
the original source identity are attached in `results.json` under
`prior_controller_verification`; the prior manifest is retained as
`review-fix1-prior-results.json`. The relative-output feedback expectation was a
test defect and is corrected in this review pass.

Review-fix validation: 15/19 affected native commands and22 source contracts
pass on the current source. Four invocations (absolute and relative workflow
receipts, Zarr save/reopen, spatial publication) retain diagnosed sandbox
canonicalization failures. All preceding review workflow assertions pass; the
report lists absolute controller output directories. Previous outside-sandbox
completions are not transferred to this changed source. No assertion, quota or
tolerance was weakened. The complex-project and Density implementations are
unchanged in this review pass and retain their explicitly source-scoped evidence.

Project count caching uses native instance/generation/project revisions; the
active image and one round-robin image are checked on each unchanged Project
frame. Non-active Keep/remove updates appear within one cycle. Current-image
scope does not invoke this cache. Native project authorities always construct
non-null Documents, including unavailable linked images; the cache additionally
handles null entries defensively.

Review regression evidence: `review-fix1-organization-headless.log` and
`review-fix1-replay.log` cover popup replacement/reopen, stale-owner notices,
isolated errors, blocked Batch dispatch, bounded count refresh and annotation
training counts. `review-fix1-fluorescence.log` covers actual bad typed-ID removal,
confirmed raw/stream removal followed by Keep again, fallback colours and
never-selected correction guidance. `review-fix1-dark-flat.log` proves mismatch
feedback remains distinct. `review-fix1-registration.log` proves coincident
edit/server swap, drag reset, draft preservation and native dataset recapture.
`review-fix1-i18n.log` requires all16 new keys and15 translations each.
`review-fix1-workflow-safety.log` and its relative counterpart prove the Region
notice, busy-removal guards, raw Keep-again and second-F8 focus before the export
boundary. Source contracts: `review-fix1-final-source-contracts.log`.

Frozen products: all14 native libraries and38 binding objects are byte-identical
before/after. Core SHA-256:
`1f68d6329cee33a47796a6482e94d93e90b0c44028bb81f36c7f907f0b7a0e0c`.
`review-fix1-before.json`, `review-fix1-after.json` and `results.json` record hashes and the
new frontend source/binary identity. No native core/binding build or package step.

Captures prove readable feedback, focus and geometry/overlay registration;
`pass-c-visual-fr.png` shows readable translated training counts at French 200%
and its executable check reports zero overflow. These are usability checks,
not cosmetic matching or biological/platform/release acceptance.


## Milestone 2 — Files and overwrite policy

Task 2.4 fix round 1 passes **40 source contracts and all 95 native commands** through per-command Q: mapping inside the sandbox (the original 94 plus partial-prediction, including relative --tool-paths). An additional unmapped read-only reader probe passes. No sandbox case remains blocked. [Fix report](../../../build/ui-workflow-safety/m2/t24-fix1-report.md), [results](../../../build/ui-workflow-safety/m2/results.json) and t24-fix1-before/after.json own the current source and unchanged 14 libraries/38 binding objects. Earlier sbx2/t24-ctl results are pre-fix baseline evidence only.

| Finding | Delivered behaviour | Current evidence |
| --- | --- | --- |
| 1: typed paths / launch-directory surprises / one-image Add | Core plus 29 tool fields have Browse, exact IDs/replay targets, independent read/write paths, project-relative interactive/process-relative option semantics, stack-aware drops and multi-file Add; standalone unhit drops explain the missing project | Tool-paths, relative tool-paths, separate normal/French200 views, Batch/core/family Browse cases; source contracts |
| 2: competing overwrite rules | One gate, six scripted flags, no replace checkbox, preserved protection/create-only rules and complete-only prediction receipts | Writer/scripted/interactive/guard/decision/partial-prediction modes and full complex walk; mapped passes |

Milestone exit, item by item:

- [x] No file operation requires typing a path in the selected workflows; existing import-window pickers remain. Browse-only operations pass.
- [x] Outputs show resolved destinations and success receipts or native directory reports. Partial predictions are not announced complete; receipts raise without taking focus or stopping typing.
- [x] Exactly one overwrite dialog exists; no checkbox arms replacement. Native policies/identity guards and six scripted flags are checked.
- [x] Publication and the copied complex Draw/Run/Keep/Export/Save/reopen/Recheck, then registered Remove/Confirm/Save/reopen pass mapped, with older identities intact.

Decisions taken (the plan's stated defaults): interactive relative names use project, last folder, then process directory. Scripted options stay verbatim and process-relative until edited, dropped or browsed. The in-app picker was extended. Six scripted flags pre-authorize replacement; no checkbox does.

Native contracts remain: Spatial CSV and the two TMA summaries are create-only and existing files are refused before work. Save As to a different existing project identity is refused natively without a dialog. Stain-model save remains composed from native parts because its guarded native save has no write policy. These remain optional native follow-ups, not selected work.

Stored-reader limitation: the frozen TIFF reader normalizes forward-slash inputs before keeping a recipe (tiff_server.cpp:400). The window now opens stored strings verbatim and restores the exact stored text/identifier. Mapped and unmapped probes pass. A literally non-normal stored recipe cannot be constructed by that native sequence here; legacy non-normal recipe support is not claimed and native SHA equality is not relaxed.

Sandbox method note: Each case executes a batch file with cmd /d /c. That same command removes the previous Q: mapping, maps Q: to this writable worktree, changes cwd through Q:, sets QUPATHXX_TEST_GUI explicitly, and spells the executable/output through Q:. Runs are sequential and recreate the mapping every time. The sandbox cannot open C:\Users\HEQ for attributes; weakly_canonical walks that prefix for a not-yet-existing file. Q: avoids that spelling, without changing assertions, native identity guards or product I/O. Mapped passes count as passes. t24-fix1-run.py and its t24-fix1-run-*.cmd files retain the reusable method.

Separate replay note: Eye and metadata/Undo already failed on main before Task 2.2 because old coordinates hit empty space. They were retargeted to actual shell/widget bounds and the stable Edit image metadata ID. Every original visibility/state/Undo/saved-byte assertion remains, with a modal-input assertion added. Product guards were unchanged; t22-fix1-report.md retains baseline diagnosis.

Pixel and saved-stream browser captures are separate at normal/French200; all changed windows have zero overflow. Fifty-two captures are retained losslessly as PNGs in t24-fix1-captures.json. They prove usable controls/readable values, not cosmetic parity or biological/platform acceptance. No current matrix case is pending for the controller; t24-fix1-real-paths.ps1 supplies its planned real-path repeat. Stop at this handoff.


### Task 3.1 carry-over addendum

A. Standalone unhit image drops now report through the general `error` string. The actual GUI check refuses a drop, performs successful Undo, then requires the refusal to disappear and the normal `64 x 48 px` image status to return (`--core-paths all` and `drop`).

B. `PathField::hit` remembers the root window ID and compares it with the current front root ID. It never retains a window pointer. A field survives destruction of its ImGui context in the check; no-context/new-context hit tests and the existing topmost/covered-field stacking checks pass.

C. The `--tool-paths views` harness now calls the product `configure_gui_fonts`. Normal/French 200% captures retain zero overflow. The French fluorescence capture visibly renders U+2019 in “L’analyse” (see `t31-fluorescence-fr200.png`); this is readability evidence, not pixel-parity qualification.

Final evidence: [Task 3.1 report](../../../build/ui-workflow-safety/m3/t31-report.md), `t31-caption-source-contracts.log` (42 passed), and `t31-native-results.json` (the full 95-command Milestone 2 matrix plus absolute/relative state checks: 97 passed on the final frontend). All 14 native archives and 38 binding objects remain exact. The report records the initial incomplete metadata inventory, its diagnosed scanner correction, and the resulting extra cached frontend build/matrix cost. No scientific assertion or persisted identity changed.

## Milestone 3 — Kept-result lifecycle

Milestone 3 is implemented on `feature/ui-workflow-safety`, based on `7c981bf9`.
All changes remain uncommitted. The final source contracts and native matrix,
including the copied complex-project walk, pass through the per-command Q: run
mapping. Compilation, objects and linking use real C: paths. Evidence is local
under `build/ui-workflow-safety/m3/`: [machine results](../../../build/ui-workflow-safety/m3/results.json),
[Task 3.4 report](../../../build/ui-workflow-safety/m3/t34-report.md), and the
source-scoped `t31-report.md`, `t32-report.md`, `t33-report.md`,
`t33-fix1-report.md` and `t33-fix2-report.md`.

| Finding | Delivered behavior | Decisive evidence |
| --- | --- | --- |
| 3: duplicate Keep and removal | The common presenter distinguishes fresh previews, kept previews and rechecks; selected native record actions preserve exact identity. Irreversible removal confirms; four undoable recipe families remove immediately and Undo restores their identities. | Final `--workflow-safety`, `--record-lists` absolute/relative/long, four fluorescence-family suites, and `--simplification-complex`; `t34-native-results.json`. Tasks 1/3.2 receipts retain the original duplicate-Keep characterisation. |
| 9: understandable state and feedback | Family-specific captions replace raw state lines, unknown tokens are explicitly unknown, missing-parent errors stay visible, and session-only classifier models show the nonblocking warning before replacement. | Final `--result-state`, `--record-lists`, `--model-safety` absolute/relative/long and source contracts; `t34-captures.json`. |
| 11: vocabulary | In-memory records read Kept, object creation reads Add, disk writes retain Saved; all new captions have 15 translations. | Final i18n/design/source suites, immutable presenter comparison, seeded `--tool-paths views`, and `t33-fix1-report.md` caption inventory. |
| 14: repeated lifecycle code | Region/Tissue/Cells use one presenter; selected kept families use the cached clipped list with family-owned actions and stable replay marks. | Final 24-fixture `--analysis-tasks presenter`, `--record-lists` and all per-family consumers; `t33-fix2-report.md` owns the final list interface. |
| 15: string status protocol | One presentation header parses native tokens by their actual family; unknown values cannot become a false verification failure. Native wire tokens remain unchanged. | Final headless `--result-state`, lexical source guard and retained token inventory in `gui_result_state_checks.hpp`; final source suite. |

Milestone exit, item by item:

- **One status vocabulary on screen:** the final state inventory, source guard,
  presenter traces and seeded normal/French 200% views pass. Mixed load/export
  operations select the correct caption family; density row and summary states
  use RunState. Arbitrary native error prose and scientific phase details remain
  literal where the contract calls for them.
- **Kept families can be listed, rechecked and removed consistently:** selected
  Pixel, Object, Raw pixel files, Raw measurements, Spatial, fluorescence,
  dark/flat, linear unmixing, marker rules and current-image Tables are covered
  by actual native actions and final family suites. Recheck is offered only for
  families with a native recheck/status authority; model/recipe archives without
  one do not acquire a synthetic status or recheck. Pixel stream links retain
  their existing combo selector and native verify/read/remove actions; the
  milestone does not claim every selector in the application became the shared
  list. Undoable removals act immediately; irreversible ones confirm.
- **No raw token or 64-hex identifier is the primary label:** selected list rows
  have a name/family and short native ID; complete IDs stay in Details/Copy.
  Native `--record-lists` checks and seeded views pin this, including unlisted
  parent IDs retained for native validation. Unknown-status diagnostic suffixes
  may still show the raw token; scientific digests remain available in details.
- **Inspecting a kept record never changes the working model:** final
  `--model-safety` absolute, relative and 90-character-folder runs train twice
  with changed features/classes, preview the old archive, and retain the exact
  working object, SHA, feature/training options and live prediction. Historical
  masks use the historical classes/palette. Review fix round 1 additionally
  separates the kept mask from live actions and numbers: **Close kept mask**
  returns to the same live prediction without recomputing. Trace, Add and Keep
  result are disabled while the kept mask is shown. The legend uses a colour
  swatch and normal text, bounded by both classes and palette. Raw file and record previews already
  had separate historical model ownership; the checks establish that behavior
  without changing the native frontend. Use kept model explicitly adopts.
  Review round 2 restores the shared Opacity slider beside a kept mask, even
  without a working model; Close preserves that opacity for the live palette.

Defaults taken (these are the controller's readings where the plan left a
choice; the owner has **not** confirmed them individually):

- The batch option reads **Add cell objects (in checkpoint projects)**, using
  the plan's Add vocabulary for creating objects, instead of Publish.
- Batch checkpoints open as projects and export their tables; no **Export
  combined CSV** writer was added. The native checkpoint/table checks pass.
- In Region/Tissue/Cells the running step sentence stays; Cells phases have
  captions; sample and marker counts remain visible; only run time moved under
  Details.
- Accepted panel traces are versioned fixtures under
  `tests/fixtures/gui-analysis-tasks`; final comparisons do not re-record them.
- A list row shows a state only where native code has a status for that record;
  Recheck exists for those families only.
- Removal confirms only when it cannot be undone. Fluorescence, Dark/flat
  correction, Linear unmixing and Marker rules remove immediately; Undo restores.
- Project-density row and summary states use `RunState` captions, as recorded in
  the Task 3.3 fix round. The native `reused` token displays **Reused**, distinct
  from freshly **Saved**; literal completion/computed/reused/failed counts stay.
  The independent retained density consumer passes; no native density rebuild.
- The unsaved-model warning is a visible line beside replacement actions,
  without a dialog or blocking the action. It follows model facts: either a
  training or prediction record holding the exact model counts as kept; removing
  its final holder brings the warning back unless a model Save succeeded.
- The kept-mask view is exclusive: live actions, numbers and contours stay
  hidden or disabled until **Close kept mask** restores the preserved live
  result without recomputing. This is the controller's reading of the brief's
  unspecified presentation, not an individually confirmed owner decision.

- A Raw record selection that matched and subsequently disappeared falls back
  to latest/none. An ID that never matched stays as given and its actions report
  native errors; it never selects or confirms removal of another file (J).
- A model holder in another still-open document counts as kept through a weak
  identity token; returning to that document rechecks its record natively.
  Expiry searches only the current document. Successful Save remains a session
  fact (K). J and K are controller readings, not individual owner decisions.
  After the holder project closes the warning returns even if that project was
  saved, because the window cannot see closed projects.

Task 3.4 validation (historical): **50 source contracts; 105 native commands**, including the
100-command Task 3.3 fix-round matrix, three new model-safety output modes and
an additional long-folder record check and the retained density-panel baseline consumer. All 24 presenter fixtures remain exact;
normal/French 200% model captures and existing seeded tool captures have zero
measured overflow. The Task 3.4 captures established warning placement and historical palette
identity, but did not establish an exclusive view or a readable dark-class
legend. Review fix round 1 replaces that claim with explicit kept/live captures
and swatches beside normal-colour class names. These are behavior/readability checks, not cosmetic
parity or biological acceptance. Native-reported extended-prefix checkpoint
paths pass strict filesystem-equivalence checks in the long record run.

All **14 native libraries and 38 binding objects**, `core.hpp`, HEAD, branch,
index and accepted traces remain exact. Every runtime executable/library is
scanned for case-insensitive `Q:\cpp` in ASCII and UTF-16 with no match. Final
commands, cwd/mapping, exit codes and executable hashes are in `results.json`.
One combined 25-unit cached frontend build was followed by three organization
unit/link repairs for missing failure diagnostics, a gradient-halo fixture and
relative fixture paths. Source review then caught an introduced supplied-model
startup-controls regression: its original fallback was restored, five affected
units/archive/links rebuilt, and the final matrix repeated. Earlier candidate
results stay in `t34-initial-native-results.json`; all attempts remain in
`t34-attempts.json`. This extra repair/build/matrix cost is the Task 3.4 report's
concern; no required check remains
for the controller.

The milestone does not change scientific kernels, prediction math, model or
record identity, stored training data, persistence/export formats, native wire
states, replay enums/targets, binding ABI, SDK, wheels or published candidates.
No commit, push, branch change, configure, native/binding build, later milestone
or optional native follow-up was performed. Stop at this handoff.

### Milestone 3 review fix round 1

The final review candidate passes **59 source contracts and 112 native commands**.
[Review report](../../../build/ui-workflow-safety/m3/review-fix1-report.md) and
[machine results](../../../build/ui-workflow-safety/m3/results.json) own this
candidate; Task 3.4's original counts/captures above remain historical evidence.
Nine A–I guards fail on the review baseline and pass after correction. Actual
native checks establish exclusive kept/live actions and numbers, Close without
recomputing, normal-text dark-class legends, prediction/training record retention
and removal, sticky successful Saves, Raw fallback/Open, unchanged Live source
and Cells Export state, and exact all-kept CSV bytes. Restored Data/tiled/Files
states are additional to the record states; normalization captions retain IDs.

The exit item about inspecting a kept record now includes exclusive presentation
and exact live restoration, not just preserving the working model behind the
mask. Captures show historical colour swatches with readable normal-colour names;
the original coloured-name capture did not prove that. Warning facts are checked
for prediction as well as training archives and return after the last holder is
removed unless a Save succeeded. The defaults list above records A's exclusive
view ruling as the controller's reading, not an owner-confirmed decision.

All24 presenter fixtures compare without recording. Captures cover each touched
window and restored state normal/French200 with overflow0. Original retained
cases run once, then four extra output variants complete relative/90-character
sweep/normalization coverage. The normalization consumer has no output argument;
its relative variant resolves a relative cwd. Its ordinary long cwd hit the
frozen atomic writer's temporary-path limit; the same-directory extended-length
cwd passes every original assertion and is verified as the same directory.
This does not claim ordinary unprefixed long-path support for that frozen writer.
The controller's round 2 normalization run with a 104-character folder hits
another frozen native limit: `cpp/expression_matrix_writer.cpp` reports
**Cannot create expression staging directory** at a 256-character directory
path. That source is unchanged since main; this invocation is not counted as
passing. Neither native limit was rebuilt or relaxed.


One combined 25-unit frontend build passes; two later organization-test unit/link
repairs fix only driver IDs/scroll state. No product-code repair or second
combined build. That extra cost and the explicit long-path invocation limit are
the report's concerns. All14 frozen libraries/38 binding objects and core.hpp
remain exact; runtime executable/library scans find no Q:\cpp embedding. No
required check remains for the controller. Milestones 4–6 follow the selected
plan; only N awaits an owner decision. No commit, push, native/binding build,
publication or broader scientific/platform/performance acceptance is claimed.

### Milestone 3 review fix round 2

J–M restore the never-matched Raw ID error boundary, retain kept-model facts
across open documents using weak identity, and restore historical Opacity with
and without a working model. The document status guard is removed: handoff prose
receives link/diff checks. The defaults and inspection exit item above include
these rulings. The catalogue and all captions/printf conversions are unchanged.

The two native long-path limits are recorded together above. The accepted
90-character normalization variant retains its equivalent extended-length cwd
and every original assertion; the controller's failing 104-character staging
variant is not counted as passing.

The final candidate passes **61 source contracts and 117 mapped native commands**
(the prior complete matrix plus J–L absolute/relative/90-character variants
and relative/90-character fluorescence variants).
[Round 2 report](../../../build/ui-workflow-safety/m3/review-fix2-report.md) and
[machine results](../../../build/ui-workflow-safety/m3/results.json) own this
candidate. Three source guards fail on the round 1 source; native J–L flows,
review/model-safety/record-lists/fluorescence, the complex walk and unchanged
presenter comparison all pass. Captures cover touched Pixel/Raw windows normal
and French200, including historical opacity with/without a working model, with
zero measured overflow. Every path comparison retains native file equivalence.

One five-unit combined cached frontend build (no catalogue rebuild or repair)
precedes focused checks and one final matrix. All14 native libraries/38 binding
objects, catalogue, core and24 fixture files stay exact. All21 runtime
executables/libraries are clean of case-insensitive ASCII/UTF-16 Q:\cpp. Final
source/Git/link/diff and exact CSV checks pass. No required check remains for
the controller, no native/binding/configure/Git mutation or later milestone was
performed. Milestones4–6 follow; only N awaits an owner decision.

## Milestone 4 — Command table

Milestone 4 gives menus, tool titles, button captions, tips, key dispatch and
Help one command table. Task 4.3 adds a shared availability answer for each
covered action, including a translated disabled reason and a matching inline
reason on primary actions. The presenter still owns which task rows exist.
Help qualifies short captions with their task/table and scope, lists all 67
owning chords once (both Redo chords), and rebuilds only when language changes.

| Finding | Change and evidence |
|---|---|
| 6 — Chords and their captions disagree | Table-backed readers and one dispatcher; [Task 4.1](../../../build/ui-workflow-safety/m4/t41-report.md), [Task 4.2](../../../build/ui-workflow-safety/m4/t42-report.md), [control-key repair](../../../build/ui-workflow-safety/m4/t42-fix1-report.md), [workspace/popup repair](../../../build/ui-workflow-safety/m4/t42-fix2-report.md); final scope/wiring/key-button checks in [results](../../../build/ui-workflow-safety/m4/results.json). |
| 9 — Disabled controls do not explain why | Eight tools, Region/Tissue/Cells, Tables, Batch, Files and Use profile share availability answers; disabled trace/tip and primary inline reason checks, bilingual reason rendering and native state-driven chains in [Task 4.3 report](../../../build/ui-workflow-safety/m4/t43-report.md) and the [milestone review fix](../../../build/ui-workflow-safety/m4/review-fix1-report.md). |
| 12 — Controls disappear | Formerly hidden operation controls remain visible and disabled; no-image/empty-selection/job scenes and harmless clicks; progress Stop/Cancel and kept-mode Close remain conditional. The report lists disclosure/data/recovery exceptions. |
| 13 — Help is incomplete | Generated owning-chord list; English/French caption uniqueness (except Redo), all 67 chords and no chordless rows, registered scrolling dialog, top/middle/bottom captures normal/French 200% with zero overflow. |

The exit line is assessed item by item:

* **A chord, its caption and its enablement cannot disagree.** Source membership
  and ownership contracts, rendered table readers, dispatcher scope/wiring walks
  and 122 task plus 8 Tables physical-key/button pairs per output variant cover
  the changed routes. Keys enter through the private physical seam. The seven
  object-command workspace checks include the image/panels and exclude tools.
  Legacy I/O wiring cases retain the observation limits of the Task 4.2 reports;
  this is not an exhaustive product of every parameter/native error state.
* **Every disabled action says why.** Checks cover submitted WorkflowActions in
  all 24 task situations and eight tools with no image, empty selection and
  actual native work, plus Tables/Batch, Files and Use profile. All 39 reason keys render in two
  languages; primary inline and tooltip reasons agree. Training, path entry,
  tracing and closing a kept mask remove the corresponding reason. This claim
  concerns the migrated workflow actions, not every disabled parameter input.
  The Annotations editor's three actions and the TMA window receive their reasons
  in Milestone 5 Tasks 5.3/5.4; this exit line does not claim their coverage.
* **Help is complete by construction.** Only owning rows with chords generate
  lines. Both Redo chords and key-only commands are present, aliases and retired
  cancellation chords are absent. Rendered English/French uniqueness and
  complete-list checks supplement the source contract.

The defaults below come from the plan and controller rulings. They were
**not separately confirmed** one by one by the owner; none is described as approved.

* Eight cancellation chords are retired: Ctrl+Shift+M (Region), Ctrl+Shift+D
  (Tissue), Ctrl+Alt+G (Cells), Ctrl+F3 (estimate), Ctrl+Alt+Shift+X (cell table),
  Ctrl+Alt+Shift+Q (measurement export), Ctrl+Alt+Shift+F (file checksum) and
  Ctrl+F10 (recovery maintenance). Stop stays beside progress; existing scripted
  inputs for those commands still work.
* F7 imports at once and reports the file and Undo. Keep/Recheck chords stay:
  Region Ctrl+K/Ctrl+Shift+K, Tissue Ctrl+U/Ctrl+Shift+U, Cells
  Ctrl+Shift+C/Ctrl+Alt+C. The Cells difference (Ctrl+C is Copy) remains an open
  owner question rather than a new remapping.
* The eight Global rows that already worked while typing are Open project,
  Save project, Project/Image/Annotations/Workflow panels, Choose language and
  Quick reference (Ctrl+O, Ctrl+S, Ctrl+1–4, Ctrl+Shift+L and F1).
* Select all, Delete, Escape, Enter, insert vertex, Boolean and Merge act anywhere
  in the main image workspace and its panels, outside a tool window. Tool keys
  R/E/P/L/T/V/H and O act anywhere except while typing. Exact modifiers are
  required. Holding a key acts once, including tool keys and vertex deletion.
  No shortcut acts while a dialog, menu or list is open, including its dismissal
  frame. Only actually submitted popups own keys.
* Scripted keys request their command and pass the same gate as pressed keys
  without requiring the pointer on the image. Escape/Enter still arrive as keys;
  scripted editing keys no longer reach an active text field. Public replay
  kinds/targets and retired-chord inputs retain their identities.
* A Cancel batch request is never refused, including dialog, popup and text
  ownership. Run stays blocked; other cancel inputs retain their existing gate.
  This is the explicit exception to the scripted-key gate above.
* A refused key shows one sentence at its action, or on the task's message line
  where the action has no inline reason. Export refusals show their task and set
  the existing report field to the same translated sentence without submitting
  work. A key blocked by a dialog stays silent.
  A refusal under the Cells rule shows on the Cells message line while it stands
  and ends when the action becomes available, an export starts or an export
  fails. Its report keeps the original sentence until export start or failure;
  a dialog temporarily hides the line without changing that history.
* Both Files keys bring their task forward and open the compact header even
  when refused. A refused Review says why on the Files message line and in its
  report; a refused Hash keeps its inline reason without another message.
* Help lines carry the task or table and scope they act on. Mouse gestures
  (right drag, scroll, Ctrl+click, Shift+drag and double-click) remain handwritten.
  Close kept mask stays conditional on kept-mask mode. In the docked workspace,
  with Cells in front, the Cells export key follows the Cells button, including
  kept-run source/currentness and busy gates. In the compact layout the
  image-cells key takes the Tables answer, regardless of the stored task.
  The image-cells key brings Tables forward unless Cells is already in front
  in the docked workspace. Before the milestone it brought Cells forward
  whenever the cell source was kept runs.
* Beyond five catalogue titles and Exit, Task 4.1 makes tool-window titles follow
  the language, shows chords in menus/tips where absent, and removes R from the
  Select tool tip.
* The Annotations editor's actions and the TMA window get their reasons in
  Milestone 5, alongside their planned editor/window changes.

Recorded deviations: Task 4.1 inventories actual membership rather than the
plan's estimated 35 sites, uses a private desktop presentation scope, existing
shell-header adapters, translated menu keys for tool titles and formatted native
TMA captions. Task 4.2 adds direct chord owners, an alternate Redo/key-only rows,
a private per-frame context/request cache, a navigation-claim bit and an event-ID
queue for Escape/Enter. The controller's workspace ruling replaces view-only
ownership and retains submitted-popup ownership through dismissal. Its reports
record fixture repairs, finite I/O-cancellation observation limits and extra
build cost. Task 4.3 uses a presenter availability callback and a plain-button
adapter; keeps the five mouse gestures and legacy correction explanation;
retains conditional metadata Retry and disclosure/data-dependent editors;
and records the stricter table/export-key cases. Its first session missed the
one-build budget. The final correction round rebuilds the frontend together,
including the empty-line whitespace edit; any later test-only build is listed
separately. Exact costs, failures, assertion changes and coverage limits are in
the reports and machine records.

The independent milestone review fixes the Batch replay exception and restores
its meaningful panel-handoff check (proved failing under the old gate); keeps
compact Tables export independent of the Cells task rule; restores refused-key
feedback and exact feedback-path text; corrects three reasons; adds Files/profile
availability; and removes dead F5/hint/catalogue code. Workflow row lookup is
indexed once and task answers use stack storage. The 24 accepted task traces
remain unchanged without re-recording. The [first review fix report](../../../build/ui-workflow-safety/m4/review-fix1-report.md)
records that pass's checks, negative proof, exact assertion changes and test-only
fixture repairs/build cost. Its validation passed 120 native cases,
32 dispatcher/presenter variants, six review/reasons/Help forms and 74 source
contracts on final binaries. Build churn exceeded the one-build target: 39 compile
attempts, three GUI archives and 57 links, including one failed test compile and
two targeted GUI corrections. These corrections add no scientific workflow.

The [second review fix report](../../../build/ui-workflow-safety/m4/review-fix2-report.md)
records the final C2/B2/E2/A2/H2/T1/T2 corrections: one availability-owned translated
refusal sentence, compact image-cells keys using the Tables answer, Files navigation
and feedback on refusal, dispatcher-owned Batch stop, uncached full-cache fallback,
and state-driven navigation Keep. All 182 final commands pass: 120 native cases,
32 dispatcher/presenter variants, nine focused output-folder forms, 20 navigation
repetitions and the 74 source contracts. Normal and French 200% task traces match
all 24 accepted fixtures without re-recording. Public replay Cancel stops a running
batch under a registered dialog; blocked Run starts nothing. The unchanged panel
assertion again rejects the retained old-gate binary. Build cost is 26 compiles,
one archive and 17 links, all successful: one combined frontend build plus one
organization-only rebuild to correct the new compact-key test's context expectation.
The controller found refusal-lifetime and retained hash-input issues (L1/L2)
after round 2, so that round did not end the review.
The [round 3 report](../../../build/ui-workflow-safety/m4/review-fix3-report.md)
records the corrected lifetime and scoped cleanup of unchanged 256-MiB hash
inputs, including failure unwind. Seven focused mapped runs and 74 source
contracts pass; the review passes in all three folder forms, all 24 task traces
stay exact, and all four post-run scans find no file >=200 MiB. Actual cost:
5 compiles, 2 archives and 34 links, all successful; two new-fixture repairs and
a live-dialog correction exceeded the one-build target. Launcher failures are
also retained. No whole-matrix mapped run belongs to this round; the controller
reruns that matrix on the real path.

Final mapped commands, working/output directories, exit codes and executable
SHA-256 values are in [results.json](../../../build/ui-workflow-safety/m4/results.json).
The report links the source/fixture diffs, captures and preservation proof.
Scientific algorithms/options, measurements, record identities, stored bytes,
export formats and native validators are unchanged. The 14 native libraries,
38 binding objects, public core header and frozen wheel/SDK are unchanged;
runtime binaries have no mapped-source Q:\cpp. No native/binding configure/build,
commit, push or branch change belongs to this milestone. Native long-path
limits and broader biological/platform/performance acceptance are unchanged.
Milestones 5–6 follow; only N awaits an owner decision.

## Milestone 5 — Inputs, drafts and tool scope

Milestone 5 delivers the selected frontend workflow. Exact final commands,
exit codes, mapped working/output directories and executable SHA-256 values are
in [results.json](../../../build/ui-workflow-safety/m5/results.json).
The [Task 5.4 report](../../../build/ui-workflow-safety/m5/t54-report.md)
owns its declarations, survival/native-action tables, assertion changes,
source deviations, captures, build cost and preservation evidence.

| Finding | Delivered behavior | Evidence |
| --- | --- | --- |
| 4 — hidden shared state | Declared input consumers; accurate Batch summary; Fluorescence Recheck uses private requests and leaves the form and choices alone | `--analysis-inputs all`, `--batch-recipe`, `--tool-scope`; [5.1](../../../build/ui-workflow-safety/m5/t51-report.md), [5.1 fix](../../../build/ui-workflow-safety/m5/t51-fix1-report.md), [5.2](../../../build/ui-workflow-safety/m5/t52-report.md), [5.2 fix](../../../build/ui-workflow-safety/m5/t52-fix1-report.md) |
| 5 — implicit draft/preview resets | Owning-object drafts, independent Applies, TMA ready/loading preview lifetime, plane-specific landmarks/glyphs/polygon points | `--drafts` (includes `--draft-fix1 all`), `--tool-scope`, `--simplification-tools`; [5.3](../../../build/ui-workflow-safety/m5/t53-report.md), [5.3 fix](../../../build/ui-workflow-safety/m5/t53-fix1-report.md) |
| 8 — misleading or hidden Batch inputs | One effective native recipe supplies the summary and Run; shared values and pinned model identity are visible | `--batch-recipe`, `--tool-paths batch`; 5.2 reports above |
| 18 — pointer/global-revision tool scope | Six tools distinguish document/plane/display changes from image/source changes, retaining native currentness checks | `--tool-scope` in absolute, relative and long output forms; native suites for all six tools; 5.4 report above |

The milestone exit is taken item by item:

- Editing inputs never cancels unrelated work silently: the independent
  consumer masks, all six fields, running/finished cases and displayed/report
  lifetimes are checked by `--analysis-inputs all`. This covers the five declared
  consumers, not every possible future task.
- A batch runs what its window shows: `--batch-recipe` compares the displayed
  effective settings with the native checkpoint identified by the run report,
  including exact parameter hashes and immutable running settings.
- No draft or tool state is dropped by a display change: `--drafts` and
  `--tool-scope` cover annotation/TMA drafts, the six tools, polygon points,
  view replacement and plane hide/return. Drawn vertices/positions establish
  meaningful overlay registration. Native-invalid state still follows native
  refusal; the GUI scope grants no scientific reuse authority. This does not
  claim every optional tool or a new native identity contract.

Defaults and interpretations taken below are the plan's stated defaults and the
controller's readings of its text. **The owner has not confirmed them one by
one.** Closing TMA to keep a preview is specifically unconfirmed.
The plan states “Name and Class commit on Enter or focus loss as one undo step
each”; the implemented Class control commits on choosing a class.

The input consumer table, including stain estimation, is:

| Consumer | Input fields that can cancel it |
| --- | --- |
| Region | Kind, Vectors, White, Nucleus, Intensity statistics, ROI sampling |
| Tissue | Vectors, White, Nucleus |
| Cells | Vectors, White |
| Pixel-classifier training | Vectors, White; only while running |
| Stain estimation | White, Nucleus |

Task 5.1 and its fix take these defaults/deviations: an input edit stops a
running training and leaves a finished training, its table and Keep action alone;
Use estimate also stops running training by the table. An identical loaded stain
model/profile is no edit and cancels nothing. Until notifications exist,
`Cancelled: ...` stays in the consumer's own panel/window until it runs again,
and an editor list omits consumers belonging to that editor's own section.
Already-stopping consumers receive no sentence. Feedback ends at active-image
ownership changes. Existing session error fields carry the sentence in the UI
language while it stands. Tissue/Cells edit only their consumed fields in place;
Region's Advanced inputs always shows both stain vectors. The complete normalized
stain basis and cached activation reference count as Vectors; the requested
editor White remains the actual request reference. The extra stain-estimation
consumer and native generation observations are private frontend mechanisms.
The 5.1 fix's changed finished-training expectations and source guard are listed
in its report; no scientific identity or tolerance was relaxed.

Task 5.2 and its fix use a linked summary in place of greyed controls. A ticked
ROI batch takes Intensity statistics from shared inputs; a scripted session
supplying batch options starts unticked. Tissue-only settings are cleared for
other kinds. The summary shows the shared white reference/stain vectors, names
its active-image “Sampling example”, and states each batch kind's outputs and
running-settings behavior. The pinned pixel model is shown by classes and a
short identifier with Unpin; the raw model has no Unpin because native validation
requires it. Newly catalogued pre-existing captions are translated wherever
they appear. The native report exposes a parameter hash/checkpoint, so validation
reads that checkpoint rather than adding a threshold field. Canonical sampling
uses a valid nonzero pyramid level; the model has classes/hash rather than a
name. The fix caches even an absent sampling target by document/selection
revision and gives all Pixels pin states one output sentence. Fixture/navigation,
encoding, localization and bounded build corrections are documented in both
reports; no existing numeric assertion changed in 5.2.

The changed sentences under the Batch kinds are retained as follows:

| Kind | Sentence |
| --- | --- |
| ROI, Tissue, Cells | “Each run keeps its starting settings. Results go to independent checkpoints; the open project is unchanged.” |
| Pixels, with or without a pin/active model | “Each target produces a verified QPS file; only complete image bundles are reused. The open project is unchanged.” |
| Raw channel statistics | “Measures original TIFF samples at the selected axes. Each ROI retains its input hash and original labels in an independent checkpoint.” |
| Raw prediction files / streamed contours | “Uses the pinned raw model and exact sampling axes. Complete per-image projects are sealed before checkpoint reuse.” |

Task 5.3 takes Name and Class committing as one undo step each: Name on Enter
or focus loss, Class on choice. Name commits to the object it was typed for even
if selection changed first; Escape restores that object's current name. Geometry
with parent, and colour with reported probability, are two drafts with separate
Apply actions. Untouched components follow document edits; touched components
stay; Apply writes onto the current shape. A dirty draft dropped by another
selection, removal, image or workspace change leaves “Unapplied geometry changes
were discarded.” or “Unapplied changes were discarded.” in its pane until the
next draft edit or Apply. Image/workspace changes set that sentence from that
event alone. A typed value is discarded with the sentence, never written into
the next object after a selection-changing press. Only that editor's own draft
fields lose focus/pending final writes. A clean Apply says “There is nothing to
apply.”; Clear properties and unlock without properties says “This object has
no properties.” Numeric drafts update while typing, and untouched focused fields
follow edits made elsewhere. Parent ID retains its caption and opens a cached
list of other area annotations plus No parent. In TMA, case ID, core geometry
and labels are drafts; values for a vacant slot remain when a core appears there;
clicking the same row does not reload fields; an empty grid name is no edit.
Labels remain on Grid edits. Bounds/parent and component-wise rebasing preserve
new shapes; literal native names/IDs and live scalar buffers need private UI
helpers. The three changed assertions in the old dirty-drafts pair are listed
verbatim in the 5.3 report. The fix narrows draft text retirement, avoids copying
object maps/core values, opens the parent cache only when needed and corrects
transition notices/property reasons; its existing expectations stay unchanged.

Task 5.4 keeps ready and loading TMA previews when the window closes/hides;
Discard preview removes one explicitly, while image/source changes retire it
with the existing sentence. A new Run/Load explicitly replaces its preview;
editing next-run inputs does not discard the ready preview. Recheck uses its
record's native request and a separate private unmixing source controller.
Load settings from this result alone copies the record's form settings, leaving
the chosen correction/unmixing alone. The scope also holds the document token
weakly because image IDs are project-local. Native TIFF/OME-Zarr views actually
mint new reader generations in this retained runtime: the scope stores the real
generation and uses native `same_reader_files` only when it changes. It retains
that ReaderSource observation as an additional plan deviation. Existing TMA
friend access supplies the weak token without editing core.hpp. Per-plane gene
sessions and landmark/polygon drafts hide other-plane data and preserve return
state. Document-only panel APIs pass image ID zero and use that weak identity;
Gene display receives the project ID and Registration retains its native frame
string. Contour/publication budgets use the captured request, so keeping the
form unchanged cannot alter publication of a previous run.
Registration's acceptance caption follows native Undo/Redo while its fit
is retained. Refused retained actions have catalogue reasons; native authorities still
decide currentness. Normal/French 200% captures verify readable controls and zero
overflow. All build/fixture corrections and changed assertions are in the report.
The standalone unmixing suite's reopened-Recheck expectation now requires the
choice to stay unchanged, preserving its native verdict/SHA assertions; its
following toggle fixture supplies its own on/off precondition. This is ruling 9,
and needed only a test-executable rebuild, with passing product evidence retained.
Gene expression retains the current plane and at most three other recently shown
planes. Registration retains pairs/source/selection for every plane, with fit,
request and preview bounded to the current plane and three recently shown others.
A referenced recipe's Remove and a duplicate Keep are disabled with a reason
instead of failing on click. The plane-hidden preview sentence in correction and
unmixing reuses an existing English caption; its translation is left to Milestone 6.
These are the plan's stated defaults and the controller's readings of its text.
**The owner has not confirmed them one by one.**

This review pass defers Registration and recipe-reference work in closed windows, clears stale current-plane gene sessions, ends editor cancellation entries on rerun, compares TMA label names without per-core string allocation, removes the unused TMA revision member, and makes `--scope-fix1 all` run its three windowed checks.
Task 5.3 intentionally changed the expectation in `cpp/tests/gui_simplification_checks.hpp` to `pending && pending->enabled==!metadata_first`: Name commits at once, so the appearance Apply is the only pending step; the Task 5.4 report did not list that historical expectation.

Scientific kernels, numeric contracts, source pixels, calibration, measurements,
record identities, persistence, exports and public replay enums are unchanged.
The 14 retained native libraries and 38 binding objects stay exact; no core or
binding build/configure, wheel/SDK change, commit, push or branch switch occurred.
Task 5.4 leaves Spatial, Project density and import windows as delivered.
Pixel/object classifier request matching remains with the native layer and
Milestone N5. Broader parity, biology, performance, platform/publication claims,
Milestone 6 and optional native follow-ups remain outside this handoff.


## Milestone 6 — Feedback and enforcement

The review stack pass measured run_gui at 362,208 → 361,072 bytes, analysis_tasks::draw unchanged at 165,232, and the tissue/cell builder bodies at 75,232/174,544 → 49,800/49,784 bytes (with 144-byte wrappers).
It moved branch-local request construction and notification observation into non-inline helpers (the observer frame is 1,056 bytes), preserving behavior and assertions; [the review fix report](../../../build/ui-workflow-safety/m6/review-fix1-report.md) records the single cached build and mapped checks.

Milestones 1–6 are delivered and uncommitted in this worktree. The frontend plan
stops here; only optional Milestone N awaits an owner decision. Task 6.4 is
**DONE_WITH_CONCERNS** for avoidable build/measurement-settling cost, with all
selected checks complete. There is no command left for the controller.

[Task 6.1](../../../build/ui-workflow-safety/m6/t61-report.md),
[foreground notification fix](../../../build/ui-workflow-safety/m6/t61-fix1-report.md),
[Task 6.2](../../../build/ui-workflow-safety/m6/t62-report.md),
[Task 6.3](../../../build/ui-workflow-safety/m6/t63-report.md),
[Task 6.4](../../../build/ui-workflow-safety/m6/t64-report.md),
[final machine results](../../../build/ui-workflow-safety/m6/results.json).
All paths in the evidence column below are relative to build/ui-workflow-safety/m6/.

| Finding | Delivered result | Evidence |
| --- | --- | --- |
| 9: attributable feedback | One session notification owner; source/path-bearing errors remain standing, Activity retains the last 50; quiet foreground edits only resolve their own error. | Final --notifications model/table/training/batch/inline/retry/stain-failure/mean-source/cells-source/scenes and relative/long cases; --milestone4-review; t61-fix1-report.md. |
| 11: vocabulary/units | One frontend owner for µm, µm², µm/px and mm²; memory Keep differs from disk Save in all 15 word families. Native schema/data units remain literal. | Final gui_i18n, --units-vocabulary, --units-guidance, source vocabulary contract; t64-catalogue-changes.json and four Russian/Indonesian captures. |
| 16: enforcement/translation | Compiled token values, runtime ComboScope stacks and literal data IDs; parsed raw-call ratchet; named localization trace with exact in-scope fixture and real new-caption/lowered-fixture probes. | Final --contract-enforcement, 85 source contracts, tests/fixtures/gui-translation-misses.json, t64-post.json, t64-negative-caption-diagnostic.json, t64-negative-lower.json; compatible Task 6.3 negative-source evidence remains scoped to its checker inputs. |
| 17: large run_gui | Selected mechanisms moved to private shared headers; function is still large. | t64-run-gui-lines.json and the ownership table below; final 120-case matrix, 39 additional cases, three focused checks and supplementary GL checks. |

### Exit line, item by item

| Exit condition | Check and actual coverage |
| --- | --- |
| Errors are attributed and persistent | --notifications verifies source, severity, path, Activity/dismiss controls, error precedence, retry, shared/inline error routes and file/job completion. The foreground fix observes that ordinary edits post nothing and clear only their own standing error. No timer expires an error. |
| One unit spelling | gui_i18n, --units-vocabulary and --units-guidance verify keys, printf conversions, actual µ/² glyphs across the composed fonts/scales/locales and shared control surfaces. Imported measurement/model schema names and CSV/native identifiers retain their exact data spelling; this is frontend vocabulary, not a native schema migration. |
| A contract test fails only for a broken user-visible rule | Native token/palette/stack/ID checks and parsed bypass guards replace the named brittle greps. Task 6.3's five named rule-break probes and harmless reformat remain compatible checker evidence; Task 6.4's actual missing-caption and stale-fixture probes fail with the required diagnostics, then restore exactly. This does not prove the statement for every repository source assertion: ambiguous native request/data-flow and licence/support guards remain exact source audits, as Task 6.3 records. |

The final candidate passes 120 matrix cases (including the complex-project
Draw/Run/Keep/Export/Save/reopen/Recheck/Remove walk), 39 additional cases and three
focused checks, plus two affected-driver reruns after a test-only rebuild.
The normal coverage and intentional-caption native draws also
pass: 166 native invocations are recorded with exact argv, cwd,
exit and executable SHA-256. The negative ratchet programs have expected exit 1.
Every file-creating check used the Q: mapping; compiler/archive/link inputs and
outputs used real workspace paths. All 14 retained libraries, 38 binding objects
and 120 public headers are exact. The 21 runtime executable/library files contain
no case-insensitive Q:\cpp in ASCII or UTF-16LE. No large hash fixture remains.

### Defaults taken, individually unconfirmed

These are **the plan's stated defaults and the controller's readings of its text**.
**The owner has not confirmed them one by one.** They are recorded as defaults,
not as additional owner selections.

- Translation scope is now **owner-selected in L3**: shell, header menus,
  registered dialogs, the four primary panes and all Workflow tasks. Specialist
  windows have unasserted missing-key counts. Existing translations in those
  windows remain; no forced-English rewrite was selected.
- L3 translates the measured in-scope captions and Project-settings additions
  in all 15 languages. The two calibration double lookups are fixed; invariant
  notation/format names and numeric templates remain data. The per-surface
  fixture now requires zero misses. Counts cover executed lookup paths rather
  than claiming every possible native sentence or raw literal is translated.
- Foreground edits post no notice and only end their own standing error.
  Success notices are posted only with a written file or a finished background
  job (Task 6.1 fix round). Native operation results and inline feedback stay with
  their existing owners.
- A standing error leaves the status bar on Dismiss error, a newer error, a
  legitimate same-source notice, a successful quiet same-source resolve, or
  session end. Unrelated-source informational/success notices do not replace it.
  Resolution/dismissal retains Activity history and selects the newest retained
  non-error entry. There is no expiry timer; inline errors keep their own existing
  clearing rules.
- "Save model", "Save pixel model" and "Save raw model" keep their names
  (Task 6.2): each writes a model file that the matching "Load" action reads
  back, so they are disk saves, not memory Keep actions.
- The **366 raw ImGui calls were recorded and not converted** into a general
  localization migration (Task 6.3). Its data-label argument repairs and scoped
  combo conversion preserve the selected control/identity contracts. The two
  raw class-editor combo pairs remain.
- The following keep/save word families, including their grammatical inflections,
  are the machine-translation choices. They have not received native-speaker
  certification. Exact changed cells and row references are in the Task 6.4 report.

| Language | Keep / kept family | Save / saved family |
| --- | --- | --- |
| es | conservar / conservado | guardar / guardado |
| fr | conserver / conservé | enregistrer / enregistré |
| de | behalten / behalten | speichern / gespeichert |
| pt-BR | manter / mantido | salvar / salvo |
| it | conservare / conservato | salvare / salvato |
| zh-CN | 保留 / 已保留 | 保存 / 已保存 |
| zh-TW | 保留 / 已保留 | 儲存 / 已儲存 |
| ja | 保持 / 保持済み | 保存 / 保存済み |
| ko | 보관 / 보관됨 | 저장 / 저장됨 |
| ru | оставить / оставлено, оставленный | сохранить / сохранено, сохранённый |
| tr | tutmak / tutulan, tutuldu | kaydetmek / kaydedilen, kaydedilmiş |
| nl | bewaren / bewaard | opslaan / opgeslagen |
| pl | zachować / zachowano, zachowany | zapisać / zapisano, zapisany |
| id | mempertahankan / dipertahankan | simpan / disimpan, tersimpan |
| vi | giữ / đã giữ | lưu / đã lưu |

The reports record these implementation deviations/default readings:

- Task 6.1 extends the narrow Notifications sketch with the existing status
  summary, cached/read-only entry access, typed UI-message keys and per-operation
  generation metadata. OutputScope/PendingGuiExport carry the actual initiating
  tool/task through delayed output. The shared channel had 38 direct assignments
  plus indirect consumers, rather than the approximate plan count. The fix round
  replaced foreground completion posts with source-specific quiet resolution.
  The report retains ten product and five test-only builds and later source/
  duplicate-notice corrections; that cost is not new scientific qualification.
- Task 6.2 treats the two composed application text faces (with their CJK/emoji
  merges) as the glyph authority; the emoji source alone is not an independent
  text face. unit_text/unit_caption preserve legacy IDs, and native/model/import
  schema/data spellings stay exact. Save model destination labels are unchanged.
  The report records four product/archive rounds and the namespace, fixture,
  sentinel, conditional-unit and argument-translation corrections, with each
  changed assertion listed there.
- Task 6.3 uses exact per-file/per-function bypass ceilings, including stale-ceiling
  diagnostics. Thirteen actual combo pairs across eight files replace the estimate;
  raw class combos remain. Data-label IDs use native/session identities; Dear
  ImGui's existing ## display truncation was not escaped. Ambiguous data-flow
  guards remain exact. The report records the obsolete test activation-ID repair,
  late three-label audit, link continuation and native design-test-only correction;
  its assertion table and failures remain the detailed evidence.
- Task 6.4 interprets the plan's empty-miss text as the required exact baseline
  ratchet, bootstrapped from final native draws without a new build for fixture
  data. Data class-color labels and filename hints bypass lookup; cached tool
  titles are measured only under the requested surface. Synthetic stress rows do
  not enter the product-caption baseline. Existing specialist translations remain
  because forcing English would violate the unchanged-language ruling. Only the
  word-family rows change translated product text.
  Two further synthetic body labels required a test-only rebuild and two driver
  reruns; the product GUI library stayed exact. Original command/executable hashes
  remain attached to their evidence, with the old test executable retained.
  A seed CRLF byte-offset repair resumed post-processing without a native rerun.
  A probe rerun refused its existing destination; a fresh numbered fixture passed.
  These failures and corrections are preserved in the task report/evidence.
- Task 6.4 exceeded the one-build request: 62 compiler
  invocations, 4 archive attempts and
  49 links, with 6 diagnosed
  failed tool commands. Successful objects were preserved until their inputs
  changed. Two earlier 120-case matrices qualify only their own candidates and
  stopped before the additional matrix. The final complete matrix is after the
  cache-attribution correction. All compiler, macro, include, path and driver
  corrections are detailed in t64-report.md; no assertion/tolerance was weakened.

### How far run_gui was reduced

At main 7c981bf9 run_gui spans lines 196–3822: **3,627 lines** including comments
and blanks. The final source spans 206–3772: **3,567 lines**, a net reduction of
**60 lines** across the plan. The retained checkpoint before Task 3.3 was 3,305
lines; later safety/UI integration grew it again. The extractions did not make it
small, and these are cumulative counts, not invented per-milestone reductions.

| Milestone | What moved behind shared private headers |
| --- | --- |
| 2 | Path field/state/destination ownership in gui_path_field.hpp and gui_core_paths.hpp; pending output/replacement policy in gui_output_gate.hpp and its output helpers. run_gui still dispatches the native writers. |
| 3 | Result parsers/presentation/lists in gui_result_state.hpp, gui_result_panel.hpp and gui_record_list.hpp; Region/Tissue/Cells bodies and exclusive sampling caches behind gui_analysis_tasks.hpp. Existing job owners/poll ordering remain authoritative. |
| 5 | Shared captured inputs/fan-out in gui_analysis_inputs.hpp; draft ownership/rebasing in gui_draft.hpp; tool/document/plane scope in gui_tool_scope.hpp and existing tool controllers. |
| 6 | Notification/history/observer behaviour in gui_notifications.hpp; unit vocabulary in gui_units.hpp; scoped/literal adapters in gui_layout.hpp; optional missing-key observation in gui_i18n.hpp. Session ownership and much coordination remain in run_gui. |

### What this milestone did not change

No scientific kernel, raw image sample, calibration/measurement rule, native
request/reuse identity, stored result identity, persistence/export policy, public
GuiInput enum, binding/SDK/wheel, reader/format admission or published candidate
changed. No biological/speed/platform acceptance or broad parity completion is
claimed. Existing native pending work/kept results were not dropped or cancelled
by these observation/presentation changes. Missing old captions remain English
unless separately selected. The final source snapshot, exact checks, remaining
coverage limits and build waste are in the Task 6.4 report. Stop at this handoff.

## Layout and action placement

L1 grouped actions with their fields/lists and added the rendered layout lint.
L2 separates Refresh/Reset glyphs, gives numbers a compact shared width, uses the
canvas divider/gap for destructive groups, and sizes the summary/Activity by
content. Text/path fields and Task 6.1 notice semantics/colours/actions stay exact.

Native checks reject near-identical glyph masks (foreground IoU >=0.90), check
numeric/strip/status geometry and enforce zero layout violations over 47 surfaces
in English/French 200%. All 170 matrix cases, 86 contracts and lossless before/
after captures pass; frozen 14 libraries/38 binding objects remain exact.
DONE_WITH_CONCERNS: one extra test-only coverage compile exceeded the one-build
target; no selected product issue remains open. See the
[L2 report](../../../build/ui-workflow-safety/layout/l2-report.md) for captures,
glyph sheet, changed assertions, commands/hashes and actual cost. L1's historical
before-audit evidence limit remains recorded in its separate report.

L3 removes the status summary’s leading separator and translates the measured
in-scope captions plus Project settings. The miss fixture is held at zero;
numeric templates/format names remain data and calibration labels are looked up
once. All 172 final native cases, 86 source contracts and French/Russian 200%
captures pass. DONE_WITH_CONCERNS for one diagnosed compile failure and an extra
caption rebuild after visual QA; no selected
product issue remains open. See the
[L3 report](../../../build/ui-workflow-safety/layout/l3-report.md).
