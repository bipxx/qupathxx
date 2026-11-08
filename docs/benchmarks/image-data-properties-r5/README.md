# Portable ImageData properties — local R5-DATAPROP1 closeout

Final native `cb693c5a20e32e40c25a71246613a6955036ae5c` retains portable custom
image properties through native saved-data import, Python, document history,
immutable save snapshots, schema21 reopen and the ImGui inspector. The
[receipt](receipt.json), [compiler inputs](compiler-inputs-overflow.json) and
[observer inputs](observer-inputs.json) identify the exact scope. Later docs,
observers and generated stubs do not change the recorded native identity.

The [pinned fixture](../../../tests/fixtures/qupath-image-data-properties-0.7.0/manifest.json)
captures uncovered scalar, collection, unsupported/cycle and bound cases with
the existing QuPath0.7 runtime. Values are read back after save; integer references
are decimal strings, real references are hexadecimal/nonfinite strings, and
type tags distinguish bool/int/null. The existing saved-data fixture already
supplies the simple string property, so that capture was reused.

The native value tree preserves exact signed64 integers, booleans/null, binary64
values including nonfinite/signed zero, UTF-8 strings and nested lists/maps.
Java wrapper widths/collection classes normalize to portable value types.
Unsupported or cyclic content drops its entire named top-level property only
after explicit loss review. Malformed framing, duplicate keys, excess depth/count/
text/payload and invalid stored encoding refuse before publication. The model
shares native immutable ownership; Python receives independent deep copies.

| Check | Result and actual source scope |
| --- | --- |
| [First staged cohort](staged.xml) | Native3d272074:296 pass/14 skips/two failures across312 cases. Both failures are out-of-range Python integers producing generic bad-cast errors. This failure record is retained |
| [Repaired property cohort](staged-overflow.xml) | Final nativecb693c5a:all40 new headless cases pass; one desktop case skipped here. Covers captured types/collections, exact integer boundaries and explicit OverflowError, whole-property losses, deep-copy/no-op/history/request precedence, worker final release, budgets, migration rollback and independently constructed typed payloads |
| Reconciled headless scope | 298 passes/14 skips across312 distinct cases.258 legacy passes/13 skips remain at3d272074; the41-case property module is replaced by the final cohort. No old binary is relabeled |
| [Current native suites](native-overflow.xml) | Four pass: document edits, project, project save and QuPath project import. Native typed-value/history/render-invariance marker is exercised |
| [Actual desktop](desktop.xml) | One passes: inspect typed values, save, reopen and retain exact values without changing ROI geometry |
| [Installed import](installed.xml) | Final installed native identity/path confirmed; captured typed import/reopen passes |
| [Packaged stubs](public-stubs.xml) and typing | Five checks pass; positive example passes and negative example produces exactly argument-type, optional-assignment and read-only-property diagnostics |
| Product integrity | All461 compiler inputs remain frozen; only `cpp/image_data_properties_bindings.cpp` differs between the two native candidates. All65 hashed final wheel members match installed bytes/RECORD; stub-only repackaging preserves native bytes |

The installed smoke repeats one import case. Desktop executes the one skipped
new property flow; other inherited desktop skips keep their original scope.
These counts are evidence for this workflow, not a completion percentage.

The [capture](image-data-properties.png) was inspected: exact
`9007199254740993`, bool true, nested map/null/real3.5 and Unicode text are visible
in the historical property inspector. The existing ROI remains separate.

Waste review: reused parser/list handling, UTF-8/SHA primitives, native shared
factory construction, history/snapshot/SQL authorities and cached dependencies.
Fact maps cannot represent these types without coercion, so one bounded typed
model/codec serves every consumer. The [initial baseline](baseline.log) reproduces
loss on the prior installed product. The [first failure log](staged.log) retains
both generic bad casts. Checked `PyLong_AsLongLong` propagation fixes the boundary
without weakening assertions. One679.95s build plus34.28s binding-only repair;
one stub generation and one final install. Only the changed property module and
current native roles were rechecked; unchanged legacy results were reconciled
using the one-file compiler-input difference. No full numerical/platform campaign.

Executed [helper copies](helpers/) preserve configuration alongside command JSON;
they retain the original workspace `build/ci` layout and are evidence copies,
not portable entry points. Original candidates, logs and stages remain under
workspace `build/image-data-properties-r5`. Byte-addressed capture/probe/evidence
files retain their executed bytes through Git attributes.

Limits remain explicit in the [API contract](../../image-data-properties.md):
only the recognized pinned representations are retained, shared identity and Java
class implementation details are normalized, executable workflows are not run,
and bounds describe retained state rather than total process resources. Broader
Java classes/versions, other R4/R5 gaps, research accuracy, resource/speed and
supported release claims remain open. All milestones stay Partial.

Next is the actual native saved-data TMA-grid loss. Reuse the delivered TMA model/
commands/store and text-codec evidence; capture only the uncovered binary grid
contract. See the [single current task](../../current-task.md).

Upstream contract references: pinned [PathIO serialization](https://raw.githubusercontent.com/qupath/qupath/v0.7.0/qupath-core/src/main/java/qupath/lib/io/PathIO.java)
and [ImageData property API](https://raw.githubusercontent.com/qupath/qupath/v0.7.0/qupath-core/src/main/java/qupath/lib/images/ImageData.java).
