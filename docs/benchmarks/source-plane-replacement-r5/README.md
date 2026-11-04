# Retained planes on source replacement — R5-SOURCEAXES1

Locally verified Windows workflow. Native source `eb53f2bc50d6f76c47d954cb84ca439d65472923`;
observer `1b359d96a343c4cbc778845dbb254cf3619690e9`. [Validation](validation.json) binds all
452 compiler inputs, 62 installed members and wheel RECORD.
The only changed compiler input from the prior product is `cpp/project.cpp`.
All M0-M7 remain Partial; broader editing, scientific, resource and release work
stays in the delivery queue. No general speed advantage is claimed.

## Behavior

Direct relink and reviewed replacement now check every retained object's plane
before publishing a different source. The shared private guard scans current
object plane values without copying geometry or facts. It reads already-opened
candidate raw metadata once when non-default planes require it. It does not
reopen the candidate or decode pixels. Existing matching-live-source verification
and no-op behavior remain unchanged; ordinary default-plane readers need no raw
metadata lookup. No binding, storage, schema or public signature changed.

| Outcome | Evidence |
| --- | --- |
| Z/time/channel reduction cannot strand retained objects | Six [baseline failures](before.xml) on695158a8 prove the previous omission. All six corresponding staged cases now refuse, preserving old reader/path/baseline, objects, dirty state and saved bytes |
| Benign reduction remains usable | Six compatible Z/time/channel route cases preserve planes, cached-reader handoff, old-reader ownership, raw statistics and saved replay |
| Review is not a frozen object set | Plane edits and undo between review/apply are checked at publication. Refusal leaves the byte review unconsumed; repairing the plane lets the same review apply |
| Source reuse remains intact | Matching-byte movement and live review no-op retain their existing semantics and avoid pixel decoding. Older affected source/project/reader contracts pass |
| Real desktop behavior | Refusal displays [the plane error](axis-refusal.png) and leaves the original source/file. Compatible adoption is [saved through ImGui](axis-save.png), reopened and measured on the retained plane |

The current raw axes are checked at the publication boundary because objects may
change after file review. Source-byte acceptance cannot override missing retained
planes. Unsupported formats/series and existing display metadata checks remain
explicit; no new reader or source-review model was introduced.

## Results

- [Staged new and older contracts](staged.xml): **93 passes,11 declared skips**,
  including15 new headless cases and two opt-in desktop skips. These were run
  before installation and reused afterward with the same package bytes.
- [Native](native.xml): project, qupath_project and file_fingerprint pass.
- Desktop refusal passes in [the first retry](desktop-retry.xml); desktop
  compatible apply/save/reopen passes in [the corrected save case](desktop-save-fixed.xml).
  Both captures were visually inspected. The saved dataset's independent expected
  raw mean is13; its retained Z plane survives restart.
- Existing generated stubs are byte-identical to the prior wheel and source.
  Prior stub/typing checks retain their unchanged binding-input scope; no repeat
  generation or typing campaign was needed.

One cached build took **37.37s**; one product installation; zero stub generations.
No full platform/scientific/performance campaign ran. Native SHA256:
`a9286487d96fc6a48780c16a11e41b3c79a9dc7b0aa2d3ad23375d7ee470b04a`. Wheel SHA256: `f18c27168ccb55a8548acb29cdfda841fbfeeded08bd731e9dacdbe580b4a2f3`.
Wheels, installed environment and verified native products remain at root
`build/source-plane-replacement-r5` and `build/ci/source-plane-replacement-r5-env`.
Only raw receipts/logs, helper copies and captures are committed here.

## Retained failures and waste review

The initial [desktop run](desktop.xml) has two failed assertions. GUI viewport
state legitimately dirties a project independently of source adoption; the
headless refusal cases still enforce the complete unchanged dirty state.
Desktop source/file/baseline/plane assertions were separated from viewport state.

The first retry passes refusal but reports zero saves for compatible adoption.
[A hover/timing retry](desktop-save.xml) still reports zero saves. Both attempts
were observer mistakes: appending/extending `GuiOptions.inputs` mutates a Python
copy from nanobind, so the save action never reached C++. The fix builds the
Python event list locally and assigns it once. The corrected test requires an
actual completed save plus reopened source/plane/data evidence; it does not
replace saving with a delay or merely accept in-memory success.

Retain all three failed desktop attempts. Changing timing/targets before checking
container ownership was waste. Inspect actual supported methods and copied
container semantics first. Reuse this same verified product and all passing
staged/native checks; no native correction, reinstall or broad repeated cohort
was necessary. Earlier desktop observers were working files before1b359d96;
raw logs/commands are retained without claiming that commit reproduces their
exact test text. Archived helpers identify as-run root build/ci commands, not
instructions to redispatch the completed workflow.

Redundant extraction copies are retained under the previously reported automatic
approval deletion restriction; no alternate cleanup was attempted in this task.

## Next workflow

R4-CLIP1: annotation copy/paste. Current GUI/object APIs have drawing, edit,
append/import and merge authorities but no annotation clipboard flow. First
inspect pinned upstream selection/descendant/plane semantics, then reuse the
existing native snapshot/append/command paths. Preserve geometry and intended
facts/state, generate new IDs with correct parent mapping, make unsupported
cases explicit, and finish actual native/Python/ImGui paste plus undo/reopen.
Do not add a second UI object model or repeat delivered source/plane mechanisms.
