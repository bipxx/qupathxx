# UI workflow simplification — milestone 1

Status: delivered and verified, 2026-11-11. This receipt covers phases 1–3 and 5 of
[the plan](../../ui-workflow-simplification-plan.md); phase 4 remains unselected.
Three implementation/review agents cover classifiers, TMA/density, and
measurements/interaction checks. Root integrates the shared workflow contract.

## Scope and invariants

The UI uses the existing C++ application/scientific authorities. Caption and
layout changes preserve command IDs, replay targets, shortcuts, prerequisites,
Undo, native result identities, calibration, persistence and export protection.
Add objects, Keep result, Save project and Export remain separate actions.
Stop/Cancel is independent of pane Close. Literal names and imported metadata
are preserved. No kernel, binding, schema, package or SDK change is selected.

## Finding-to-evidence map

| Audit finding | Implementation | Decisive evidence |
| --- | --- | --- |
| 1. Cancel versus Hide | Stop glyph while busy; native X retains the legacy Hide target | Floating/docked close, reopen, sibling and Cancel interaction checks; classifier target/handler audit |
| 2. Premature table navigation | Native kept-record availability and family/current-image routing | No-result, Run/Keep/View and exact CSV interaction replay |
| 3. Ambiguous Apply | Named independent object and color/reported-probability drafts | Both dirty-draft Apply orders, pristine disabled actions and native Undo |
| 4. Add/Keep/Save verbs | Effect-specific labels over the retained native operations | 41 classifier caption/target mappings; fluorescence Run/Add/Keep/replay/Undo consumer |
| 5. Repeated Region input blocks | One Analysis inputs context and Advanced inputs route; explicit Use profile | Applied/not-applied profile replays, native mode/white/profile and saved-byte assertions |
| 6. Preview versus kept | Captured-kind labels, exact record identity and separate differing results | Exact Keep dedup plus differing OD-preview/stain-kept assertions |
| 7. Long shortcut captions | Stable hidden IDs and translated captions with tooltip shortcuts | Native localized ID/click checks, source contracts and classifier migration audit |
| 8. Repeated export prompts | One CSV file field and stable Export CSV action | Exact native ROI/cell exports, selected run, replacement and protected-path checks |
| 9. Density engineering prose | Plain guidance, reachable Details, localized hints and busy-only Stop | Actual input text/Check/Run, exact report/CSV/recipe and deterministic Cancel retention |
| 10. TMA setup/export | Concise fields, exact compact numeric editing, cached native omission report | Threshold-adjacent bit-roundtrips; native omission/ack/refusal/Undo/exact QPTMA and hidden-export checks |
| 11. Classifier hierarchy | One native-readiness primary stage; no automatic next stage | Mutually exclusive role source audit and retained model-only/guard/target checks |
| 12. Repeated measurement units | Compact physical/pixel rows; nucleus and unknown calibration remain | All16-locale native measurement checks and normal/French200 panel views |

## Source and runtime identity

Local artifacts live in `build/ui-workflow-simplification`. `source-final.json`
and `final-inputs/` freeze the GUI/test context. The private runtime links that
context against the retained bc8 native engine; it does not qualify concurrent
unrelated native source edits. `native-before.json` and the retained 38-object
binding manifest are checked again at handoff. The original six-image review
project remains immutable; review edits and exports use its separate copy.

The initial GUI build compiled all nine local units affected by shared
locale/action headers. An affected gui.cpp increment followed; the final result
wrapping change rebuilt its two includers, gui.cpp and fluorescence.cpp, reusing
the other seven objects. Later fixture repairs rebuilt only the organization
consumer. Tests and affected standalone consumers link the matching private GUI
archive. No core, binding or package rebuild was performed.

## Validation record

Workspace-root `build/ui-workflow-simplification/results.json` records all twelve
verified findings, exact binary/source hashes, compatible receipts and residual
scope. `out/finalize_evidence.py` checks source compatibility and all retained
library/binding/original-project hashes before producing it.
`implementation.patch` and `implementation-scope.json` isolate the seventeen
owned source/test changes from their retained pre-integration baselines, keeping
unrelated dirty hunks out of the review patch.

| Passing evidence | Scope |
| --- | --- |
| `source-contracts.json` | 20 source contracts |
| `headless-final02.json` | Native Close/reopen/Cancel, exact numeric focus/blur, all16-locale narrow result/measurement bounds, TMA loss/ack/refusal/export/hidden completion, density Check/Run/Cancel/exact output |
| `workflow-final05.json` | Both dirty Apply orders/Undo, explicit profiles, no-result and stale states, exact Keep dedup, different captured domains, actual French200 scrolling/readability and exact CSV routing |
| `tools-final02.json` | Tool reachability, TMA edit/Undo and exact metadata/QPTMA exports after measured-body scroll |
| `complex-final06.json` | Actual NuCLS rectangle/Run/Keep/View/CSV/GUI Save, native reopen and GUI Recheck; preexisting identities, calibration, raw spot and original project unchanged |
| `membership-final02.json`, `fluorescence-final02.json` | Retained domain targets; actual native Run/Add/Keep/replay/Undo/stale and blocked paths |
| `panels-final.json`, `exports-final.json`, `header-final.json` | Compatible normal/French200 panel/modal coverage, export protections/replacement/run selection, grouped header/Help |

The final product inputs equal build10. Later changes affect test fixtures only;
after build13 only complex review changed. The collector checks the workflow's preceding
test functions against retained build13 source, and headless/tool source against
build10. Compatible build09 checks cover unchanged panel/export/header paths;
the only later product delta is result-row wrapping/bounds observation, directly
covered by the new locale checks and workflow captures. Passing build09/10/13
test binaries, input snapshots and all failed attempts remain available.

All14 cached libraries and38 binding objects match their pre-change hashes.
The original review project retains SHA256
`565512f0d64f270cdc7051f352584e184732f6373e912be10b3d3f4b0502aa4b`.
The native cache has Python OFF and the app dependency list has no Python import.
The classifier source audit preserves41 caption mappings and target/handler sets;
117 new captions each have15 translations plus English.

Corrections addressed native API equality, true dock membership, actual hover/
press ownership, registered wheel replay, native viewport mouse quantization,
RegionRequest field order, partly clipped replay controls and a historical
fluorescence link command missing the retained core's HDF5 dependencies. Exact
geometry tolerance, scientific identities, byte comparisons and native guards
remain intact. The [current card](../../current-task.md) records the handoff.

Captures establish readable values, usable controls and geometry/overlay
registration. They do not establish cosmetic QuPath parity, biological accuracy,
speed acceptance or an observed pathologist/researcher usability study. The
all16-locale headless bounds use the fixture font; actual application captures
establish normal/French200 readability rather than a full visual study of every
language. Commit,
merge, launch and publication remain subsequent owner-selected handoffs.

## Main integration — 2026-11-11

Owner selects local main merge. `build/ui-workflow-simplification-merge` freezes
91 current main-based GUI/test/interface inputs and its distinct native consumer.
The completed UI-only workspace/layout/catalog prerequisites accompany the fixes.
Main's reader, annotation import/export/clipboard, startup and gene handlers are
retained through18 exact overlap blocks; unrelated native/SDK/format-registry
work is excluded. The merged header/Help and child tool routes remain available.

Twenty source contracts, one cached9-unit frontend build and all nine selected
native consumers/replays pass in this source context. These include the complete
copied NuCLS Save/reopen/Recheck workflow and exact CSV/QPTMA, independent Apply,
Close/Cancel, localization/scaling, protected exports, header/Help, catalog and
fluorescence behavior. This is new integration evidence, rather than a transfer
of the earlier binary's qualification. `results.json` holds source/binary hashes
and command results; `merge.json` records the scoped commit and local main merge.

All14 cached libraries,38 binding objects, original review-project bytes and233
old-checkout dirty files plus HEAD/index remain unchanged. No product build/test
failed; an ambiguous preparation brace match was replaced by positional overlap
mapping before compilation. No kernel, binding, package, push or interactive
launch is selected. The separate cleanup task may retire the verified old
checkout only after the merge/Git/source lease is released.
