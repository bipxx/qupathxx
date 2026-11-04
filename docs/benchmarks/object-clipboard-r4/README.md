# R4-CLIP1: annotation clipboard

Local Windows engineering verification. Native source `dd33c2b1890e6e90b221b4b62b18db9e9eebc92f`;
final observer/stubs `576b5b8196d98a26961612f0d9725ad15116b584`. All M0-M7 remain Partial.
See the [user/API contract](../../annotation-clipboard.md) and
[identity/results summary](validation.json). The native source fingerprint
`16a16f515837c413ca408d5cdc674455c76a0e77fc13dff7c316550e01f4938f` covers all 453 compiler inputs.
All 62 installed package members and wheel RECORD hashes
match. Final native SHA256: `53bc10ff0321b58ab05c2c9da010d199da0d6846150c1afb15ffce3cd67d025e`.

The new native `copy_objects`/`paste_objects` functions reuse the existing strict
GeoJSON decoder/exporter and atomic append/remapping authority. Two-argument
append callers keep their existing interface/history label. The explicit
selection/descendant, plane, coordinate, quota and loss contracts are documented
in the guide; file-import dimension matching remains unchanged. Python exports,
generated types and actual system-clipboard ImGui actions use that authority.
No new persistence schema, reader, decoder or UI object model was introduced.

| Evidence | Result / actual assertion |
| --- | --- |
| [Old installed baseline](baseline.xml) | Missing copy API reproduces on eb53f2bc; original failure retained |
| [First staged cohort](staged.xml) plus [three repaired observers](staged-retry.xml) | 173 distinct passes / 14 declared desktop skips. Includes 18 new headless clipboard cases and affected earlier editing, interchange, facts/state, planes, selection and project contracts |
| [Native suites](native.xml) | interchange, document_edits, selection, geometry_edits and project all pass |
| [Installed desktop](desktop-retry.xml) | Actual copy/paste with selected fresh IDs; undo/redo changes model revision twice, then save/reopen retains graph/nucleus/properties/lock/color. A second flow closes the source window and pastes to a different displayed plane |
| [Packaged stubs](public-stubs.xml) and [typing](typing-retry.log) | Five runtime/package stub contracts pass; positive example passes and [four exact negative diagnostics](typing-negative-retry.log) reject wrong arguments |

The clipboard contracts independently assert source-text snapshot independence,
selection-only/descendant scope, omitted-parent reporting, class-color and graph
remapping, cell nuclei and retained facts/state, target calibration, cross-image
pixel coordinates, malformed/duplicate/NUL/CRS text, bounds/class/plane refusal,
byte/ID limits, unchanged state/history/next IDs after refusal, and one atomic
paste history entry. Existing ObjectSelection behavior stays in force: undo
prunes removed IDs and redo does not automatically reselect them. Selection is
therefore checked immediately after paste, before the actual history flow.

Captured outputs were inspected: [pasted selection](clipboard.png),
[successful saved project after history](clipboard-history.png), and
[paste on displayed plane](clipboard-plane.png). The tiny UInt16 TIFF uses its
existing broad display window; its dark canvas does not imply missing pixels.

## Waste review and retained attempts

One cached combined native build took 706.00s, followed by one
stub generation and one final-product installation. The header/API/GUI changes
were settled together. Final types were repackaged using identical native bytes.
Healthy checks were reused; no full qualification, new fixture campaign,
biological comparison or speed campaign was run.

Three first-stage failures remain: two tests omitted the informational probability
extension notice and one called the active_plane property. Only those three were
retried; 170 earlier passes and five native suites were reused. The original
[desktop attempt](desktop.xml) remains: it expected automatic reselection after
redo and copied a channel-specific object hidden by the fixture's display view.
The corrected test checks immediate paste selection, real history/save/reopen,
and an explicit visible-channel precondition. No native correction was needed.
The original [typing setup failure](typing.log) remains: putting an entire
site-packages directory on MYPYPATH is rejected by mypy. Normal installed-
interpreter resolution passes; packaged-stub tests were not rerun for that repair.

Plan change: CLIP1 is locally verified. Next address the demonstrated R5 saved-
data root-properties loss; reuse native immutable facts and the existing import,
review, storage and frontend paths. Broader editing/interchange, scientific,
whole-slide resources, platform support, integrations and release work remain.
Only openly licensed readers/codecs are eligible; this task adds none.

## Reproduction context

Commands and immutable source inputs are recorded in the adjacent JSON/log files;
[helpers](helpers) are archived from `build/ci` and retain that original directory
layout assumption. They are evidence, not instructions to rerun healthy campaigns
from this directory. Source worktree was `build/ci/scientific-grid-159`; native
cache was `build/ci/gridfrontends167-package`. Wheels and five retained native
executables remain under local `build/object-clipboard-r4`; their hashes are in
validation.json. Native identity is dd33c2b1; observer fixes/stubs are 576b5b81 and
did not change compiler inputs. No source-bound platform, biological or overall
speed claim transfers from older qualifications to this code.
