# Saved image analysis defaults — local R5-SETTINGS1 closeout

Native source `c68bc511c28d571d52e02300092b81c7d4eeca36` retains recognized
QuPath0.7 image type/stain defaults through import, native/Python inspection,
explicit desktop adoption, document history, snapshots and schema20 reopen.
The [receipt](receipt.json) records exact product, compiler-input and executable
identities. [Compiler inputs](compiler-inputs.json) remain unchanged after build;
later documentation, Python observers and generated stubs do not relabel native
source acceptance.

The pinned [fixture](../../../tests/fixtures/qupath-image-settings-0.7.0/manifest.json)
and [capture command](capture-command.json) cover all six image types, custom
three-stain identity vectors with Unicode profile text, and wide white references.
The probe saves and reads ImageData back before recording names, normalized
vectors, white and residual identity. Independent upstream vectors agree within
absolute1e-14 (zero relative tolerance); native save/reopen preserves exact values.
The existing native stain normalization, residual, matrix and model-hash authority
is reused; no numerical kernel was replaced.

| Verification | Result and scope |
| --- | --- |
| [Staged affected contracts](staged.xml) | 257 passed/13 declared skips across270 selected cases;29 new headless defaults cases include malformed foreign records, unsupported RGB8 application, history/no-op/clear, request precedence, owning snapshots, legacy19 rollback/upgrade and damaged stored payloads |
| [Native suites](native.xml) | All six pass: document edits, analysis, stains, project, project save and QuPath project import; actual defaults history marker is exercised |
| [Desktop](desktop-fixed.xml) | Two pass: saved defaults stay inactive until explicitly adopted; adopting the full third vector/white is retained in an actual kept result and survives project reopen |
| [Installed import](installed.xml) | One exact candidate import/reopen smoke using the final installed product |
| [Packaged stubs](public-stubs.xml) and typing | Five stub checks pass; positive example passes, negative example produces exactly argument-type, optional-assignment and read-only-property diagnostics |
| Package/source integrity | All457 recorded compiler inputs match; all65 hashed wheel members match their installed bytes and RECORD; native bytes unchanged by stub-only repackaging |

These are cohort-specific counts. The installed smoke repeats one staged case;
it is not an additional distinct feature. GUI skips in the headless cohort are
separately exercised only for the two declared defaults cases. Windows local
evidence does not qualify other hosts or every existing skipped desktop flow.

The [desktop capture](settings-True.png) was inspected: the saved type, Unicode
profile, channel names, full basis and white appear in the inspector, and the
analysis controls show explicitly adopted supplied stains. The archived result
provides kind/white/full-basis assertions; no extra telemetry API was introduced.

The [initial baseline](baseline.log) reproduces dropped settings on the prior
ROOT1 product. The [first desktop attempt](desktop.log) and its [XML](desktop.xml)
are retained: both cases reached analysis, but the observer assumed an unexposed
`GuiReport.analysis_result`. Corrected tests use existing Keep result/archive
APIs and compare saved results. Only the affected two GUI cases were repeated;
the native/headless successes remain in their original unchanged scope.

Waste review: one cached build took764.38s, one stub generation and one final
install. Reused the existing build/dependency cache, pinned runtime, stain kernel,
review/store authorities and passed evidence. Removed the assumed report member
and an incorrect documentation filename; no native rebuild, new decoder/model,
additional reviewer or full science/platform qualification was needed. Preserve
the executed probe bytes and byte-addressed evidence with Git attributes; no
recapture is needed for line-ending policy. Stale documentation anchors were
redirected to existing headings and checked without a product build.
[Commands/helpers](helpers/) and the receipt preserve configuration; original
local outputs remain in `build/image-settings-r5` under the workspace root.

Limits: high positive white references are retained, while RGB8 transforms and
GUI adoption refuse values outside1..255. Defaults never automatically rewrite
explicit requests or retained recipes. Unsupported ImageData properties,
executable workflow history, other formats/records, broader interchange,
biological acceptance, whole-slide resources and release support remain open.
All milestones remain Partial, and this is no overall speed-superiority claim.
Next task is the existing saved ImageData-property loss, using the same queue.

See the [public API contract](../../image-analysis-settings.md) and
[single current task](../../current-task.md).
