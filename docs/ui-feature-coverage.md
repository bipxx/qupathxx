# UI feature coverage and simplification

Scope: the existing qupathxx desktop and compact API UI. This is a navigation
and preservation review, not a QuPath parity promise or new scientific phase.
Source inventory covers primary panes, native tool controllers, nested windows,
registered modals, menus, shortcuts and conditional capabilities. A source trace
does not claim that every scientific recipe was rerun during this UI change.

## Findings and fixes

| Finding | Evidence from the reviewed UI | Impact and correction | Validation |
| --- | --- | --- | --- |
| Hidden, duplicated navigation and requirements (design/usability smell; medium; high confidence) | Review baseline `gui.cpp:2964`: `BeginCombo("##Tool launcher"`; `gui.cpp:1595`: `BeginMenu("Analyze")`; their `"Gene expression..."` image guards differed | Discovery needed another popup; Project density and Spatial were absent from Analyze. Both now use the same 12-tool inventory and complete controller bindings. Grouped rows are visible, requirements are explained, and model/file browsers remain accessible without an image. | Retained-route/context matrix, missing/duplicate-handler refusal, filtered/empty/translated catalogue and real tool opening |
| Legacy tool frames bypass the shared style (design smell; medium; high confidence) | Review baseline `gui_tma.hpp:261`: `ImGui::GetFontSize()/13.f`; independent placement/constraints also occur in object/pixel/raw classifiers and quantitative channels | Five tool frames now use the shared viewport sizing and localization helper. Content fit applies at first open; later manual/saved/docked dimensions remain user-owned. | First-fit, explicit resize and fresh-session restore checks; affected tool and raw views |
| Fixed empty result/list space (localized design smell; low; high confidence) | Review baseline `gui_tma.hpp:389`: `"core catalog",{0,140.f*scale}`; `gui_project_density.hpp:73`: `"Density images",{0,110}`; `gui_raw.hpp:173`: `{0,150.f*scale}` | TMA preview/catalogue, density image/preview and raw channel tables use the existing content-sized, bounded collection rule. Data, controls, clipping limits and native operations are retained. | Short/long collection bounds, affected TMA/raw views and exact native-data invariants |

The independent code lane found duplicated launch callbacks; the design lane
traced their drifting entry points; the UX lane checked visible discovery,
disabled reasons and exit paths. These findings are usability/maintenance issues.
No deceptive-choice mechanism was found in scope. A broad rewrite of `gui.cpp`
was investigated but deferred: it is unnecessary for these concrete fixes.

## Closed review findings and docking problems

The owner selected implementation after the read-only second review. These are
usability and lifecycle defects; no scientific result defect was demonstrated.
The code quotes below describe the reviewed baseline, not the corrected source.

| Problem | Baseline evidence | Implemented contract and decisive check |
| --- | --- | --- |
| Primary panes could not close | `Begin(localized.c_str(),nullptr,...)` in `GuiWorkspace::begin` | Docked headers have Close; floating frames have native X; Hide is in the placement menu. All four close/reopen through actual native input; hidden visibility survives restart. |
| Active activity buttons only focused | `select(panes[i])` in the rail handler | Clicking the active left pane hides the left group and reclaims viewer space. A second click reveals it. View/task commands always reveal; right/floating panes are unaffected. Native Rml input and layout checks cover these routes. |
| Image could dock on the right | `DockBuilderDockWindow(image_pane,top)` in `default_right` | Image is left/floating; Workflow has the default right dock. Menu commands, native right drops and old layout restoration share enforcement. Valid custom stacks remain. |
| Floating inspectors repeated the title and wasted height | `title("Image")` plus native title; `WorkSize.y*.7f` floating size | One native floating title, border, close/resize controls and initial content fit. Docked panes retain a draggable custom header; later manual/saved size remains owned by the user. |
| Named child editors were undiscoverable | `Dark` returned `No matching tools.` although `Dark/flat correction...` existed | Full English/localized names for Marker rules, Dark/flat, Linear unmixing and Saved stream files appear with the parent caption and open the existing child controller. The 12 parent routes and prerequisites remain. |
| QPTMA export interrupted core editing | `I acknowledge the omitted metadata` and `Export QuPath grid` preceded `TMA views` | QPTMA and CSV live under Export. Replay intent reveals the same tab without changing target IDs. Core edits/undo, exact native CSV/QPTMA and refusal without acknowledgment pass. |
| Completed exports showed inactive import actions | `Apply grid preview` and `Discard grid preview` rendered after export | Preview actions appear with a preview; Cancel appears during an operation. Status/errors and native import/export jobs remain. |

Evidence: `build/ui-docking-lifecycle`. Existing numerical/scientific receipts
remain scoped to their original source; this review does not claim a new science
qualification. Desktop automation was unavailable after documented recovery;
native GL input replays and their captured frames establish rendered behavior.

## Comparison with VS Code: remaining usability priorities

Reviewed 2026-11-10 against Microsoft's current [workbench anatomy](https://code.visualstudio.com/docs/editing/getting-started/userinterface), [custom layout](https://code.visualstudio.com/docs/configure/custom-layout), [sidebar guidelines](https://code.visualstudio.com/api/ux-guidelines/sidebars) and [view guidelines](https://code.visualstudio.com/api/ux-guidelines/views). Visually compared the official workbench image with the last qualified qupathxx native captures (`final-lifecycle-checks/lifecycle-reopen.png`, `lifecycle-floating.png` and TMA views). Baseline runtime hash: `cb1c996f147eeaa08f9cc052308e76c40498f18c7607ec5444318ed1675df9d6`; evidence is under `build/ui-docking-lifecycle/vscode-review`. Header/Help implementation is proceeding separately and is not judged complete or absent from these older captures.

The main gap is workbench navigation. The shared activity dimensions/selection
indicator, image browser with selected-entry metadata, viewport-local toolbar,
primary close/reopen, persisted visibility and Image placement policy are useful
foundations. VS Code's state/container conventions provide the next practical
reference. Small cosmetic differences do not justify another visual campaign.

All five findings below are usability/design smells with **high confidence**;
none demonstrates a scientific correctness or persistence failure. Priority is
relative within this UI scope. Evidence lines describe the reviewed snapshot.

| Order / severity | Finding and exact evidence | User impact / smallest improvement | Decisive validation |
| --- | --- | --- | --- |
| 1 / medium | **Primary activity selection and focused pane share state.** `gui_workspace.hpp:174`: `active_=i;shown_[i]=true;focus_=i;`; `gui_shell.cpp:145`: `selected=i<4?snapshot.pane==i:`. Floating Image capture shows Project on the left while Image owns the rail marker. | Focusing a right/floating pane changes the left rail's selection even though left content stays put. Separate primary-sidebar selection from focused pane and secondary-sidebar selection. Retain Image left/floating. | Open Project left plus Workflow right; focus right/floating content and confirm the left marker still identifies the left container. Clicking its active activity hides/reveals the left group without affecting the right. |
| 2 / medium | **Workflow mixes object properties, scientific operations, results, tool discovery and source management behind one chooser.** `gui_workflow.hpp:7`: `"Measurements","Region analysis","Threshold tissue mask","Watershed cells","Tables","Tools","Source and files"`; `gui.cpp:1793`: `BeginCombo("##Task"`. | Users must remember hidden locations and distinguish Measurements from Tables. First clarify those labels as Object measurements and Result tables. Then group related analysis operations under an explicit Analysis area; give results/jobs stable visible destinations and keep Tools/source access in clear named routes. Keep every existing controller, option and result. | Trace select annotation → configure/run → review/keep → table/export and running/cancel/error paths; destinations remain visible and the 12 parent/4 child routes and current native controls remain available. |
| 3 / medium | **The viewer toolbar rearranges into multiple rows.** `assets/ui/workspace.rcss:10`: `.toolbar { position: absolute; display: flex; flex-wrap: wrap; align-content: flex-start;`. The 1100px reopen capture shows two rows above the 440px-wide viewer. | Common controls shift vertically and reduce viewing height. Keep selection/drawing/undo in stable groups; place lower-frequency commands in a named, keyboard-accessible overflow menu when width is insufficient. Keep the toolbar above the slide. | At narrow, normal and 200% scale, every command is reachable, groups/order stay predictable, tooltips/shortcuts work, and the viewer/overlay geometry remains correct. |
| 4 / low | **Floating inspectors retain an almost empty command row.** `gui_workspace.hpp:193`: `ui::ActionRow row(extra_actions+1+(docked?1:0));`; line197: `ImGui::InvisibleButton("##Drag panel",{row.field_width(),height});`. Floating Image has one layout button between its native title and image name. | The duplicate heading is fixed, but this row still consumes useful space. Move placement commands to the title/context menu; retain an in-body command row only for actual content actions. Preserve the native X/resize and accessible placement commands. | Floating Image has one title and starts its content directly beneath it. Close, keyboard/context placement, docking, saved geometry and any Project Add/Save actions remain usable. |
| 5 / low | **Sidebar visibility lacks shared global toggle commands.** `gui_workspace.hpp:244` routes Ctrl+1–4 to `select`; `gui.cpp:1630`: `if(ui::MenuItem("Project")) focus_pane(GuiWorkspace::project_pane);`. | Mouse rail toggling works, but keyboard/View routes only reveal named panes. Add shared Toggle primary sidebar / Toggle secondary sidebar actions, with menu, shortcut and optional layout controls using the same dispatcher. Preserve Ctrl+Alt+B for Batch rather than copying VS Code's binding blindly. Coordinate entry placement with the in-progress header work. | Verify mouse/menu/keyboard use the same visibility state, respect modal ownership, restore each side independently and preserve running jobs and user data. |

Code lane: traced pane selection, rendering and command dispatch. Design lane:
checked primary/secondary ownership and seven-task grouping. UX lane: inspected
normal/float/narrow captures, exit/reopen, command density and keyboard routes.
These notes were reconciled into the five findings; no delegated review or new
native build was needed. Existing 37 UI scenarios/17 source contracts retain
only their original passing scope and do not qualify these proposed changes.

Optional capability difference: qupathxx's current floats remain inside the
application (`gui.cpp:309` enables docking, not multi-viewport windows). VS Code
supports detached windows and multi-monitor use. Treat that as a separately
selected feature if researchers need it, not a defect requiring immediate work.

This is a read-only review of application behavior: no app source/resource,
project, runtime or layout change. No commit/merge. Proposed fixes require their
own scoped implementation and checks; no feature or parity phase is selected.

## Detail audit: buttons, repeated sections and wording

Milestone 1 is delivered, 2026-11-11: all twelve findings below have verified
dispositions in the [implementation receipt](benchmarks/ui-workflow-simplification/README.md).
The table preserves baseline evidence and the selected corrections.
Implementation sequencing and acceptance checks are in the
[workflow simplification plan](ui-workflow-simplification-plan.md).

Owner-selected audit, 2026-11-10. Scope is the workbench's Project/Image/
Annotations/Workflow content and existing classifier, density and TMA tools.
Checked six rendered native views (Region, Cells, Tables, Annotations, Pixel
classifier and TMA setup), current handlers and native semantics where wording
could change interpretation. The captures retain their last qualified runtime
identity; concurrent header/Help work is outside this audit. Source quotations
and line numbers are frozen in `build/ui-docking-lifecycle/detail-audit/evidence.json`.
No application changes or additional native build were made by this pass.

These are **12 high-confidence usability/design findings**, grouped by the
smallest useful correction. Medium findings should precede low-priority density
polish. This baseline review predates implementation; the linked receipt owns
the current verification status and residual scope.

| Order / severity | Finding and baseline evidence | Concrete improvement | Check when implemented |
| --- | --- | --- | --- |
| 1 / medium | **The same X means cancel work and hide a panel.** `gui_raw_pixel.hpp:94`: `button("Cancel operation",...ui::Icon::Close)` and `button("Hide panel",...ui::Icon::Close)` are adjacent; their handlers call `jobs_.cancel()` and set `options_.open=false`. Pixel classifier also has an in-body Cancel X below the native window X. | Use Stop/Cancel only for running work; native X closes/hides. Remove duplicate in-body Hide presentation only after routing its existing replay target to the retained close action. Keep tooltip/accessible names explicit. | Cancel and Hide have distinct hit targets/icons; hiding does not cancel work, cancellation does not close, public replay targets remain usable. |
| 2 / medium | **Result navigation appears before there is a result to show.** `gui.cpp:2676`: `ui::Button("Go to tables###ROI tables")` is unconditional; the following Keep action requires `analysis.result()`. Region capture shows table navigation but no Keep button/result. Tables exports consume kept results. | Group result values, Keep and table navigation together. Use **View kept results**; show/enable it when the relevant family has kept rows. For an unkept preview show **Keep this result to include it in tables.** Keep the full Tables destination reachable elsewhere. | Run → Keep → View results reaches the expected family/current image; viewing older kept results is explicit, with no unintended save or discarded preview. |
| 3 / medium | **Two Apply buttons commit different drafts.** `gui.cpp:2387`: `PrimaryButton("Apply object edit")`; line2411: `Button("Apply###Apply object display state")`. The latter also edits reported class probability, which is metadata. | Label the first **Apply object changes** and the second **Apply color and probability**, or split the latter's visual and reported-probability fields into accurately named groups. Do not call the whole operation appearance-only. Disable each until its own draft changes. | Name/class/geometry and color/reported probability apply independently, retain precision and native Undo, and do not commit each other's drafts. |
| 4 / medium | **Object creation and retained results use overlapping verbs.** `gui.cpp:2808`: `Add annotations`; `gui_pixel_classifier.hpp:220`: `Publish class annotations`; `gui_fluorescence.cpp:140`: `Keep fluorescence cells` publishes cell objects, while `Keep fluorescence run` retains a result. | Standardize actual effects: **Add annotations**, **Add cells**, **Keep result** / **Keep training data**, **Save project**, **Export CSV**. Retain separate Add/Keep actions; they are different native operations. Show a brief explanation only at the relevant result step. | Correct native object creation versus archival calls, exact result IDs/rows, persistence, publish guards and Undo remain unchanged. |
| 5 / medium | **Region has overlapping input sections before the main action.** `gui.cpp:2479`: `Image analysis settings`; line2503: `Active inputs`; line2559: `Analysis inputs`. The normal capture repeats profile/current/input terminology before Run. | Use one **Analysis inputs** area: current measurement/white reference first; clearly distinguish **Saved profile: H&E (not applied)** with an explicit **Use profile** action. Keep advanced model files/estimation available through their existing controls or one named advanced route. Do not reintroduce many folded sections. | Saved versus active values are unmistakable; applying a profile remains explicit; OD and supplied-stain modes, calibration and existing editors remain accurate/reachable. |
| 6 / medium | **Preview and kept results have inconsistent labels; Saved can imply disk persistence.** `gui.cpp:2690`: `Saved analysis`; line2695: `Saved channel %d`. Current means above it use `Mean OD R` or `Mean stain 1`. `latest_analysis` may be retained in an unsaved project. | Use **Preview result** and **Kept result**, with each result's own domain-aware channel labels. Keep both when they differ; expose result identity/provenance and recheck state. Explain disk persistence through the existing Save project action/status. | Change active settings after keeping a result: stored values keep their own kind/IDs/calibration labels and remain visually distinguishable from a new preview. |
| 7 / low | **Shortcuts and Undo explanations bloat action captions.** `gui.cpp:2636`: `Analyze region (Ctrl+M)`; `gui_object_classifier.hpp:172`: `Apply predictions (one Undo)`; table export captions include long chords. | Use concise visible verbs: **Run analysis**, **Apply predictions**, **Export CSV**. Put shortcut hints in a consistent trailing hint/tooltip; show **Undo available** after the operation. Keep target IDs and keyboard handlers stable. | Narrow/French/200% labels fit; keyboard hints and accessible names remain available; all shortcuts still route to the same actions. |
| 8 / low | **Export repeats its prompt and re-encodes selected scope in the button.** Tables shows `Export destination`, a `.csv destination` hint and `Choose an export destination.`; the CTA switches among Export image/project cells. | One field label **CSV file**, an example/path hint, and stable **Export CSV**. Scope and table type remain explicit selectors. Show one actionable reason only when an action is unavailable. Preserve currentness and archived-measurement explanations. | Current image/project, cell/live/kept and ROI/tissue variants retain exact native requests; validation errors are visible and output protection remains unchanged. |
| 9 / medium | **Routine density instructions expose implementation terminology.** `gui_project_density.hpp:93`: `Preview cohorts and calibration`; line124: `Cancellation requested. Native geometry/open/save must reach a cancellation point.`; line135 starts `Native default analysis quotas apply.` | **Check inputs** with supporting **Checks image classes and calibration**. Cancellation: **Stopping after the current operation. Completed results are kept.** Use **Previous results folder (optional)**. Put quota/receipt/engine-identity detail in accessible Details; keep save-first, new-folder and actual error/reuse conditions visible. | No promise of immediate cancellation; exact reuse rules, completed output retention, invalid class/calibration feedback and native request inputs stay unchanged. |
| 10 / medium | **TMA labels repeat context and obscure a threshold.** `gui_tma.hpp:279`: `Detected grid name`; repeated Detected rows/columns and Core width/height wrap. Line290: `Missing tissue fraction`; native `tma_dearray.cpp:322` tests `core.tissue_density<o.missing_density`. | **Grid name**, **Rows**, **Columns**, **Width (px)** / **Height (px)** and **Minimum tissue coverage**, with **Cores below this value are marked missing.** Display ordinary values without forced six-zero tails, retaining full editable precision and units. | Below/equal/above-threshold behavior remains native-identical; px/um changes preserve calibrated geometry and exact stored inputs; long locales stay readable. |
| 11 / low | **Classifier stages have weak action hierarchy.** `gui_pixel_classifier.hpp:180` renders non-icon actions through `ui::Button(label)`; the capture gives Train, Predict, Trace, Publish and Keep the same treatment. | Clearly identify **Train → Predict → Add objects** stages. Give the currently available main stage one primary action; keep file/advanced actions secondary and unavailable stages discoverable with their prerequisite. Do not automatically run the next stage. | Model-only and train-first paths, cancellation, contour settings, Keep/Publish separation and all existing controller routes remain. |
| 12 / low | **Measurements repeat labels over several lines.** `gui_measurements.hpp:9,12` renders `Area` once in pixels and again in micrometers, similarly for Length and nucleus values; Annotations capture shows four lines for two quantities. | Compact rows such as **Area: 330 µm² (2640 px²)** and **Length: 74 µm (208 px)**, or aligned unit columns. Keep pixels available and calibration-unknown explicit. Do not rename every Length to Perimeter: the selected geometry may be open. | Exact native values, calibration units, nucleus distinction and literal imported metadata remain readable, including missing calibration and enlarged fonts. |

### Copy and action rules for the next implementation

- One primary action per active stage. Text verbs for scientific actions; icons
  for familiar utilities such as Close, Info, Open, Save and Refresh.
- Use one vocabulary by effect: Add creates objects, Keep retains a result,
  Save project writes the project, Export creates an external file. Never
  merge these operations merely to remove buttons.
- Section titles establish context; field/button labels need not repeat it.
  Show help once at the relevant step, not above every action or while its
  referenced button is absent.
- Preserve mode/channel names, physical units, reported-probability meaning,
  exact result identity, stale/currentness state, validation errors and loss
  acknowledgments. For QPTMA, present actual omissions as a short list using the
  existing native omission report; keep the acknowledgment before export.
- Move technical background into reachable Details, while keeping any condition
  that changes the user's decision beside its control. Preserve literal names,
  scientific inputs, native validation and persisted values.

Not reported as defects: blank space below a short list; needed distinction
between current and retained results; purposeful repeated access to properties
from their context; font/pixel equivalence with VS Code. No scientific control
removal or new native algorithm is selected. Implementation should start with
findings1–6 as a cohesive usability pass and validate affected routes, state,
Undo/data/persistence and locale/scale behavior under the existing build policy.

## Existing feature map

All entries below remain available. Menus and tool rows share one inventory;
existing keyboard handlers, replay target IDs, modal ownership and native action
controllers remain the authorities. The compact API inspector retains a compact
Tools section instead of inserting a long list into its continuous form.

| Family | Retained features | Entry and nested surfaces | Evidence / boundary |
| --- | --- | --- | --- |
| Workspace | Activity rail, viewer toolbar, fit/pan/zoom, undo/redo, object visibility, theme, scale, language, docking/stacking/floating/reset/persistence | View, Language, workspace settings, pane layout icons, Ctrl+1–4 | `gui_workspace.hpp`, `gui_shell.*`, docking/shell/i18n tests; presentation only |
| Projects | New/open/native/QuPath import review, add/readers, save/as, replace/close, recovery/checkpoints | File, Project switcher/settings, registered confirmation dialogs | `gui.cpp`, entry/startup/project/recovery checks; native project services retained |
| Image catalogue | Thumbnails, search, metadata grouping, recent projects, missing-file filter, row actions/relink/remove, rename, metadata/description/tags | Project list, row/context menu, lower metadata inspector | `gui_project_*`, metadata/relink/remove contracts; selection is not image activation |
| Image | Display mode, black/white/gamma/reset, calibration apply/clear/undo, overview, image information/imported properties/analysis profile | Image inspector and registered information/calibration dialogs | `gui_calibration_editor.hpp`, inspector checks; display never changes raw samples |
| Objects | Draw/edit ROI shapes and holes, ordered/multiple selection, visibility/locks, geometry/hierarchy, classes, appearance/probability, historical properties | Annotations and canvas tools; Object information | `gui.cpp`, class/editor/replay checks; native edits and identities unchanged |
| Object exchange | Copy/paste/current plane, GeoJSON import/export, object measurements, scope and compatibility choices | Annotations Import/export, Edit, F6/F7, clipboard shortcuts | Native interchange/export guards, replacement dialog and export workflow checks |
| Measurements | Shape values, calibrated/unknown units, mean RGB and captured cell features | Workflow → Measurements | `gui_measurements.hpp`, sampling and exact native result evidence |
| Region | OD/stains, sampling/intensity options, image profile, model files/estimation, run/cancel/keep/recheck/provenance | Workflow → Region analysis; native shortcuts | Existing analysis/stain controllers and retained-result/export tests |
| Tissue | Feature/threshold/smoothing/grid/area options, preview, contours/add annotations, keep/recheck/provenance | Workflow → Threshold tissue mask | `gui_tissue.hpp`, tissue/contour/archive checks; native kernels unchanged |
| Cells | Detector/features/grid/area options, nuclei/body preview, publish/keep/recheck/export | Workflow → Watershed cells | Cell workflow checks; selected latest saved run remains distinct from live/project exports |
| Tables | Cells/ROI/tissue, image/project scope, saved/live cell source, current-result requirements, destinations/format/limits, progress/cancel/errors/replacement | Workflow → Tables and result links/shortcuts | `gui_export.hpp`, exact two-kept-run export oracle and protected destinations |
| Object classifiers | Features/training, models, prediction/review/apply, retained training and classifier records | Tools or Analyze → Classifiers; F4 | `gui_object_classifier.hpp`; train capability gates stay native |
| Pixel classifiers | Training/prediction, model files, preview, contours/publish, retained runs, stream export | Tools or Analyze → Classifiers; F5 | `gui_pixel_classifier.hpp`, pixel archive and contour/stream contracts |
| Pixel stream files | Open/verify/read/navigate/close, preview, retained links/relink/remove | Pixel classifiers → Saved stream files | `gui_pixel_stream_browser.hpp`; separate file lifecycle retained |
| Raw pixel classifier | Training/models, preview/files, retained files, streamed contours/replay/apply | Tools or Analyze → Classifiers | `gui_raw_pixel.hpp`; may open file/model tools without an image |
| Quantitative image | Display/axes/series, native data measurements/exports, results, retained analysis/verification/removal | Raw channels F2/toolbar; Tools or Analyze → Project tools | `gui_raw.hpp`; quantitative-image gate and reader integration preserved |
| Spatial analysis | Source/target selection, distances/neighborhood/density options, tissue/exclusions, run/cancel/keep/recheck/export/catalogue | Spatial toolbar; Tools or Analyze → Spatial and fluorescence | `gui_spatial.hpp`; all spatial operations remain native |
| Project density | Saved project/image/class cohorts, preview/calibration, run/cancel/resume/reuse, outputs/status | Tools or Analyze → Project tools | `gui_project_density.hpp`; saved-project requirement and native density service retained |
| Project batch | ROI/tissue/cells/pixel/raw modes, filters, selected/TMA scopes, budgets/checkpoints/resume/report/cancel | Tools or Analyze → Project tools; Ctrl+Alt+B; TMA per-core batch | `gui_batch.hpp`; no batch state machine or defaults changed |
| TMA | Detect/import/manual grid, preview/apply/discard, core selection/missing/case/geometry, rows/columns, grid labels/translation, batch, metadata/QPTMA export | TMA menu; Tools/Analyze; Core/Grid/Export tabs | `gui_tma.hpp`; creation/undo, replay target membership and native import/export retained |
| Fluorescence | Segmentation inputs, sampling, preview, records/provenance/export, correction/unmixing domain choices | Tools/Analyze → Spatial and fluorescence | `gui_fluorescence.*`; requires an open image, native feature gates retained |
| Fluorescence preparation | Marker rules, dark/flat correction, linear unmixing, their preview/model/result controls | Tools search or Fluorescence cells → named child windows | `gui_marker_rules.*`, `gui_dark_flat.*`, `gui_linear_unmixing.*`; not removed merely because absent from the top-level tool list |
| Gene expression | Dense/backed views, gene selection/normalization/display, ROI summary, comparison and existing capability-specific options | Tools/Analyze; Gene comparison child window | `gui_genes.hpp`, `gui_backed_genes.hpp`; unrelated expression-service work remains separate |
| Registration | Landmark pairs, fit/transform/preview, records/verification/export | Tools/Analyze → Landmark registration | `gui_registration.*`; no transform or geometry change |
| Spatial import | Existing open-format sources including indexed/H5AD paths, input previews, image/project choice, calibration/alignment, validation/commit/cancel | Tools/Analyze → Import spatial expression; file picker | `gui_spatial_import.hpp`, indexed-import/file-picker contracts; native import interpretation retained |
| Source and files | Checksums, active/file source, source review/acceptance, optional output/progress/cancel, recorded source state | Workflow → Source and files | Native fingerprint/source verification; exact stored identity is not relaxed |
| Dialogs and menus | All 13 registered modal kinds, action roles/ownership, cancel/escape/close, replacement and destructive choices | `gui_dialog_contract.hpp` inventory and shared menu components | Existing registry/action tests retained; no modal or target enum entry removed |

## Coverage limits

New validation targets navigation, conditional availability, collection sizing,
normal/scaled/localized rendering and data preservation. Unchanged scientific
recipes reuse their existing source-scoped evidence; this does not assert every
combination, biological acceptance or full QuPath parity. Native tool windows
remain free to expose advanced parameters; simplification removes navigation
duplication and unused layout space rather than scientific controls.

Reviewed: code duplication, design boundaries, navigation/usability, nested
surfaces, conditional tools and exit paths. No deceptive-choice finding was
reported; no checkout, consent or subscription flow exists in this scope.
Tooling: native catalogue/handler/sizing/docking tests, normal/French-200% views,
TMA edits/undo/exact CSV, raw channels/planes/measurement/keep/save/reopen,
public target membership, and existing source contracts. The preservation check
retains 331 static control/result captions across the six affected scientific
tool modules and keeps public input/target enums and all modal definitions exact.

Residual scope: unchanged advanced scientific operations retain their previous
evidence. Integration-only reader, gene and expression changes remain separate
and are not requalified by these UI checks. Unchanged advanced scientific
combinations retain their earlier source-scoped receipts.
