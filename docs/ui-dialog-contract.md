# Native dialog and popup contract

This is the implementation contract for the native desktop, complementing the
[design tokens and recipes](design-system.md#dialog-and-popup-recipes). It governs
presentation and command dispatch; existing C++ services own validation, jobs,
project mutation, persistence and recovery.

## Registered ownership

Every modal must have one `DialogKind` in `cpp/gui_dialog_contract.hpp`, one
declared controller and one owner scope. Free-form names or alternate modal
frames are not supported. The current registry covers:

| Kind | Controller | Owner |
| --- | --- | --- |
| ImageMetadata | gui_entry_editor.hpp | Captured project and image entry |
| ProjectMetadata | gui_entry_editor.hpp | Captured project |
| CloseProject | gui.cpp | Current project |
| ReplaceWorkspace | gui.cpp | Staged workspace |
| RemoveImage | gui.cpp | Captured project and image entry |
| Language | gui.cpp | Session preference |
| SourceVerification | gui.cpp | Captured project, image, document and native source review |
| DeleteCheckpoint | gui.cpp | Reviewed recovery checkpoint |
| ImportFile | gui_import_picker.hpp | Local filesystem choice |

Each frame constructs `DialogState(owner_valid, busy, variant)` explicitly.
Owner and busy snapshots are immutable. Invalid ownership disables publication
and shows a notice; Cancel and declared Reload remain available when ready.
Busy state disables body, header dismissal and actions. A local submit guard can
disable publication, for example until a source change is acknowledged. Native
handlers must still revalidate at their authority boundary.

## Frame and action invariants

1. `OpenDialog(kind)` requests a modal; `RenderDialog` alone owns popup lifetime,
   viewport bounds, header, notices, scrolling body, footer and dispatch.
2. Actions have typed identities, one label, role, placement, completion policy
   and optional stable scope. Measurement and drawing use the same definition.
   Widget IDs remain stable when the displayed language changes.
3. Each footer begins with exactly one Cancel/Close, followed by any destructive
   choice, with at most one primary action last. Only Reload belongs in a header;
   only declared secondary actions may appear in the body.
4. A fitting footer aligns to the trailing edge; otherwise actions stack at full
   width. Reserve the largest footer needed by the dialog's supported variants.
   Body tabs may change variants without pushing the current footer off screen.
5. Body content scrolls independently. Combine notices into one bounded,
   scrollable region above the footer; retain their complete text. Titles wrap
   before header actions; long context is available on hover. Fields have labels
   above full-width values. Default keyboard focus is Cancel.
6. The active modal owns input and its opaque surface even when the workbench is
   disabled. Re-enable before the popup background is drawn. Background
   canvas/application shortcuts are blocked. A child popup or active text field
   owns its first Escape; only a later unclaimed Escape requests parent Cancel.
7. Select at most one typed intent per frame. Only the handler publishes native
   changes. `Refused` retains the dialog and draft; `Pending` retains it while the
   existing controller advances; `Success` obeys the declared completion policy.
   An available Cancel must return Success without publishing changes.
8. External completion may request dismissal without inventing a second Cancel
   command. Save/close and workspace replacement remain open until their native
   save/recovery state machines finish. Exceptions do not unbalance popup scopes.

Metadata Apply publishes only the named native metadata or description/tag scope
and keeps the editor open; Save project persists it. Cancel discards unapplied
drafts. The language selector changes preferences immediately and therefore uses
Close. The file picker returns a validated existing path or unused output path;
it does not create/write that output or interpret scientific data.

## Popup menus

Use `BeginPopupMenu`, `BeginContextMenu`, `BeginMenu` and `MenuItem` from
`gui_layout.hpp`. They own viewport bounds, row spacing and shortcut columns.
Keep original stable command IDs. Context/row commands target the selected entry
without opening another image or changing the current viewed-image highlight.
Destructive mutations use the registered confirmation when required.

## Enforcement and coverage

`tests/test_gui_dialog_contract.py` checks registry ownership, rejects raw modal
or menu entry points, unmanaged dismissal, free-form frame composition and
duplicated action declarations. Negative fixtures prove that the guards fail
on bypasses. Adding a modal requires a registry entry and a matching controller.

`cpp/tests/gui_dialog_contract_checks.hpp` exercises actual ImGui frames:
malformed definitions, stable IDs in every supported language, invalid/busy
owners, inherited disabled state, refused/pending/success handlers, nested-popup
and active-text Escape, one dispatch per frame, and file choice without writes.
It runs in `qupathxx_gui_replay_tests`, alongside native metadata publication,
stale/duplicate/Unicode, cancellation and Save/reopen checks.

`qupathxx_gui_organization_tests` covers native close/replacement/removal,
metadata background-shortcut blocking, source Cancel/Apply/Save/reopen and
recovery Cancel/checked deletion. GL captures check normal/light/narrow forms,
long joint notices, resized French headers, German labels and the real picker:
viewport bounds, independent body scroll, visible footer and control overflow.

These checks establish the registered desktop workflows. They do not claim full
scientific parity, untested nonmodal workflows or a web-runtime implementation.
