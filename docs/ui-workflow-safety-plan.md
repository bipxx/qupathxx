# UI workflow safety and contracts plan

Status: milestones 1–6 and a follow-up layout/translation pass are delivered,
committed on feature/ui-workflow-safety and merged into main. The
[receipt](benchmarks/ui-workflow-safety/README.md) records results, individually
unconfirmed defaults, coverage limits and extra build cost. Native libraries and
binding objects were not rebuilt. Only Milestone N awaits an owner decision.
This plan follows the
owner-requested UI/UX and contract review of `c0f64039`; every code anchor below
was re-checked on `main` at `7c981bf9`. The
[workflow simplification plan](ui-workflow-simplification-plan.md) is delivered
history and its optional phase 4 stays a separate selection.

> **For agentic workers:** [AGENTS.md](../AGENTS.md) governs scope, validation
> cost, card updates and commits (owner request only). Tasks use checkbox
> (`- [ ]`) syntax. superpowers:subagent-driven-development or
> superpowers:executing-plans can drive a selected milestone; where they differ
> from AGENTS.md, AGENTS.md wins.

**Goal:** the desktop never silently discards, duplicates, overwrites or
re-targets a researcher's work, and says what each action will affect and what
it did.

**Architecture:** every default milestone is frontend-only. It changes
`cpp/gui*.{hpp,cpp}`, GUI tests, the i18n catalogue and docs, and calls the
existing C++ authorities unchanged. Milestone 1 is targeted fixes in place.
Milestones 2 to 6 each add one small shared GUI component, migrate its callers
and move the state they touch out of `run_gui`. Anything that needs a core or
binding change is isolated in optional milestone N.

**Tech stack:** C++ with the pinned Dear ImGui docking build, the RmlUi shell and
GLFW/OpenGL 3; native test executables registered in CTest (`gui_organization`,
`gui_replay`, `gui_i18n`, `gui_design` and the per-tool suites); Python
source-contract tests run with `uv run --no-project --with pytest pytest`.

**Spec:** [Appendix A](#appendix-a--findings-register) is the findings register.
Existing rules live in [design-system.md](design-system.md) and
[ui-dialog-contract.md](ui-dialog-contract.md).

## Global constraints

- C++ owns application logic. UI code calls existing native authorities and may
  use session identity only (job generation, native record IDs, object
  pointers). It never invents an identity, reuse or equality rule.
- Native core, bindings, SDK and published candidates stay frozen in milestones
  1 to 6: the 14 cached libraries and 38 binding objects are byte-identical
  before and after.
- No value is added, removed or renumbered in the public `GuiInputKind`,
  `GuiInputTarget` or `GuiInputWait` enums. Existing `###` IDs and
  `ui::WorkflowAction` `original` strings keep their spelling.
- `GuiOptions` fields keep their meaning for scripted and replay sessions.
- Every new or changed visible caption gets all 15 translations in
  `cpp/gui_i18n_catalog.inc` with identical printf conversions.
- Vocabulary: Run/Detect/Train start work; Add creates objects; Keep retains a
  record in memory; Save project writes to disk; Export writes an external
  file; Cancel stops work; a window's X only hides it. "Saved" is used only for
  things on disk.
- Modals go through the `DialogKind` registry and `RenderDialog` only.
- No assertion, tolerance, quota or stored identity is weakened to make a check
  pass. Scientific defaults, parameters and stored values do not change.
- Commits, merges and launches follow the owner's request. Each milestone stops
  at its handoff.

## Review focus

These are the conditions most likely to hurt a user that a task's own happy-path
test would not reach. Each is pinned by a named test in the owning task.

1. A job is running in another task or window when shared inputs change. The
   user is told what was cancelled, and unrelated work keeps running (task 5.1).
2. The destination already exists, is a relative name, or contains non-ASCII
   characters. One dialog asks, the resolved absolute path is shown, and
   `u8path` is used throughout (tasks 2.1, 2.3).
3. The selection or document changes while a draft or preview is open. Dirty
   drafts survive or the pane says they were dropped (tasks 5.3, 5.4).
4. A chord arrives while a text field is active, a modal is open, or the key is
   held down. Nothing mutates (tasks 1.4, 4.2).
5. A non-English locale at 200% scale. New strings are translated,
   `ui_overflow_items` is 0 and the scale bar stays legible (tasks 1.6, 6.4).

## Milestones

Each milestone is independently selectable and ends with working, tested
software. The order is the recommended one.

| # | Milestone | Findings | New shared code | Relative size |
| --- | --- | --- | --- | --- |
| 1 | Stop silent harm | 3, 4, 5, 6, 7, 8, 9, 10 in part | one dialog kind, one palette helper | medium, 13 tasks |
| 2 | Files and overwrite policy | 1, 2 | `gui_path_field.hpp`, `gui_output_gate.hpp` | large |
| 3 | Kept-result lifecycle | 3, 9, 11, 14, 15 | `gui_result_state.hpp`, `gui_result_panel.hpp`, `gui_record_list.hpp` | large |
| 4 | Command table | 6, 9, 12, 13 | `gui_commands.hpp` | medium |
| 5 | Inputs, drafts and tool scope | 4, 5, 8, 18 | `gui_analysis_inputs.hpp`, `gui_draft.hpp`, `gui_tool_scope.hpp` | medium |
| 6 | Feedback and enforcement | 9, 11, 16, 17 | `gui_notifications.hpp`, `gui_units.hpp` | medium |
| N | Optional native follow-ups | residue of 3, 7, 10, 15, 18 | core and bindings | owner decision |

Decisions the owner should confirm when selecting a milestone are listed under
that milestone. The stated default is what this plan implements if nothing else
is said.

---

## Milestone 1 — Stop silent harm

Targeted fixes inside the existing code. New test file
`cpp/tests/gui_workflow_safety_checks.hpp` (namespace `workflow_safety_checks`)
with `headless()` added to the unconditional list in
`cpp/tests/gui_organization_tests.cpp` `main` and `workflows(dir)` behind a new
`--workflow-safety` flag (opt-in `QUPATHXX_TEST_GUI=1`, like `--simplification`).

**Decisions to confirm:** Merge and Boolean move from bare `M`/`B` to `Shift+M`/
`Shift+B`; `Ctrl+Alt+B` only opens Batch; `Ctrl+Shift+Z` becomes Redo.

### Pass A — data safety

Pass A implementation (tasks 1.1–1.5) is complete. The owner waived the
intentional failing runs to avoid extra builds; their checked boxes record that
waiver. Native workflow/headless checks pass. Source-contract execution remains
pending controller access to uv's cache; the combined task 1.3 validation box
therefore stays open. The source-contract runs mentioned in tasks 1.5's checked
implementation steps are also pending, rather than claimed passing.
`RunBatchKey` is tested under a blocking dialog only; it is an explicit replay
command, not text-field keyboard input. See the Pass A report under
`build/ui-workflow-safety/m1/pass-a-report.md` for results and deviations.


### Task 1.1: Recheck verifies; it never offers Keep or rewrites inputs

**Files:** modify `cpp/gui.cpp` (`:425` state, `:2534` consume, `:2679`-`:2733`
Region, `:2834`-`:2860` Tissue, `:2950`-`:2977` Cells, `:975`/`:980` verify);
test `cpp/tests/gui_workflow_safety_checks.hpp`.

- [x] Write `recheck_never_duplicates(dir)`. For each of region, tissue and
  cells: replay `SelectAllKey`, Run, Keep, Recheck, Keep (the existing
  `AnalyzeKey`/`KeepAnalysisKey`/`RecheckAnalysisKey` kinds and their tissue and
  cell equivalents, with the waits the `simplification_checks::workflows` Run/
  Keep/Recheck scenario uses). Assert `doc->analysis_record_count()==1` (resp.
  `tissue_record_count()`, `cell_record_count()`) and status `"current"`.
- [x] Write `recheck_keeps_inputs(dir)`. Session 1 with `options.analysis_kind=1`
  runs and keeps, then saves. Session 2 on the same project with
  `options.analysis_kind=0` replays Recheck, Run, Keep. Assert two records and
  that the newest has `result.kind==AnalysisKind::OpticalDensityMeans`.
- [x] Run `gui_organization ... --workflow-safety`; both fail (count 2; kind
  `StainMeans`).
- [x] Add `recheck_analysis_generation`, `recheck_tissue_generation`,
  `recheck_cell_generation` beside `kept_analysis_generation`. Set each where
  its pending recheck is consumed. Add `!preview_is_recheck` to `can_keep`,
  `can_archive` and `can_keep_cells`; hide the Region preview block when
  `preview_is_kept || preview_is_recheck`. Under a disabled Keep show
  `"This run is a recheck of kept result %llu."`.
- [x] Delete the input assignments in the Region recheck (`:2724` onward) and
  the Tissue recheck (`:2858` onward). Cells already leaves inputs alone.
- [x] Run the flag again; both pass.

### Task 1.2: A live result can be kept once

**Files:** modify `cpp/gui_raw.hpp:310`, `cpp/gui_pixel_stream_browser.hpp`
(Keep verified stream link).

- [x] Write `keep_once(dir)`: click `GuiInputTarget::KeepRawAnalysis` twice;
  assert `doc->raw_analysis_record_count()==1`. Same for
  `KeepPixelStreamLink` and the stream record count.
- [x] Remember the kept object by pointer (`const void* kept_result_` set from
  `analysis_.get()`; `kept_link_` from `stats_.link.get()`), disable Keep while
  it equals the live one, and print `"Kept as record %llu."`.
- [x] Pixel classifier (`cpp/gui_pixel_classifier.hpp:137`, `:287`): keeping a
  result retires the live prediction by design
  ([pixel-classifier-ui.md](pixel-classifier-ui.md)). After a successful Keep
  print `"Kept as record %llu. Predict again to trace contours or add
  annotations."` beside the Keep button instead of only "Prediction: stale".

### Task 1.3: Removing a kept result asks first, and every family can remove

**Files:** create `cpp/gui_record_removal.hpp`; modify
`cpp/gui_dialog_contract.hpp` (registry row, `DialogOwnerScope::Record`, action
case), `cpp/gui.cpp:2864` and the Region and Cells kept blocks,
`cpp/gui_spatial.hpp:156`, `cpp/gui_raw.hpp:330`,
`cpp/gui_pixel_classifier.hpp:285`, `cpp/gui_raw_pixel.hpp:170`,
`docs/ui-dialog-contract.md` (registry table); tests
`cpp/tests/gui_dialog_contract_checks.hpp`, `tests/test_gui_dialog_contract.py`.

**Interfaces (produces):**

```cpp
// cpp/gui_dialog_contract.hpp
X(RemoveKeptResult,"Remove kept result?",Confirmation,"gui_record_removal.hpp",Record)
case DialogKind::RemoveKeptResult:actions.push_back({A::Confirm,"Remove kept result",R::Destructive,P::Footer,C::DismissOnSuccess});break;

// cpp/gui_record_removal.hpp
namespace qpx::detail {
struct RecordRemoval {std::string label;const Document* document=nullptr;std::uint64_t epoch=0;
    std::function<bool()> exists;std::function<void()> remove;};
class RecordRemovalDialog {
public:
    void request(RecordRemoval removal);                                  // opens the registered dialog
    void draw(const Document& active,std::uint64_t epoch,std::string& error); // call once per frame
    bool pending() const;
};
}
```

- [x] Add headless checks in the dialog-contract idiom (`Fixture`, `warm()`,
  `ActivateItemByID`): Cancel leaves `tissue_record_count()` unchanged; Confirm
  lowers it by one; `exists()==false` or a different document disables Confirm
  and shows the stale-owner notice.
- [x] Register the kind and implement the controller. Body text: the record
  label, then `"This cannot be undone. Save project then writes the removal to
  disk."`.
- [x] Route the five existing Remove buttons through `request(...)`. Keep their
  `original` IDs; the visible caption becomes `"Remove kept result"` and moves
  off the Inspect/Verify row in Spatial.
- [x] Add the same button to the Region and Cells kept blocks, calling the
  existing `document.remove_analysis(id)` and `document.remove_cells(id)`.
- [x] Run `gui_replay`, `gui_organization` and
  `uv run --no-project --with pytest pytest tests/test_gui_dialog_contract.py -q`;
  all pass.

### Task 1.4: Keyboard hazards

**Files:** modify `cpp/gui.cpp` (`:2329`, `:2346`, `:1202`, `:1206`, `:1925`,
`:2088`, `:1688`-`:1696`, `:1819`), `cpp/gui_batch.hpp:142`-`:145`,
`cpp/gui_raw.hpp:255`, `cpp/gui_tma.hpp:289`.

- [x] Write `chords_do_not_mutate(dir)` (review focus 4). With two annotations
  selected, an open registered dialog and then an active text field, replay
  `MergeKey`, `BooleanKey`, `DeleteKey`, `AddImageKey`, `RunBatchKey`. Assert
  `doc->revision()`, `project->images().size()` and `result.batch_state`
  (`"idle"`) are unchanged.
- [x] Write `add_image_key_never_repeats(dir)`: `options.project_image_path` set,
  `AddImageKey` twice. Assert one new image and that the second press only
  reveals the Add form.
- [x] Merge and Boolean require Shift. The replay switch emits `Shift+M` and
  `Shift+B` for `MergeKey` and `BooleanKey`; tooltips read `Shift+M`/`Shift+B`.
- [x] After a successful add, clear `project_image_path`; read F8 with
  `repeat=false`; when the typed file equals an existing entry's source, show
  `"This file is already in the project as %s."` and still allow Add.
- [x] `Ctrl+Alt+B` sets `open_=true` only. Add `GuiBatch::request_run()` and
  `request_cancel()`; the replay cases for `RunBatchKey`/`CancelBatchKey` call
  them, so scripted sessions keep their meaning.
- [x] Undo is exactly `Ctrl+Z`; `Ctrl+Shift+Z` and `Ctrl+Y` redo. `E`, `P` and
  `H` ignore the key when Ctrl or Alt is down. F2 and `Ctrl+Alt+T` test
  `!blocked`. `Ctrl+N` tests `!io.WantTextInput` and `repeat=false`.
- [x] Run the flag and `gui_replay`; pass. Replays keep working because the
  kinds keep their meaning; update only checks that assert the old chord text,
  keeping every other assertion.

### Task 1.5: Adapter pairs match

**Files:** modify `cpp/gui_spatial_import.hpp:160`,
`cpp/gui_registration.cpp:108`, `cpp/gui_genes.hpp:57` and `:213`,
`tests/test_gui_design_contract.py`.

- [x] Add to the design contract test: no `ImGui::EndCombo()` outside
  `gui_layout.hpp` in any file that calls `ui::BeginCombo(`. Run; fails on four
  sites.
- [x] Replace the four calls with `ui::EndCombo()`. Run; passes.

### Pass B — truthful display

### Task 1.6: Labels and overlays that can mislead

**Files:** create `cpp/gui_class_palette.hpp`; modify
`cpp/gui_marker_rules.cpp:77`, `cpp/gui_pixel_classifier.hpp:75`,
`cpp/gui_raw_pixel.hpp:64`, `cpp/gui_pixel_stream_browser.hpp` (palette copy),
`cpp/gui_overlay.hpp`, `cpp/gui.cpp:3398` and `:3485`,
`cpp/gui_fluorescence.cpp:73`.

**Interfaces (produces):**

```cpp
// cpp/gui_class_palette.hpp
std::vector<std::uint32_t> class_palette(const Document&,const std::vector<std::string>& classes);
// cpp/gui_overlay.hpp
struct ScaleBarLayout {float label_offset_y,thickness,outline;};
ScaleBarLayout scale_bar_layout(float text_line_height,float ui_scale);
```

- [x] Headless checks: two classes with the default colour get different
  palette entries and an explicit class colour is kept; for scales 1, 1.5 and 2
  the label's bottom is above the bar's top
  (`label_offset_y>=text_line_height+thickness`).
- [x] `class_palette` implements the Pixel-window rule (document colour unless
  it is the default `0x41e1be`, then the cycling eight-colour table). Use it in
  all three windows and draw a class legend in Raw pixel.
- [x] Marker units by domain: 0 `"original channel samples"`, 1
  `"dark/flat-corrected samples"`, 2 `"selected derived component/residual"`.
- [x] Scale bar uses `scale_bar_layout` and draws a dark outline under the
  white bar and label.
- [x] Object information prints `"all channels"` when `plane.c<0`.
- [x] When the chosen correction stops matching, clear the recipe but keep the
  box ticked, so Run is blocked with `"The selected correction no longer
  matches this image. Choose it again in Dark/flat correction."`.
- [x] Pixel classifier Training (`cpp/gui_pixel_classifier.hpp:132`) prints
  `"Training uses %zu labelled areas (%zu added by a classifier in this
  session)."`, counting the labelled IDs that are also in the session's
  published IDs. Excluding them is item N4.
- [x] GL check at French 200% (review focus 5): `ui_overflow_items==0`.

### Task 1.7: Doubled tooltips and chords left in captions

**Files:** modify `cpp/gui.cpp:1527` (`names[16]`), `:1740`, `:3085` and the
Review-for-project button, `cpp/gui_batch.hpp:288`,
`cpp/gui_tool_catalog.hpp:30`.

- [x] Headless check: for every shell label and every `ui::IconButton` hint,
  the composed text does not end in two parenthesised groups.
- [x] `names[16]` becomes `"Raw channels"`; the eye button becomes
  `"Show objects###Show objects (O)"`; Run batch, Hash file and Review for
  project go through `ui::WorkflowAction(original, caption, shortcut)`; the
  tool definition becomes label `"Raw channels..."` with shortcut `"F2"`.

### Task 1.8: Object classifier and TMA say what they will touch

**Files:** modify `cpp/gui_object_classifier.hpp:101` and `:197`,
`cpp/gui_tma.hpp:426`, `:436`, `:439`-`:442`, `:467`.

- [x] Checks: with zero changes the window shows `"No objects changed."`; a
  completed preview survives a selection change; `selection.replace` at both
  TMA sites runs inside `perform` (a core that is not visible yields an error
  line, not an exception).
- [x] Cancel only a running classifier job on selection change. Print
  `"(Undo available)"` only when `changed_>0`.
- [x] Move the current-core line above the TMA tab bar. Print
  `"Applies to %zu cores."` beside Missing and Apply case ID. Captions become
  `"Remove row %s (%zu cores)"` and `"Remove column %s (%zu cores)"` with their
  IDs unchanged.

### Pass C — reachability and feedback

Implemented in this pass. Checked steps below record implementation and the
checks added/run; output-path canonicalization prevents several executable
completions in this sandbox. The [receipt](benchmarks/ui-workflow-safety/README.md)
and `build/ui-workflow-safety/m1/pass-c-report.md` identify the exact controller
commands. Historical Pass A pending notes are superseded by its fix-round-1
report and Pass B/controller evidence; no earlier source is relabeled.

### Task 1.9: Raw channels opens wherever it works

**Files:** modify `cpp/gui_raw.hpp:60`, `cpp/gui.cpp:1343`, `:1531`, `:1715`,
`:1739`; test `cpp/tests/gui_zarr_tests.cpp`.

- [x] Check: with an OME-Zarr document, `tools.open(ToolId::Channels, context)`
  returns true.
- [x] Add `static bool GuiRaw::supports(std::string_view reader_id)` and use it
  at the five sites.

### Task 1.10: Blocked actions state their reason

**Files:** modify `cpp/gui_fluorescence.cpp:125`-`:146`,
`cpp/gui_batch.hpp:285`-`:288`, `cpp/gui_project_density.hpp:119`.

- [x] Checks in `gui_fluorescence`: with nothing selected the Run reason line is
  present; after a tiled run the tiled sentence replaces the two greyed
  buttons. In `gui_organization`: Batch with no output folder shows the reason
  beside Run and does not start; Density keeps its check table after a Run that
  fails with an existing output folder.
- [x] Fluorescence prints the first unmet condition under Run (strings in the
  copy table).
- [x] Batch draws `error()` beside the status line above the scrolling recipe
  and the first unmet prerequisite beside Run.
- [x] Density drops `preview_.reset()` from the Run lambda and resets the check
  when a result arrives or inputs change.

### Task 1.11: Exports and counts report what happened

**Files:** modify `cpp/gui.cpp` (`:1422`, `:1447` dispatch, `:2984`, `:3013`,
`:3062`), `cpp/gui_tma.hpp:387` and the CSV status line,
`cpp/gui_spatial.hpp` (Export line).

- [x] Checks: after each export the status text contains the absolute
  destination; the Cells-task Export CSV with an empty Tables path focuses the
  Tables CSV field and writes nothing; under Project scope the count line sums
  all project images.
- [x] Record the dispatched absolute path per job and print
  `"Exported %llu rows to %s"` (tables) and `"Exported %zu objects to %s"`
  (GeoJSON). Show the success line under Tables only; busy and failed states
  stay visible under every task.
- [x] GeoJSON import (`cpp/gui.cpp:1495`, button and F7) reports
  `"Imported %zu objects from %s (Undo available)"`.
- [x] The count line becomes `"Current image: %zu ROI / %zu tissue / %zu cell
  runs kept"`, with a `"Project: ..."` line when the scope is Project.

### Task 1.12: Stop, close and resize keep finished work

**Files:** modify `cpp/gui_pixel_classifier.hpp:213`,
`cpp/gui_pixel_stream_browser.hpp:76`, `cpp/gui.cpp:3138`,
`cpp/gui_registration.cpp:35`-`:38`; tests `cpp/tests/gui_registration_tests.cpp`
and the safety checks.

- [x] Checks: pressing Stop during contour tracing leaves
  `result.pixel_preview_ready` true; hiding the stream browser after Verify
  keeps the verified state; a polygon draft survives a window resize; landmark
  pairs survive a server swap and return after a plane round trip.
- [x] Stop cancels only jobs whose `busy()` is true. The stream browser's X
  only hides. Canvas resize ends drag gestures but keeps `draft_points`.
- [x] Registration: `changed` no longer includes `server!=doc.server()`; on a
  plane change stash `pairs` per plane and restore them on return.

### Task 1.13: Catalogue and receipt

- [x] Add the copy below to `cpp/gui_i18n_catalog.inc` with 15 translations
  each. Run `gui_i18n`; pass.
- [x] One combined cached frontend build, then the suites in
  [Validation and handoff](#validation-and-handoff).

**Copy introduced by milestone 1**

| Key | Where |
| --- | --- |
| `Remove kept result`, `Remove kept result?`, `This cannot be undone. Save project then writes the removal to disk.` | 1.3 |
| `This run is a recheck of kept result %llu.` | 1.1 |
| `Kept as record %llu.` | 1.2 |
| `This file is already in the project as %s.` | 1.4 |
| `dark/flat-corrected samples`, `all channels` | 1.6 |
| `The selected correction no longer matches this image. Choose it again in Dark/flat correction.` | 1.6 |
| `Raw channels`, `Raw channels...` | 1.7 |
| `No objects changed.`, `Applies to %zu cores.`, `Remove row %s (%zu cores)`, `Remove column %s (%zu cores)` | 1.8 |
| `Fluorescence analysis needs a quantitative TIFF image.`, `Select an area annotation or turn off Use selected ROI`, `Choose an unmixing recipe in Linear unmixing, or turn off Use selected unmixing.` | 1.10 |
| `Tiled runs write masks and measurements to the folder above. They are not added to the project.` | 1.10 |
| `Open a project to run a batch.`, `Open an image to run a batch.`, `Enter an output folder.` | 1.10 |
| `Kept as record %llu. Predict again to trace contours or add annotations.` | 1.2 |
| `Training uses %zu labelled areas (%zu added by a classifier in this session).` | 1.6 |
| `Exported %llu rows to %s`, `Exported %zu objects to %s`, `Imported %zu objects from %s (Undo available)` | 1.11 |
| `Current image: %zu ROI / %zu tissue / %zu cell runs kept`, `Project: %zu ROI / %zu tissue / %zu cell runs kept` | 1.11 |

**Exit:** no path keeps a rechecked or already-kept result twice; every Remove
of a kept result is confirmed; no bare key mutates; the four misleading labels
and overlays are corrected; every blocked primary action names its reason.

---

## Milestone 2 — Files and overwrite policy

Findings 1 and 2. One path component and one overwrite rule replace roughly 35
text fields, six "Replace ..." checkboxes and three protected-path lists.

**Decisions taken (the plan's stated defaults):** relative names resolve against the project folder
(then the last used folder, then the process directory) instead of always the
process directory; the in-app picker is extended rather than adding OS-native
dialogs.

**Interfaces (produces):**

```cpp
// cpp/gui_path_field.hpp
namespace qpx::ui {
enum class PathRole {OpenFile,NewFile,OpenFolder,NewFolder};
struct PathSpec {const char* label;PathRole role;std::vector<std::string> extensions;const char* suggestion="";};
struct PathContext {std::string project_directory;std::string* last_directory=nullptr;};
struct PathState {std::string resolved;bool exists=false,valid=false;std::string problem;};
std::string resolve_user_path(std::string_view text,const PathContext&);
class PathField {
public:
    explicit PathField(PathSpec);
    bool draw(const PathContext&,bool disabled=false);   // label, text, Browse, resolved-path line
    const PathState& state() const;                      // refreshed on edit, not per frame
    const char* text() const;void set(std::string_view);
    bool hovered() const;void drop(const std::string& path);
};
}
// cpp/gui_output_gate.hpp
namespace qpx::detail {
struct OutputIntent {std::string path,scope;std::vector<std::string> protected_paths;ExportOwnerStamp owner;
    bool preauthorized=false;                      // a GuiOptions replace flag set by a script
    std::function<void(ExportWritePolicy)> write;std::function<void(const std::string&)> fail;};
class OutputGate {
public:
    void submit(OutputIntent);                      // new file: write(CreateNew); existing: ask
    void draw(const std::function<bool(const ExportOwnerStamp&)>& owner_valid);  // DialogKind::ReplaceExport
    bool pending() const;
};
}
```

### Task 2.1: PathField and path resolution

**Files:** create `cpp/gui_path_field.hpp`; modify `cpp/gui_import_picker.hpp`,
`cpp/gui_dialog_contract.hpp` (variants `Folder`, `NewFolder` for
`ImportFile`), `cpp/gui_project_preferences.hpp` (`; qupathxx last-folder=`).

- [x] Headless checks (review focus 2): `resolve_user_path("results.csv", ctx)`
  equals `<project_directory>/results.csv`; with no project it uses
  `*last_directory`; an absolute or non-ASCII path round-trips through
  `u8path` unchanged; `PathState.exists` and `.problem` are right for an
  existing file, a missing parent and a folder given to a file role.
- [x] Picker checks: `.qpxproj` is listed for the project role; entries are
  sorted with folders first; a folder with 1,000 non-matching files shows its
  first match on page one; Previous returns to the earlier page; folder roles
  list folders only.
- [x] Implement `PathField`, extend the picker with `PickerMode`, sorting,
  scan-until-64-matches, backward paging and an "All files" toggle, keeping the
  existing two-argument `open(...)` overload.

### Task 2.2: Core path fields and multi-file Add

**Files:** create `cpp/gui_project_forms.hpp` (the Open, Save As, Add and Relink
forms and their state, moved out of `run_gui`); modify `cpp/gui.cpp` (`:361`
drop callback, `:1865`-`:1965` forms, `:2088` add, `:2455`-`:2456`, `:3019`,
`:3027`, stain model and fingerprint fields),
`tests/test_gui_image_inspector_contract.py` and any source contract that
slices the moved text.

- [x] Checks: dropping three image paths queues three adds and reports per-file
  errors; dropping a `.qpxproj` fills and reveals the Open form without opening
  it; `options.project_open_path`, `options.project_image_path`,
  `options.interchange_path` and `options.measurement_table_path` still
  prefill their fields.
- [x] Register `glfwSetDropCallback`; route a drop to the hovered `PathField`,
  otherwise to Open (project files) or the Add queue (everything else).
- [x] The Add form processes its queue one file per frame with
  `"Adding %zu of %zu"`.
- [x] Split the shared destinations: GeoJSON and object CSV get their own
  fields; ROI and tissue tables get their own. Export with an empty field
  opens the picker in `NewFile` mode with a suggested name
  (`<project stem>-<kind>.csv`).
- [x] Regroup Annotations ▸ Import / export (`cpp/gui.cpp:2453`-`:2485`) into
  four headed groups, each holding only its own options: Clipboard (Copy
  descendants), Import GeoJSON (source, Assume image pixels, Allow lossy
  conversion), Export GeoJSON (destination, ellipse bounds, nested hierarchy,
  Selected objects only) and Export measurements CSV (destination, Include
  mean RGB). "Paste to displayed plane" uses the Paste glyph, not Import.
  Widget IDs and replay targets are unchanged.

### Task 2.3: One overwrite rule

**Files:** create `cpp/gui_output_gate.hpp` (lift `PendingGuiExport`,
`begin_export` and `dispatch_export` from `cpp/gui.cpp:1447`); modify the tool
windows that write files: `cpp/gui_tma.hpp:181`-`:248`,
`cpp/gui_raw.hpp` (NumPy and CSV), `cpp/gui_spatial.hpp` (CSV),
`cpp/gui_pixel_classifier.hpp:215` and `:260`,
`cpp/gui_object_classifier.hpp:156`, `cpp/gui_raw_pixel.hpp:137`-`:153`,
`tests/test_gui_export_contract.py`.

- [x] Checks for each writer: a new destination writes without a dialog; an
  existing one opens `ReplaceExport`, Cancel preserves its bytes and Confirm
  replaces them; a protected path is refused before any work; a `GuiOptions`
  replace flag set to true still replaces without a dialog.
- [x] Every writer calls `gate.submit(...)`. Remove the six checkboxes from the
  windows. Use `export_protected_paths` as the single protected-path list.
- [x] Each success posts the absolute path with Copy path and Open folder,
  through a shared `ui::open_in_shell(path)` lifted from
  `cpp/gui_project_density.hpp:39`.

### Task 2.4: Tool-window path fields

- [x] Replace the remaining path inputs with `PathField` in TMA, Batch,
  Density, the classifier and raw windows, the fluorescence family and Spatial.
  Split fields that are both a read source and a write target (Raw pixel
  "Prediction file", the two model fields).

**Exit:** no file operation requires typing a path; every output shows where it
went; exactly one overwrite dialog exists and no checkbox arms a replace.

---

## Milestone 3 — Kept-result lifecycle

Findings 3, 9, 11, 14 and 15. One state vocabulary, one presenter and one
record list replace thirteen hand-built blocks. Native tokens stay the wire
format; the GUI parses them once.

**Interfaces (produces):**

```cpp
// cpp/gui_result_state.hpp
namespace qpx::ui {
enum class RunState {Idle,Running,Ready,Stale,Cancelling,Cancelled,Failed,Verified,Historical,Unknown};
enum class RecordState {Current,Unverified,ObjectChanged,SourceChanged,SourceClosed,VerificationFailed,Unknown};
enum class ExportState {Idle,Writing,Succeeded,Cancelled,Failed,Unknown};
RunState parse_run_state(std::string_view);
RecordState parse_record_state(std::string_view);
ExportState parse_export_state(std::string_view);   // "saved" and "succeeded" both map to Succeeded
const char* caption(RunState);const char* caption(RecordState);const char* caption(ExportState);
}
// cpp/gui_result_panel.hpp
struct ResultIds {const char* keep;const char* view;const char* recheck;const char* add;const char* remove;};
struct KeptView {std::uint64_t id=0;std::string label;RecordState state=RecordState::Unknown;bool can_add=false;};
struct ResultModel {RunState run=RunState::Idle;std::string phase,error;std::uint64_t done=0,total=0;
    bool has_preview=false,preview_is_kept=false,preview_is_recheck=false,project=false;
    std::optional<KeptView> kept;std::size_t kept_count=0;};
struct ResultActions {std::function<void()> keep,view,recheck,add,remove,cancel;};
void draw_run_status(const ResultModel&,const ResultActions&);   // sentence, progress, Stop, error
void draw_keep_row(const ResultModel&,const ResultIds&,const ResultActions&);
void draw_kept_block(const ResultModel&,const ResultIds&,const ResultActions&,const std::function<void()>& details);
// cpp/gui_record_list.hpp
struct RecordRow {std::uint64_t id=0;std::string label,kind;RecordState state=RecordState::Unknown;};
struct RecordListActions {std::function<void(std::uint64_t)> use,preview,recheck,remove;};
std::uint64_t draw_record_list(const char* id,const std::vector<RecordRow>&,std::uint64_t selected,const RecordListActions&);
```

### Task 3.1: State vocabulary

- [x] Headless checks: every token the native layer emits today maps to a named
  state (tokens listed from `analysis_status`, `tissue_status`, `cell_status`,
  the job `state` strings and the export states); an unexpected token maps to
  `Unknown` and renders `"Unknown status (%s)"`, never "Recheck failed".
- [x] Implement the parsers and captions; add the captions to the catalogue.

### Task 3.2: Region, Tissue and Cells use one presenter

**Files:** create `cpp/gui_analysis_tasks.hpp` (the three task bodies and their
state, moved from `cpp/gui.cpp:2508`-`:2993`); modify `cpp/gui.cpp`, the source
contracts that slice that region.

- [x] Characterisation first: record the `ui::WorkflowTrace` actions and
  results for no result, running, preview, kept, stale, cancelled and failed in
  each task, before moving code.
- [x] Move the three bodies behind `draw(TaskContext&)` and build them from
  `ResultModel`. `ResultIds` carries today's `original` strings, so IDs,
  replay kinds and trace keys are unchanged.
- [x] After the move the three tasks behave alike: same status captions, same
  primary rule, Remove in all three, provenance drawn inside the kept block,
  timing moved into Details.
- [x] The characterisation traces match except for the intended differences,
  which the test names one by one.

### Task 3.3: Record lists and vocabulary

**Files:** modify `cpp/gui_pixel_classifier.hpp:270`-`:290`,
`cpp/gui_object_classifier.hpp:205`-`:225`, `cpp/gui_raw_pixel.hpp:160`-`:173`,
`cpp/gui_raw.hpp:320`-`:333`, `cpp/gui_spatial.hpp:147`-`:157`, the
fluorescence family record combos, and the Tables task in `cpp/gui.cpp:3010`.

- [x] Checks: Raw pixel selects a record from a list (no typed ID); the object
  classifier can remove a record (existing
  `document.remove_classifier_record`); Spatial's table and Export follow the
  highlighted row; Tables lists every kept run of the current image with its
  object name and state.
- [x] Replace each block with `draw_record_list`. Rows read
  `"<name or family> #<short id>"`; 64-hex identifiers move into Details.
- [x] Tables gains the scope `"Current image (all kept runs)"` for cells, built
  from `document.cell_records()`; the existing selected-annotation scope keeps
  its exact request.
- [x] Rename the remaining "Saved ..." captions for in-memory records to
  "Kept ..." (IDs unchanged), and "Published" to "Added" in status lines.

### Task 3.4: Looking at a kept record does not replace the working model

**Files:** modify `cpp/gui_pixel_classifier.hpp:74`, `:134`, `:283`,
`cpp/gui_raw_pixel.hpp:148`-`:149`.

- [x] Checks: train a model, then Preview kept mask on an older record; the
  active model's SHA and feature options are unchanged and the historical mask
  is drawn with the record's own classes. With an unkept, unsaved model, Use
  kept model and Load show the warning line first.
- [x] Draw a kept mask from the record's archive and class list without calling
  `adopt()`.
- [x] Track `model_unsaved_` (set when training adopts a model, cleared by Keep
  training data or Save). While it is set, show `"The current model has not
  been kept or saved."` beside Use kept model and Load.
- [x] Raw pixel and Pixel classifier use the same model-file row with Load
  before Save.

### Task 3.5: Batch results are reachable

**Files:** modify `cpp/gui_batch.hpp:275`-`:283`.

- [x] First confirm the suspected dead end: open a batch checkpoint
  `.qpxproj` in the desktop and export its table. Record the outcome in the
  receipt.
- [x] After a run, show the output folder with Open folder
  (`ui::open_in_shell` from task 2.3, or the existing
  `GuiProjectDensity::open_file` if milestone 2 has not run) and relabel
  `"Publish cell objects"` to `"Publish cell objects (in checkpoint projects)"`.
- [x] If checkpoints do not open as projects, add `"Export combined CSV"`,
  which calls the existing native `save_batch_checkpoint_csv` for each
  completed image through the overwrite rule of task 2.3.

**Exit:** one status vocabulary on screen; every kept family can be listed,
rechecked and removed the same way; no raw token or 64-hex ID is the primary
label; inspecting a kept record never changes the working model.

---

## Milestone 4 — Command table

Findings 6, 9, 12 and 13. One table feeds menus, the shell, the tool catalogue,
key dispatch, tooltips, disabled reasons and Help.

**Decisions to confirm:** the per-job cancel chords are retired in favour of the
Stop button; F7 keeps importing immediately but reports the file and
"Undo available".

**Interfaces (produces):**

```cpp
// cpp/gui_commands.hpp
namespace qpx::ui {
// One enumerator per existing HeaderCommand, ShellCommand, ToolId, ToolChild and WorkflowAction
// `original`, named after the enumerator or action it replaces; `Count` last.
enum class CommandId : std::uint16_t;
enum class KeyScope : std::uint8_t {Global,NotInText,Canvas};
struct Availability {bool enabled=true;const char* reason=nullptr;};
struct CommandInfo {CommandId id;const char* caption;ImGuiKeyChord chord;KeyScope scope;bool repeat;const char* legacy_id;};
const CommandInfo& command(CommandId);
std::string chord_text(ImGuiKeyChord);
struct ChordContext {bool dialog_open=false,text_input=false,canvas_focused=false;};
bool chord_pressed(CommandId,const ChordContext&);   // exact modifiers, scope, repeat
}
// cpp/gui_workflow_ui.hpp (overload)
bool WorkflowAction(const char* original,const char* caption,const char* shortcut,design::Action role,Availability);
```

### Task 4.1: Table and readers, no behaviour change

**Files:** create `cpp/gui_commands.hpp`; modify `cpp/gui_header.hpp:45`-`:79`,
`cpp/gui.cpp:1527`-`:1530` and `:1725`-`:1740`, `cpp/gui_workspace.hpp:253`,
`cpp/gui_tool_catalog.hpp:18`-`:31`, `cpp/gui_shell.cpp:19`,
`tests/test_gui_header_contract.py`.

- [x] Checks: the table has one row per `HeaderCommand`, `ShellCommand` and
  `ToolId` (`static_assert` on sizes); every caption and chord shown by the
  header, shell, toolbar and catalogue equals the table's.
- [x] The five places read caption and chord from the table. The header source
  contract reads the table instead of repeating 27 names.
- [x] Each tool window takes its title from its tool definition, which removes
  the five entry-versus-title mismatches (for example "Raw channels" opening
  "Quantitative channels", `cpp/gui_raw.hpp:259`). File ▸ Close
  (`cpp/gui_header.hpp:50`) reads `"Exit"` in the desktop app and stays
  `"Close"` in the embedded viewer.

### Task 4.2: One dispatcher

**Files:** modify the roughly 80 key checks in `cpp/gui.cpp` and the six tool
headers that own a chord.

- [x] Write `every_chord_respects_scope` (review focus 4): for each owning row,
  fire its chord with a dialog open, a text field active, pointer and focus in a
  tool window, then held for ten frames. Dialogs block every row; text permits
  only Global rows; tool windows block Canvas rows; non-repeating commands run once.
- [x] Replace each hand-written predicate with `chord_pressed(id, ctx)`. Replay
  kinds request the gated command; under the controller's ruling, Escape and
  Enter also reach controls as keys, and every key input still positions and pins the pointer.
- [x] Shift, Alt and Ctrl mean the same thing across sibling commands: the
  plain chord runs, Shift adds objects, and cancel has no chord.

The controller's fix-round-2 ruling makes tool/visibility keys NotInText and the seven object/edit Canvas rows available throughout the main workspace; `workspace_focused` replaces `canvas_focused`. Only submitted popup windows own keys. The owner has not reviewed these revised rulings.

Implemented using the plan's stated default, sharpened by the Task 4.2 brief:
one per-frame command context/request cache, exact modifiers, no repeat, and
direct chord owners for presentation aliases. Evidence, cost/corrections and the
unchanged Keep/Recheck question are in the [Task 4.2 report](../build/ui-workflow-safety/m4/t42-report.md).

### Task 4.3: Reasons and Help

Implemented under the replacement assertion rule and final correction rulings
A–D. Availability is shared by widgets, inline reasons and keys, with one cached
answer per task action/frame. Help adds translated task/table context and heading
style. Stop/Cancel and Close kept mask stay conditional; mouse gestures remain.
The [report](../build/ui-workflow-safety/m4/t43-report.md) records results,
cost, exact assertion changes, deviations and the Milestone 4 handoff.

**Files:** modify `cpp/gui_workflow_ui.hpp:45`, `cpp/gui_help.hpp:49`-`:64`, and
the disabled actions in the classifier, raw and fluorescence-family windows.

- [x] Checks: every disabled `WorkflowAction` exposes a non-empty reason in its
  trace; the Help page lists every table row that has a chord.
- [x] Each window gets one `availability(target)` function that returns the
  reason; buttons are always drawn, disabled with the reason as tooltip, and
  the primary action also prints it inline.
- [x] Help's quick reference is generated from the table, grouped by scope.
  Remove handwritten chord prose; retain the five mouse-gesture explanations
  under the explicit task ruling.

**Exit:** a chord, its caption and its enablement cannot disagree; every
disabled action says why; Help is complete by construction.

---

## Milestone 5 — Inputs, drafts and tool scope

Findings 4, 5, 8 and 18.

**Decisions to confirm:** Name and Class commit on Enter or focus loss as one
undo step each; closing the TMA window keeps a ready preview (today's test
asserts it is discarded).

**Interfaces (produces):**

```cpp
// cpp/gui_analysis_inputs.hpp
struct AnalysisInputs {
    enum Field : unsigned {Kind=1,Vectors=2,White=4,Nucleus=8,IntensityStatistics=16,RoiSampling=32};
    int kind=0;std::vector<double> vectors,white;std::optional<std::array<ColorVector,3>> estimated_basis;
    ColorVector estimated_white{};bool nucleus=false,intensity_statistics=false;
    RoiSamplingPolicy roi_sampling=RoiSamplingPolicy::GeometryCoverage;std::uint64_t revision=0;
    void edited(unsigned fields);                       // bumps revision, records changed fields
};
inline constexpr unsigned tissue_inputs=AnalysisInputs::Vectors|AnalysisInputs::White|AnalysisInputs::Nucleus;
inline constexpr unsigned cell_inputs=AnalysisInputs::Vectors|AnalysisInputs::White;
inline constexpr unsigned training_inputs=AnalysisInputs::Vectors|AnalysisInputs::White;
bool draw_analysis_inputs_editor(AnalysisInputs&,unsigned fields); // declared visible fields; inline in Region, Tissue and Cells
// cpp/gui_draft.hpp
template<class T> struct Draft {T value{},baseline{};
    bool dirty() const {return !(value==baseline);}
    void rebase(const T& current) {if(!dirty())value=current;baseline=current;}
    void reset(const T& current) {value=baseline=current;}};
// cpp/gui_tool_scope.hpp
struct ToolScope {std::uint64_t image_id=0;std::string source_generation;ObjectPlane plane{};std::uint64_t document_revision=0;};
struct ScopeChange {bool image=false,source=false,plane=false,document=false;};
ScopeChange update(ToolScope&,const Document&,std::uint64_t image_id);
```

### Task 5.1: Shared inputs with declared consumers

**Files:** create `cpp/gui_analysis_inputs.hpp`; modify
`cpp/gui_analysis_tasks.hpp` (or `cpp/gui.cpp:2508`-`:2623` if milestone 3 has
not run), `cpp/gui_pixel_classifier.hpp:123`.

- [x] Checks (review focus 1): toggling "Intensity statistics" while a cell
  detection runs leaves `result.cell_state=="running"`; changing the white
  reference cancels it and posts `"Cancelled: cell detection (analysis inputs
  changed)."`; Tissue and Cells edit inputs without leaving their task.
- [x] Move the eight input locals into `AnalysisInputs`. An edit cancels only
  consumers whose mask intersects the changed fields, and says so.
- [x] "Edit inputs" opens `draw_analysis_inputs_editor` in place.

### Task 5.2: Batch shows the recipe it runs

**Files:** modify `cpp/gui_batch.hpp:170`-`:325`.

- [x] Checks: with "Use image analysis settings" on, the displayed threshold
  equals the Workflow value and equals the value in the run report; an ROI
  batch carries `intensity_statistics` from the inputs; "Stable pyramid
  snapshot" ticked in Tissue does not fail an ROI or Cells batch.
- [x] Add `ProjectBatchOptions GuiBatch::effective(const WorkflowSnapshot&) const`
  and use it for both the read-only summary and Run. Clear
  `tissue.canonical_pyramid` for non-tissue kinds there. Show the pinned pixel
  model by name with Unpin.

### Task 5.3: Drafts

**Files:** create `cpp/gui_draft.hpp`; modify the Annotations editor
(`cpp/gui.cpp:2356`-`:2432`) and `cpp/gui_tma.hpp:419`-`:450`.

- [x] Checks (review focus 3): type a name and press Enter, the object is
  renamed with one undo step; a dirty geometry draft survives an unrelated
  document edit; selecting another object with a dirty geometry draft shows
  `"Unapplied geometry changes were discarded."`; in TMA a typed case ID
  survives ticking Missing; each Apply is disabled while its draft is clean.
- [x] Name and Class commit immediately. Geometry, colour and TMA drafts become
  `Draft<T>` and use `rebase` on document changes.
- [x] Replace the typed `"Parent ID"` field (`cpp/gui.cpp:2390`) with a combo of
  area annotations shown as `"<name> (#<id>)"` plus `"No parent"`; it still
  writes the same `draft_parent` value.

### Task 5.4: Tool scope

**Files:** create `cpp/gui_tool_scope.hpp`; modify
`cpp/gui_fluorescence.cpp:39`, `cpp/gui_dark_flat.cpp:39`,
`cpp/gui_linear_unmixing.cpp:35`, `cpp/gui_registration.cpp:35`,
`cpp/gui_genes.hpp` (refresh), `cpp/gui_tma.hpp:291`,
`cpp/tests/gui_simplification_tools_checks.hpp:178`.

- [x] Checks: a TIFF view change (new server, same source generation) keeps an
  un-kept fluorescence run and a loaded correction; a plane change hides the
  preview but keeps the run; switching image clears both; hiding the TMA
  window keeps its preview and "Discard preview" is the only discard path.
- [x] Each tool keeps a `ToolScope` and clears state only on `image` or
  `source` changes. The TMA expectation at `:178` is rewritten to the new
  behaviour with its other assertions intact.
- [x] Fluorescence Recheck runs from a private request and leaves the form and
  the unmixing draft alone; add `"Load settings from this result"` as a
  separate action.

**Exit:** editing inputs never cancels unrelated work silently; a batch runs
what its window shows; no draft or tool state is dropped by a display change.

---

## Milestone 6 — Feedback and enforcement

Findings 9, 11, 16 and 17.

**Decisions to confirm:** translation scope. The default declares the shell,
menus, dialogs, the four primary panes and the Workflow tasks in scope and
leaves specialist tool windows in English.

**Interfaces (produces):**

```cpp
// cpp/gui_notifications.hpp
enum class Severity {Info,Success,Warning,Error};
struct Notice {Severity severity;std::string source,message,path;};
class Notifications {
public:
    void post(Notice);void draw_status_bar();void draw_activity(bool* open);void dismiss_errors();
};
// cpp/gui_units.hpp
namespace qpx::ui::unit {inline constexpr const char* um="µm";inline constexpr const char* um2="µm²";
    inline constexpr const char* um_per_px="µm/px";inline constexpr const char* mm2="mm²";}
// cpp/gui_layout.hpp
class ComboScope {public:ComboScope(const char* label,const char* preview,ImGuiComboFlags=0);~ComboScope();explicit operator bool() const;};
// cpp/gui_i18n.hpp
struct MissTrace {std::set<std::string> keys;};inline thread_local MissTrace* miss_trace=nullptr;
```

### Task 6.1: Notifications

- [x] Checks: an error from an Annotations action appears in the status bar in
  the error colour with its source, and stays until dismissed or replaced by a
  newer error; a success shows the path; the Activity list keeps the last 50.
- [x] Route the shared `error` string (`cpp/gui.cpp:446`, drawn at `:3120` and
  `:3441`) and the per-operation error strings through `Notifications`, keeping
  inline errors beside their actions.

### Task 6.2: Units and vocabulary lint

- [x] Add a native check in `cpp/tests/gui_i18n_tests.cpp` over the catalogue
  keys and GUI literals: no `Saved (cells|mask|run|record|results|components)`
  for in-memory records, no `Publish`, `Retain` or `Reverif`, no chord suffix
  in a visible caption, no bare `um`. An allowlist names the on-disk cases.
- [x] Replace the unit spellings with `ui::unit` constants and update the
  affected catalogue keys.

### Task 6.3: Contract enforcement that fails for the right reason

**Files:** modify `tests/test_gui_design_contract.py`,
`cpp/tests/gui_design_tests.cpp`, `cpp/gui_layout.hpp`; create
`tests/fixtures/gui-raw-imgui-allowlist.json`.

- [x] Move token-value checks to `static_assert` in `gui_design_tests.cpp` and
  delete the substring assertions that pin source formatting.
- [x] Widen the bypass guard to any first argument and to `Combo`, `Input*`,
  `TreeNode`, `Selectable`, `BeginTabItem`, `TableSetupColumn`, `Text*` and
  `SetTooltip`, enforced as a per-file ratchet: counts in the allowlist may
  fall, never rise.
- [x] Convert `ui::BeginCombo`/`ui::EndCombo` call sites to `ui::ComboScope`,
  and user-supplied labels (for example `cpp/gui_project_density.hpp:78`) to a
  helper that appends an explicit `###id`.

### Task 6.4: Translation coverage is measured

- [x] Localized GL runs set `i18n::miss_trace` around named surfaces. The exact
  baseline ratchet rejects new misses and requires lowering stale entries;
  both negative probes pass with their required diagnostics (review focus 5).
  It records 160 lookup keys / 203 surface occurrences: 158 untranslated captions
  and two already-translated calibration labels looked up a second time.
- Not selected: translate missing in-scope captions. They were measured only;
  the earlier regex estimate is superseded by the executed surface inventory.
- [x] State the plan's stated default translation scope in
  [design-system.md](design-system.md). The owner has not confirmed it individually.

**Exit:** errors are attributed and persistent; one unit spelling; a contract
test fails only when a user-visible rule is broken.

---

## Milestone N — Optional native follow-ups

Each item changes the core library and therefore the binding objects. It needs
its own owner selection, affected native regressions and binding/stub
consistency under the
[validation policy](api-delivery-plan.md#validation-and-build-policy), and a new
candidate identity. None is required by milestones 1 to 6.

| Item | Change | Unlocks |
| --- | --- | --- |
| N1 | Keep and remove of kept records become undoable document edits | the confirmation in task 1.3 can give way to Undo |
| N2 | A native lookup for an equivalent kept request | Keep refuses an exact duplicate across sessions |
| N3 | A tile sink in `cpp/fluorescence_cell_workflow.cpp:231` and a reader for tiled outputs | tile progress; Add and Keep after a tiled run |
| N4 | A persistent origin marker on classifier-created annotations | excluding them from default training after reopen |
| N5 | Pixel and object classifier requests match on captured object revisions and source generation | Keep no longer retires a live preview |
| N6 | Typed status enums in native headers, strings only at the binding boundary | removes the GUI parse layer |
| N7 | A base-directory argument for `detail::export_path` | CLI and GUI share one relative-path rule |

Before N4 is considered, confirm the suspected behaviour: train, Add
annotations, train again with the default scope, and inspect the training
request's label IDs.

---

## Validation and handoff

Apply once per selected milestone.

1. Before editing, record the retained native archive hashes, the 38 binding
   object hashes and the GUI source inputs under
   `build/ui-workflow-safety/<milestone>/`.
2. Settle captions, IDs, helpers, callers, catalogue keys and tests, then run
   one combined cached frontend build: compile only the GUI units whose inputs
   changed and link the app and affected test consumers against the retained
   `qupathxx_core.lib`, asserting its SHA-256 is unchanged. Follow the method of
   `build/ui-workflow-simplification-merge/build_frontend.py`.
3. Run the source contracts without syncing the project (a plain
   `uv run pytest` would build the Python package and the native core):
   `uv run --no-project --with pytest pytest tests/test_gui_design_contract.py tests/test_gui_dialog_contract.py tests/test_gui_export_contract.py tests/test_gui_header_contract.py tests/test_gui_image_inspector_contract.py -q`.
4. Run the affected native suites: always `gui_organization` (headless, then
   `--workflow-safety` and the flags whose code changed), `gui_replay`,
   `gui_i18n` and `gui_design`; add `gui_fluorescence`, `gui_marker_rules`,
   `gui_registration`, `gui_zarr`, `gui_dark_flat`, `gui_linear_unmixing`,
   `gui_spatial_import`, `gui_gene`, `gui_normalization` or
   `gui_target_membership` when their sources changed.
5. Walk the changed flow once on a copied complex project
   (`--simplification-complex` fixture): Draw, Run, Keep, Recheck, Remove,
   Export, Save, reopen. Capture only changed controls at normal size and
   French 200%.
6. Record results in `build/ui-workflow-safety/<milestone>/results.json`, add a
   receipt under `docs/benchmarks/ui-workflow-safety/README.md` with a
   finding-to-evidence table, and update the [current card](current-task.md).
   Stop there.

No kernel or binding rebuild, science requalification, wheel or publication
belongs to milestones 1 to 6. A check that fails needs a diagnosed cause before
it is rerun.

---

## Appendix A — Findings register

Anchors are `file:line` on `main` at `7c981bf9`. Severity and confidence are
from the review.

| # | Finding | Sev. | Anchors | Milestone |
| --- | --- | --- | --- | --- |
| 1 | No file chooser in the core flow; typed paths resolve against the launch directory; one image per Add | high | `cpp/gui.cpp:1877`, `:1959`, `:3019`; `cpp/atomic_export.hpp:29`; `cpp/gui_spatial_import.hpp:25`, `:157` | 2 |
| 2 | Three overwrite idioms: the registered dialog, six sticky checkboxes, and create-only failures after the work | medium-high | `docs/ui-dialog-contract.md:89`; `cpp/gui_pixel_classifier.hpp:215`; `cpp/gui_raw_pixel.hpp:153`; `cpp/gui_tma.hpp:188`; `cpp/atomic_export.hpp:80`; `cpp/gui.cpp:2456`, `:3028` | 2 |
| 3 | Kept results: Recheck invites a duplicate Keep, ROI and cell runs cannot be removed, other removals are one unconfirmed irreversible click | high | `cpp/gui.cpp:2723`, `:2690`, `:2864`; `cpp/analysis_records.cpp:71`; `cpp/include/qupathxx/core.hpp:330`, `:350`; `cpp/tissue_records.cpp:91`; `cpp/gui_spatial.hpp:156`; `cpp/spatial_analysis.cpp:196`; `cpp/gui_raw.hpp:310`, `:330`; `docs/pixel-classifier-ui.md:222` | 1, 3, N |
| 4 | Hidden shared state: input edits cancel other jobs, Recheck overwrites inputs, Batch runs values it does not show, previewing a kept mask replaces the model | high | `cpp/gui.cpp:2623`, `:2724`, `:2858`, `:2977`; `cpp/gui_fluorescence.cpp:67`; `cpp/gui_batch.hpp:176`, `:312`; `cpp/project_batch.cpp:253`; `cpp/gui_pixel_classifier.hpp:283` | 1, 5 |
| 5 | Implicit resets lose drafts, landmark pairs, polygon drafts and finished previews | medium-high | `cpp/gui.cpp:2363`, `:2378`, `:3138`; `cpp/gui_tma.hpp:419`, `:291`; `cpp/gui_registration.cpp:35`, `:38`; `cpp/gui_fluorescence.cpp:39`; `cpp/gui_pixel_classifier.hpp:213`; `cpp/gui_pixel_stream_browser.hpp:76` | 1, 5 |
| 6 | Keyboard: bare keys mutate, F8 re-adds a remembered path, a chord starts a batch, guards differ per site, Help lists 14 of about 75 chords | medium-high | `cpp/gui.cpp:2346`, `:2329`, `:1495`, `:1925`, `:1930`, `:2088`, `:1695`, `:1688`, `:2662`, `:2945`; `cpp/gui_batch.hpp:143`; `cpp/gui_raw.hpp:255`; `cpp/gui_tma.hpp:289`; `cpp/gui_help.hpp:56` | 1, 4 |
| 7 | Dead ends: tiled fluorescence results, batch results, Raw channels on OME-Zarr, Run greyed with the cause hidden, Density re-check | medium-high | `cpp/gui_fluorescence.cpp:80`, `:125`, `:141`, `:146`, `:154`; `cpp/gui_batch.hpp:273`, `:277`; `cpp/gui.cpp:1343`; `cpp/gui_raw.hpp:60`; `cpp/gui_project_density.hpp:119` | 1, N |
| 8 | Implicit scope and target: TMA writes, Tables scope and counts, cross-task Export, interleaved Import/export options, Parent ID | medium | `cpp/gui_tma.hpp:440`, `:444`, `:426`, `:467`; `cpp/gui.cpp:3016`, `:3013`, `:2984`, `:1458`, `:2470`, `:2390` | 1, 3, 5 |
| 9 | Status, errors and disabled states do not explain themselves | medium | `cpp/gui.cpp:2963`, `:3062`, `:3120`, `:3441`, `:2684`, `:2706`; `cpp/gui_raw_pixel.hpp:123`; `cpp/gui_tma.hpp:387`; `cpp/gui_batch.hpp:285`; `cpp/gui_workflow_ui.hpp:50` | 1, 3, 4, 6 |
| 10 | Labels and overlays that can mislead interpretation | medium | `cpp/gui_marker_rules.cpp:77`; `cpp/gui_raw_pixel.hpp:65`; `cpp/include/qupathxx/core.hpp:101`; `cpp/gui.cpp:3398`, `:3485`; `cpp/gui_pixel_classifier.hpp:132`; `cpp/gui_fluorescence.cpp:73` | 1, N |
| 11 | Vocabulary half-migrated: Saved for kept, Published for Added, "Measurements" four ways, tool names that differ from window titles, unit spellings | medium | `cpp/gui.cpp:2980`, `:2733`, `:3016`, `:1618`, `:390`; `cpp/gui_fluorescence.cpp:149`; `cpp/gui_pixel_classifier.hpp:255`; `cpp/gui_tool_catalog.hpp:30`; `cpp/gui_raw.hpp:259`; `cpp/gui_header.hpp:50`; `cpp/gui_measurements.hpp:17` | 3, 6 |
| 12 | One caption string is ID, translation key, test identity and shortcut; 149 controls are public enum values | high | `cpp/gui.cpp:2649`, `:2647`, `:1527`, `:1530`, `:1740`; `cpp/gui_workflow_ui.hpp:41`, `:60`; `cpp/gui_layout.hpp:349`, `:209`; `cpp/tests/gui_simplification_checks.hpp:310`; `cpp/include/qupathxx/core.hpp:872` | 4 |
| 13 | No command model: one command is declared in up to seven places | high | `cpp/gui_header.hpp:53`; `cpp/gui.cpp:1528`, `:1734`, `:1695`, `:1549`; `cpp/gui_tool_catalog.hpp:27` | 4 |
| 14 | The result lifecycle is re-implemented thirteen times, with drift | high | `cpp/gui.cpp:2710`, `:2963`, `:2864`; `cpp/gui_raw_pixel.hpp:161` | 3 |
| 15 | Status is a string protocol across native code, UI and tests | medium | `cpp/include/qupathxx/core.hpp:327`; `cpp/gui.cpp:2710`; `cpp/tests/gui_simplification_checks.hpp:311` | 3, N |
| 16 | Contract tests are source greps: brittle to formatting, blind to real bypasses; translation falls back silently | medium | `tests/test_gui_design_contract.py:35`, `:52`; `tests/test_gui_image_inspector_contract.py:6`; `cpp/gui_batch.hpp:40`; `cpp/gui_class_editor.hpp:55`; `cpp/gui_project_density.hpp:78`; `cpp/gui_layout.hpp:149`; `cpp/gui_spatial_import.hpp:160`; `cpp/gui_registration.cpp:108`; `cpp/gui_genes.hpp:57`, `:213`; `cpp/gui_i18n.hpp:132` | 1, 6 |
| 17 | `run_gui` is one function of about 3,600 lines holding all workbench state | medium | `cpp/gui.cpp:196` to `:3822` | 2, 3, 5 |
| 18 | Tool state is keyed on pointers and the global document revision | medium | `cpp/gui_fluorescence.cpp:39`; `cpp/gui_registration.cpp:35`; `cpp/gui_pixel_classifier.hpp:137`; `cpp/gui_object_classifier.hpp:101` | 5, N |

Smaller items folded into the tasks above: right-drag-only pan and tool windows
that open over the slide are left for the separately selectable phase 4 of the
workflow simplification plan; the eight-row object list cap
(`cpp/gui_layout.hpp:263`), the "Missing files" ghost button
(`cpp/gui_project_browser.hpp:81`), plural handling and the import wizard's
late validation are not scheduled here.
