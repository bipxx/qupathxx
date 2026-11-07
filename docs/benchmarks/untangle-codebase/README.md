# Untangle prerequisite: behavior-preserving structural refactor

Applied the owner-invoked [untangle-codebase skill](https://github.com/caldoai/skills/blob/main/skills/untangle-codebase/SKILL.md)
with both Comments and Tests selected. The [decision log](../../untangle-decision-log.md)
records inventory and redesign before edits. Three independent agents reviewed
core, workflow and frontend ownership, then reviewed the integrated fixes.

| Item | Delivered structural change |
| --- | --- |
| U1 | One neutral JSON-string encoder replaces checkpoint-owned and metadata copies; one pinned JSON dependency declaration |
| U2 | Watershed types/region/response/sampled modules have explicit includes/namespaces; no contextual include fragments |
| U3 | Common cell contours/proof/repair and tolerant archive loading live in one module; fresh detection remains strict |
| U4 | Density compatibility has its own native helper; asynchronous test failures cancel/join and collision fixtures never replace |
| U5 | One replay vocabulary preserves numeric values, Python names and historical registration order |
| U6 | Panel-owned target lists derive storage, indices and routing, preserving gaps and browser delegation |

Removed stale production/candidate comments, a narrating comment and a redundant
JSON lambda. Numerical proofs, attribution and compatibility/OS explanations stay.
One smaller replay acceptance test was superseded by full-domain behavior checks;
meaningful scientific/reference/archive/quota tests were retained. No wholesale
renaming, generic command framework, numeric rewrite or new format was introduced.

## Validation and source scope

- Ten headers compile as first/sole include. [Header log](headers.log).
- Fourteen affected native suites pass in8.09 s; the new no-window membership
  suite also passes. [Native log](native-tests.log), [membership log](gui-membership-tests.log).
- The final repeat scan found provenance's remaining JSON encoder copy. Final
  provenance/source-identity suites pass in0.20 s after that consolidation;
  all37 binding objects remain byte-identical across the small relink.
  [Final tests](provenance-tests.log), [object reuse](binding-object-reuse.json).
- Four real-slide detector runs preserve all2,237 cells, exact nucleus/body labels
  and ordered geometry hashes against the retained original. The rebuilt module
  also preserves all66 enum members/names/numbers/iteration order.
  [Result](real-output.json), [check](real-output-check.py), [output](real-output-check.log).
- Independent reviewers verified unchanged moved algorithm bodies, reuse policy,
  archive fallback/cancellation, escaping and panel target/index maps. No further
  worthwhile structural issue was found in U1-U6 after the repeat pass.

These are16 distinct native suites across the two source-scoped steps. The first
step raw tree isf71b4b256423dd001cc33c24f4dded0d3d50c57f5c52649a23b9d11ca5639b10;
only provenance.cpp changes in the final step. Unchanged-kernel evidence is reused
within that dependency scope, not transferred into a new full qualification.
Final raw tree isd1bee927656c1ada8b05774f9cb6eb1216fe9238d4e0144f32fdad9b5ded9910,
with530 inputs. [Receipt](receipt.json), [final source](source-inputs.sha256),
[earlier source](pre-provenance-source-inputs.sha256), [runtime bytes](runtime.sha256).
Compiled revision isffdb404a+dirty; the source commit adds closeout documents later.

MSVC Release cache/runtime is main build/ci/gridfrontends167-package/Release.
The direct native load avoids another runtime copy/install. Large QPX/mask inputs
and all work/failure files remain in build/untangle-validation and earlier retained
speed evidence. No binary release/wheel or frozen candidate5 asset changed.

## Waste correction and remaining scope

The initial build named a nonexistent save-test target. It stopped after building
the core/first five targets; the corrected remainder reused all objects. Both
[native build logs](native-build.log) / [resumed log](native-build-remaining.log)
are retained. The binding build was the dominant cost; do not repeat it to update
documents or enum-compatible stubs. [Consumer build](consumer-build.log) and
[small relink](provenance-relink.log) show that the final metadata correction did
not recompile bindings. Future scans should search encoding operations as well
as helper names, and settle tests/CMake wiring before building.

No speed advantage follows from these changes. Diagnostic detector times in the
correctness report are not a fresh paired benchmark. The last settled paired
result remains320 ms native versus292 ms QuPath. L1-L4 own measured kernel
optimization; L5-L7 still own removal of Python density/import/selection rules.
These known language migrations remain open and are not claimed complete by this
structural prerequisite. See the [ordered plan](../../api-delivery-plan.md#c-logic-optimization-and-thin-python-control-plan)
and [coverage](../../speed-gap-coverage.md). No100% QuPath parity claim is made.
