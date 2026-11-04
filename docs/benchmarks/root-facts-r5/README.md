# R5-ROOT1: historical image facts

Local Windows engineering verification. Final native source
`405ba1adba476fb736790ca486c3526db2c210dd`; unchanged native kernel checks retain
`ce786260a48fd42935f2fdc5f86ffe5eb060b867` scope; final observer/stubs
`49afda8b8ab0ad1ee32dc55c0c70075c184fb717`. All M0-M7 remain Partial.
See [API/workflow](../../image-properties.md), [storage](../../project-storage.md#historical-image-facts)
and [source/package/results](validation.json). All453 compiler inputs match the
final native fingerprint `252b818ca31d1cc9c591f18d367982ea618bac02ecda7b8a9b103a1b3759a27b`; all62 installed package
members and wheel RECORD hashes match. Final native SHA256 is
`bf9555a762d9ed9622372adcd29eded121a9662f82473806d8838a67daf15d0f`.

`Document.image_properties` retains immutable root metadata/measurements without
creating a fake ROI. Native QuPath version3 import, thin Python/types, ImGui
historical inspection and schema19 snapshots/save/reopen share the same authority.
ObjectProperties facts/limits and checksummed codec are reused. Image-level ROI
lock/color/probability flags refuse, empty facts normalize to None, equal values
are no-ops, and set/clear is ordinary undoable history. Selection, object IDs,
ROI facts/calibration and ROI/render revisions stay separate.

The [new synthetic pinned capture](../../../tests/fixtures/qupath-root-facts-0.7.0/manifest.json)
independently records Unicode metadata, finite/NaN/infinite/signed-zero root values
and different child facts. The existing496-file QuPath0.7.0 runtime matches its
pinned tree hash; there was no new download or broad fixture campaign. Source
PathObject/PathRootObject hashes match the existing pinned serialization sources.
The [old installed baseline](baseline.xml) reproduces root_properties_dropped.

| Evidence | Result / decisive behavior |
| --- | --- |
| [Owned candidate cohort](staged-collect-fixed.xml), [final storage cohort](staged-store.xml) and [two observer repairs](staged-report-fixed.xml) | 228 distinct passes / eleven declared skips, including15 new headless cases. Only ten unchanged pure document/classification cases are reused from ce786260; final storage/import/snapshot/child workflows run405ba1ad |
| [Six native suites after ownership repair](native-owned.xml) plus [final writer suites](native-store.xml) | Document edits, object classifiers and interchange retain ce786260 input scope; project, project_save and qupath_project pass on405ba1ad. The sole compiler-input change between those sources is project.cpp; reused kernel tests do not call Project |
| [Actual desktop](desktop.xml), [inspected capture](image-facts.png) | Root12.5, child historical3.5 and live ROI120px2/15um2 appear separately; real save/reopen retains values and state |
| [Packaged stubs](public-stubs.xml), [positive typing](typing.log) and [negative typing](typing-negative.log) | Five package/runtime contracts pass. One positive file passes; three exact diagnostics reject a map instead of facts, ignoring None and writing the readonly property |

Independent contracts cover owning immutable values, unchanged ROI/calibration/
selection, history admission/no-op/refusal, async captured-value isolation and
payload admission, legacy default/migration (including the existing1-18 cohort),
transaction rollback, checksum/orphan/invalid-state rejection, source preservation,
and actual native import without loss permission. Schema19 is one sparse image
row using the existing facts codec; loader candidates are validated privately.

## Correctness findings and waste review

The [initial final-release regression](ownership-before.xml) deterministically
reaches a15s bounded-child timeout: when history is disabled and facts are
replaced during a blocked save, worker cleanup owns the last shared control that
retains a Python wrapper while wait holds the GIL. [Original test bytes](ownership-before-test.py)
and their hash are retained. The fix is at the common ObjectProperties authority:
Python construction creates a native shared value and shared_from_this-aware
conversion reuses native ownership. Image and ROI final-release fault/retry cases
both pass; constructor signatures and Python value identity remain compatible.
A separate wait/controller or per-frontend model was unnecessary.

The first staged storage run exposed an actual borrowed-buffer lifetime bug:
encoded temporary vectors passed to SQLITE_STATIC were destroyed before done().
Both image and existing ROI facts now keep named encoded buffers alive through
statement execution; the private API deletes the rvalue overload. This retains
zero-copy writes without dangling storage or copying every large BLOB.

Process failures are preserved: [collection](staged.log) selected a private helper
instead of its public caller; the selector now preflights actual pytest names.
Five first-stage child failures loaded the older installed environment because
-I ignores parent sys.path injection. A dedicated staging-only environment points
to the unpacked product via site .pth, so parent and isolated children use the
same package before the final install. The two remaining report expectations
hard-coded schema18; they now share the independent expected-current-version
constant. Legacy fixture version numbers/old table layouts remain explicit.

There were three successful cached builds:670.91s for the combined feature,
88.97s for the shared ownership correction and34.24s for the two-buffer writer
repair. The latter corrections cover demonstrated failures, not qualification
churn. There was one final stub generation and one product installation. The
single redundant extraction was reused after processes ended; all candidate
wheels and failures remain. Pure model checks were reused, while changed
ownership/type/storage paths were retested. No platform, biological or overall
speed campaign was run. Constructor/caster final-release ownership and borrowed
buffer lifetimes should be traced before freezing the next API.

Plan change: ROOT1 is locally verified. Next address the explicit saved ImageData
image-type/stain-default losses using existing stain transforms, request identity,
import review and storage. Broader ImageData properties/formats, research parity,
resources, support and release remain open. No reader/codec was added; the
open-source-only reader policy remains in force.

## Recorded execution context

Source worktree was build/ci/scientific-grid-159 and cache was
build/ci/gridfrontends167-package. [Helpers](helpers) are archived from build/ci
and retain that original layout assumption. Adjacent command JSON and raw logs
are evidence, not instructions to rerun healthy historical attempts. The original
failed selector input is schema-selection-before.json. Candidate wheels, extracted
stage and retained test executables remain locally under build/root-facts-r5;
identities are in validation.json. Documentation/observer updates do not relabel
compiled products or transfer older qualification acceptance to this code.
