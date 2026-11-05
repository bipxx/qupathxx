# Native saved-data TMA grids — local R5-TMASAVED1 closeout

Product `9627e54507b400a676f3599041e8535e15768635` retains recognized pinned
native TMA grids through direct project import, existing TMA edits/history,
immutable snapshots, save/reopen and ImGui metadata export. The [receipt](receipt.json)
binds the installed product to its [461 recorded compiler inputs](compiler-inputs.json)
and separate [observer inputs](observer-inputs.json). No public API, schema or
native TMA model changed.

The [pinned fixture](../../../tests/fixtures/qupath-saved-tma-0.7.0/manifest.json)
contains eight binary-grid variants. Valid 2x3 grids deliberately reverse hierarchy
order, repeat core names and include fractional ellipse bounds, missing/case
state, legacy Unique ID, facts/classes/locks/color, child annotation/point/
detection/cell/nucleus and an unrelated root object. QuPath reads saved data back
before recording the reference. Invalid captures cover ragged/duplicate/empty
grids, absent members and unlisted/orphaned cores.

One native layout and UUID-to-slot map validates grid/hierarchy membership. The
existing geometry/ID import runs once; one staged TMA metadata edit attaches that
layout to the same objects inside the unpublished preview. Existing TmaCodec,
commands, consumers and store remain authoritative. No annotation-only downgrade,
new field, alternate TMA model, detector or repeated QPTMA-text capture was needed.

| Evidence | Result and scope |
| --- | --- |
| [Staged imports](staged.xml) | 64 pass/four declared skips across 68 cases, including 20 new headless cases and affected non-TMA geometry/plane/fact/default/property import regressions |
| [Original native roles](native.xml) | TMA, project and project-save roles pass. QuPath-project witness fails its raw Unicode expectation; original failure is retained |
| [Repaired native witness](native-observer.xml) | QuPath-project role passes after the expected alpha character is written as an ASCII universal Unicode escape. It checks binary slots, missing/case state, legacy fallback and membership refusals |
| [Actual desktop](desktop.xml) | Existing TMA panel recognizes 2x3 grid, exports all six metadata rows with two missing slots, saves and reopens correct IDs/children/state |
| [Installed workflow/stubs](installed.xml) | One additional two-image namespace/case-pooling workflow and five unchanged packaged-stub checks pass against the exact installed product |
| Product integrity | Product source-manifest SHA matches its original compiler inputs; all 65 hashed installed wheel members match RECORD and wheel bytes; stubs match existing source and were not regenerated |

The 22 new cases comprise 20 staged headless cases, one installed multi-image case
and one desktop case. Desktop resolves the new flow's headless skip; inherited
skips retain their original scope. Counts do not establish overall project parity.

The [desktop capture](saved-tma.png) was inspected: the existing TMA panel shows
two rows/three columns and a saved CSV export, alongside core/child overlays.
Tests compare exact bounds/IDs/parents/cases/flags and native ellipse/area behavior,
then exercise edits, undo, translation with descendants, immutable snapshots and
cross-image case inventory. This is engineering interchange evidence, not a
biological dearraying or measurement-accuracy campaign.

The original [baseline](baseline.log) refuses TMACoreObject on the previous
installed product. Two [capture attempts](capture.log), [second attempt](capture-fixed.log)
stop at Groovy parsing; leading-dot decimals and nested ternaries adjacent to
list literals were replaced by explicit literals/simple conditionals. Exact
failed probes remain alongside successful capture command metadata.

The first [native log](native.log) fails the Unicode witness. Only that C++ test
was repaired/rebuilt; expected data/assertions stayed intact. Its build triggered
CMake's recorded-input dependencies and refreshed provenance.cpp, so an
identical-core-archive postcheck also failed. The [build command/postcheck](native-observer-build-command.json),
[prior hashes](native-observer-before.json) and [observer manifest](observer-build-manifest.hpp)
preserve this distinction. The repaired observer uses its recorded `+dirty`
source identity; it is not labeled as the original product. Runtime code inputs
are unchanged, and the retained wheel/staged/installed product remains 9627e545.
Both original product and observer source-manifest hashes are checked independently.

Waste review: one 46.05 s cached product build, one targeted native-observer rebuild
(elapsed time not recorded), one install and zero stub generations. Reusing TMA
state avoided header-wide binding compilation and schema/API churn. Only the
failed native role was repeated; successful product/headless evidence was reused.
No extra wheel, detector/math/text-codec qualification or platform campaign.
Read actual paths/headings before small documentation repairs; no product build
is justified by a stopped documentation patch.

Executed [helper copies](helpers/) retain their original workspace `build/ci`
layout and are evidence copies, not portable entry points. Original logs,
candidates and the failed native executable remain in workspace `build/saved-tma-r5`.
Byte-addressed probes/fixtures/evidence retain executed bytes through Git attributes.

Limits and supported forms are in the [contract](../../qupath-saved-tma.md).
Unsupported empty/ragged/non-ellipse/unknown or inconsistent representations
refuse. Assisted saved-state exchange, other binary versions/classes/builders,
broader research/resources/platforms and release remain open. All milestones
remain Partial. The next task extends the existing assisted route to the native
state authorities already delivered; see the [current task](../../current-task.md).

Pinned references: [DefaultTMAGrid](https://raw.githubusercontent.com/qupath/qupath/v0.7.0/qupath-core/src/main/java/qupath/lib/objects/hierarchy/DefaultTMAGrid.java),
[TMACoreObject](https://raw.githubusercontent.com/qupath/qupath/v0.7.0/qupath-core/src/main/java/qupath/lib/objects/TMACoreObject.java)
and [hierarchy](https://raw.githubusercontent.com/qupath/qupath/v0.7.0/qupath-core/src/main/java/qupath/lib/objects/hierarchy/PathObjectHierarchy.java).
