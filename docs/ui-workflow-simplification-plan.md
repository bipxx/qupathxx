# UI workflow simplification plan

Status: milestone 1 delivered and verified, 2026-11-11. All twelve findings
have implementation and scoped verification in the
[delivery receipt](benchmarks/ui-workflow-simplification/README.md).
The owner-selected main integration retains the implementation and its focused
evidence; its distinct source/consumer receipt is recorded at the end of the
[delivery receipt](benchmarks/ui-workflow-simplification/README.md).
This plan follows the [detail audit](ui-feature-coverage.md#detail-audit-buttons-repeated-sections-and-wording)
and [VS Code comparison](ui-feature-coverage.md#comparison-with-vs-code-remaining-usability-priorities).
The earlier [UI improvement](ui-ux-improvement-plan.md) and
[panel refinement](ui-panel-refinement-plan.md) plans are delivered history.
Do not replay their completed phases.

## Outcome and delivery boundary

Make the existing research workflow easy to follow:

**Choose image/annotation → configure → run → inspect preview → keep a result
or add objects → view kept results → export → save the project.**

Milestone 1 resolves the twelve detail-audit findings through phases 1–3 below.
Phase 4 is a separately selectable workbench follow-up. Validate and finish the
selected milestone through phase 5; do not automatically start phase 4.

Keep C++ scientific/application authorities, parameters, result identities,
source validation, calibration, Undo, export protections and persistence intact.
ImGui/RmlUI changes are presentation and navigation. No new kernel, binding,
Python consumer, schema, SDK interface, detached OS window or QuPath parity
campaign belongs to this plan. Keep Image left/floating and the toolbar above
the viewer. Retain all existing tools, advanced options and errors.

Extend the delivered header/settings components rather than restoring old menus.
Use the existing integration checkout; preserve unrelated dirty work and user
sessions. Main also contains the separately merged grouped-header/Help change;
reconcile that overlap before any later owner-selected integration.

## Priorities and review gates

Treat these as one coherent workflow improvement, delivered in the following
order. The audit numbers refer to the linked twelve-finding inventory.

| Pass | Audit findings | User-visible outcome | Review gate |
| --- | --- | --- | --- |
| 1. Clear actions | 1, 3, 4, 7 | Stop and Close have distinct meanings; each Apply names its draft; Add, Keep, Save and Export describe their actual effects; shortcut hints leave button captions. | Existing handlers, IDs and shortcuts remain intact; each draft applies independently and Undo restores it; closing does not cancel work. |
| 2. Clear workflow | 2, 5, 6 | One Analysis inputs area leads to Run, preview, Keep and View kept results. Saved profiles require explicit application. | No empty result destination; correct table family/image; kept results retain their own channel labels and identity after inputs change. |
| 3. Clear panel content | 8, 9, 10, 11, 12 | One export prompt; plain density guidance; concise TMA fields; an obvious classifier stage; compact measurements with honest units. | Export protections/loss acknowledgment remain; native values and full numeric precision survive; advanced options and errors stay reachable. |
| 4. Focused verification | All 12 | A researcher can complete the workflow on a copied complex project with readable controls and results. | Record a disposition for every finding and pass the affected native interaction/source checks before calling the milestone complete. |

For each changed panel, review the empty, configured, running, preview, kept,
stale and error states that it actually supports. Avoid instructions about
absent controls and duplicate summaries of the same result. Collapse preview
and kept summaries only when the native Keep event identifies that exact result;
retain both whenever they represent different results. Numeric equality is not
an identity rule.

Use existing action helpers and design tokens for button roles, icon weight,
spacing, field alignment and menu groups. Fix clipping, ambiguous hit targets
and inconsistent action emphasis; do not start a pixel-matching exercise.

The default delivery stops after this milestone. The sidebar/focus, floating
placement and narrow-toolbar work in phase 4 below remains a separate selection.

## Phase 1 — Establish one action and copy contract

Ownership: `gui_layout.hpp`, `gui_icons.hpp`, `gui_i18n_catalog.inc`, affected
classifier/controller presentation and `docs/design-system.md`.

1. Map each changed action to its current native handler, enablement, replay
   target, stable ImGui ID, visible caption, shortcut and tooltip. Use a small
   migration map for changed controls; do not invent a second command framework.
2. Adopt one vocabulary by effect:

   | Effect | Visible action |
   | --- | --- |
   | Start computation | Run analysis / Detect cells / Train classifier |
   | Create document objects | Add annotations / Add cells |
   | Retain a native result | Keep result / Keep training data |
   | Write project state to disk | Save project |
   | Write an external table | Export CSV |
   | Stop active work | Cancel, using a Stop/Cancel glyph |
   | Hide a pane | Native/header Close X |

3. Use one primary action for the current stage. Keep scientific actions as text
   buttons; use familiar icons for utilities. Cancel appears while work can be
   cancelled. Closing/hiding remains independent of job cancellation.
4. Move shortcut chords from button captions into consistent hints/tooltips.
   Preserve keyboard handlers and accessible labels. For example,
   `Run analysis###Analyze region (Ctrl+M)` retains the original English ImGui
   identity while displaying/localizing the shorter caption.
5. Distinguish the two object drafts: **Apply object changes** and
   **Apply color and probability**. The latter writes reported probability as
   well as color; do not label it appearance-only or imply calibrated confidence.
   Disable each action until its own draft differs from the document.
6. In raw/pixel classifiers, separate Stop from Close. Retire duplicate in-body
   Hide presentation only after its existing replay target reaches the retained
   close action. Keep every public enum value and target identity unchanged.
7. Translate changed captions/help across all 16 supported languages. Preserve
   literal project, image, object, class and metadata strings.

Exit: Cancel versus Close is unambiguous; shortened buttons remain reachable
with mouse/keyboard; Apply affects only its named draft; classifiers have a
clear main action without automatically starting another scientific stage.

## Phase 2 — Make results and input sections explain the workflow

Ownership: `gui.cpp`, `gui_measurements.hpp`, existing result/table helpers and
their focused native UI tests. Reuse controllers and native reports.

1. Replace the overlapping Region input blocks with one **Analysis inputs**
   area. Present current measurement mode, white reference and target first.
   Show **Saved profile: H&E (not applied)** separately within that context when
   relevant, with an explicit **Use profile** action and the existing Details.
   Never apply a saved profile automatically or infer that it is active.
2. Keep the existing stain/model editors available through one clear advanced
   route; do not add another stack of folded sections or duplicate parameter
   controls. Tissue/Cells link to the same input authority.
3. Give results a stable presentation sequence:

   | State | Content and actions |
   | --- | --- |
   | No result | Configure and Run; no help referring to an absent Keep button |
   | Running | Progress, Cancel and visible errors |
   | Preview ready | Values, Keep result, and Add objects where supported |
   | Kept result exists | Kept result identity/status, View kept results, Recheck |
   | Preview differs from kept result | Show both explicitly; never substitute one for the other |
   | Stale, cancelled or failed | Clear state/recovery guidance; no false current/saved claim |

4. Replace unconditional **Go to tables** with **View kept results** near the
   result. Enable it when the relevant native result family has kept data;
   otherwise explain **Keep this result to include it in tables** when a
   preview exists. Retain general Tables access and live-object export, which
   does not require Keep.
5. Preserve the existing table-family/current-image routing. Counts and
   availability come from native record accessors, not a new UI result cache.
   Kept-data existence enables viewing; stale/currentness state remains visible
   and governs Recheck/export requirements, not permission to inspect old data.
6. Rename **Saved analysis** to **Kept result**. Derive OD/stain/channel labels
   from that result's own captured kind and provenance, never from current
   draft settings. Keep IDs, currentness, source/calibration and recheck details.
7. Keep Add, Keep and Save separate. Explain the distinction once at the result
   step: **Keep adds this result to the project. Save project writes it to disk.**
   With no project: **No project is open. Kept results are held in memory.**

Exit: Run → Keep → View kept results reaches the correct records; a changed
preview and an older kept result remain distinguishable; Add/Keep/Save do not
trigger one another; jobs and errors remain observable when panes hide.

## Phase 3 — Simplify wording and information density in existing panels

Ownership: Tables in `gui.cpp`; `gui_project_density.hpp`, `gui_tma.hpp`,
`gui_measurements.hpp`, classifier presentation and localization.

| Surface | Concrete change | Preserve |
| --- | --- | --- |
| Tables | One **CSV file** field and stable **Export CSV** button. Table/scope selectors state the destination's content. Show one actionable unavailable reason, not three destination prompts. | Live versus kept source, currentness, calibration options, protected paths and registered replacement behavior |
| Project density | **Check inputs** with **Checks image classes and calibration**; **Previous results folder (optional)**; **Stopping after the current operation. Completed results are kept.** | Save-first/new-output requirements, exact class selection, native reuse checks, cancellation and completed outputs |
| Density technical detail | Move engine/quota/receipt background into reachable Details; keep decision-changing restrictions and actual errors adjacent. | Exact engine/input identity rules; no numerical-equivalence promise |
| TMA setup | **Grid name**, **Rows**, **Columns**, **Width (px)**, **Height (px)**. Use **Minimum tissue coverage**, with **Cores below this value are marked missing.** | Native below/equal/above threshold behavior, detection options, units and calibration |
| TMA export | Use the native omission report for a short loss list; retain mandatory acknowledgment and explicit vacant-slot refusal. | Every applicable metadata loss, exact native output and input/source protection |
| Numeric inputs | Remove forced zero tails where useful. Use shortest round-trip editable text or an existing exact parser; compact summaries must not write rounded values back. | Focus/blur with no edit leaves exact inputs unchanged; full editable precision |
| Measurements | Compact rows or unit columns: e.g. **Area 330 µm² (2640 px²)**. Keep Length accurate for open/closed geometry. | Native values, pixel/physical units, nucleus distinction and unknown-calibration guidance |
| Classifiers | Label Train, Predict and Add objects stages; apply phase 1's primary/secondary action hierarchy. Keep file/model and advanced controls reachable. | Model-only entry, native prerequisites, retained training/prediction distinction, cancellation and contours |

Derive QPTMA omissions through the existing native conversion/report authority,
outside the hot draw path and cached against document identity/revision. Do not
serialize exports every frame or create a second omission/identity rule in UI.

Exit: no ambiguous Cancel/Apply/Add/Keep labels; no repeated input/export
instructions; routine guidance uses plain language. Losses, stale values, units
and scientific meaning remain explicit. All twelve audit findings have either
a validated correction or a named residual reason; none is silently dropped.

## Phase 4 — Optional workbench follow-up

Select separately after milestone 1. This addresses the earlier VS Code
comparison without expanding the current copy/content outcome.

1. Separate primary-sidebar selection, secondary selection and focused pane.
   Focusing right/floating content must not change the marker for visible left
   content. Preserve the active-left click-to-hide behavior and Image policy.
2. Add shared primary/secondary visibility commands through the delivered
   header's Panels/Layout components. Use one dispatcher for menu, keyboard and
   activity controls; keep Batch's Ctrl+Alt+B shortcut.
3. Remove the sparse floating layout row by exposing placement through an
   accessible title/context command. Preserve Project Add/Save actions, native
   Close/resize, saved placement and public replay targets.
4. Keep essential toolbar groups stable. Overflow lower-frequency commands
   at narrow widths without hiding commands, splitting essential groups or
   moving the toolbar away from the viewer.
5. First clarify **Object measurements** and **Result tables** in Workflow.
   Reorganizing all seven destinations into new containers is a later design
   decision; do not implement that larger restructure as an incidental rename.

Exit: independent sidebars, stable toolbar groups and deterministic hide/reopen
work across focus, narrow/scaled layouts and saved sessions. Detached OS windows,
multi-project workspaces and a new command palette remain separate features.

## Phase 5 — Validate once, document and hand off

1. Before implementation, take the latest completed header/Help handoff and
   record affected inputs, current native archive hashes, public contracts and
   retained binding hashes. Do not transfer older qualification to new source.
2. Settle captions, IDs, helpers, callers, locale keys and tests before one
   combined cached frontend build. Recompile every translation unit affected
   by a shared locale/layout header; reuse unaffected objects/native archives.
   No core/binding/package rebuild or historical science campaign is needed.
3. Replace caption-equality inventories for deliberately renamed controls with
   explicit old/new caption and stable target/handler mappings. Keep native
   behavioral assertions and complete tool coverage; a renamed label is not a
   reason to delete a regression check.
4. Run the current applicable source contracts and focused native scenarios:

   | Risk | Decisive check |
   | --- | --- |
   | Cancel versus Close | Hide during work, reopen and Cancel independently; no unintended job cancellation or lost state |
   | Apply scopes | Edit each draft separately; check only intended properties change and Undo restores them |
   | Result retention/navigation | No result, running, ready, kept, stale and differing preview/kept states; correct table family/scope and exact record identities |
   | Profile/units | Saved profile differs from active; OD versus supplied stains; known/unknown calibration; no implicit apply or misleading label |
   | Numeric presentation | Unedited exact-value round trip and TMA threshold boundary; compact measurement rows equal native facts |
   | Exports | Exact CSV/QPTMA, loss acknowledgment, protected-path/refusal behavior and unchanged project/source bytes |
   | Feature retention | All existing parent/child tools, advanced parameters, model-only paths, replay targets and shortcuts remain reachable |
   | Readability | Normal, narrow and French at 200%; all changed captions covered by the 16-language catalog |
   | Workspace, if phase 4 selected | Independent selection/visibility, float/close/redock/restore, toolbar overflow and modal ownership |

5. Walk the selected workflow using a copied complex project and a separate
   preview/layout. Reuse unchanged native evidence; capture only changed
   controls, readable results and meaningful geometry. Preserve user sessions.
6. Update the design contract, coverage map and current card with results,
   actual corrections and residual scope. Stop at the selected milestone.
   Commit, merge, launch or publication follow the owner's subsequent request;
   this planning request authorizes none of those actions by itself.

Completion means the workflow is understandable and all existing effects remain
correct/reachable. Control counts or screenshots alone do not establish that.
An observed pathologist/researcher walkthrough remains separate user validation.
