# Current implementation task

Use the [single R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [ordered slices](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work).
R0 is accepted; R1 inventory is reconciled with owner decisions open. Recent R2,
R4-MERGE1 and R5-QPPROJ1/QPDATA1 have scoped local evidence. All M0-M7 remain Partial.

Current: **R5-RELINK1, retain the verified source reader**, implementation in progress.

- Outcome: relink publishes the exact verified reader through the existing
  LinkedServer instead of reopening the source lazily after verification.
- Entry/evidence: Project::relink in cpp/project.cpp opens read_image for compatible
  metadata, then discards that server. LinkedServer::open_locked opens the path a
  second time on first access. This duplicates I/O and permits a different source
  version between verification and first use.
- Current evidence: prior installed product fails three new handoff cases and
  passes rejection preservation. The patch transfers the candidate and uses the
  existing source-publication helper for both initial open and relink; one cached
  build/install and affected checks remain before closeout.
- Reuse/dependency: existing reader guards/source observations, LinkedServer
  generation, metadata/calibration and TIFF view/options. Keep save snapshots lazy
  and old external reader references valid. No owner or biological decision needed.
- Checks: immediate observed source and one verified handoff; path changes obey
  the captured reader's existing source-guard/owned-buffer behavior; typed TIFF
  selection, retained calibration, old-owner lifetime and save/reopen; rejected
  relink preserves the original document/path. Use affected native/project/Python
  flows and existing stubs; no API/schema or full platform campaign.
- Stop: this concrete reopen interval is closed. Persistent fingerprint/replacement
  policy and other recovery/interchange gaps remain under R5.

Then select the next uncovered R4/R5 behavior, followed by R6/R7, R8-R10, R11,
R3, R12 and R13; pull required eligible R2 inputs forward and retain R14-R16 parity.

Latest closeout (2026-11-04, R5-QPDATA1):
Outcome/evidence: [native version3 saved-data import](benchmarks/qupath-saved-data-r5/README.md)
has12 new direct passes,257 distinct selected passes/four declared skips, three
native suites, two selected desktop flows and installed-stub consistency. Native
cc85a848 and observer e8885841 retain identities;442 compiler inputs and all62
installed members/RECORD verify. Other versions/properties/planes remain open.
Waste/cost: one cached build and one install. Reused TIFFs/runtime/native geometry
and project snapshots. Two initial test assumptions were corrected on the same
product: uppercase enum names and geometry equality across analytic/polygon forms.
Only failed cases and affected observer controls reran. Public headers/bindings
stayed unchanged, so typing/stubs were reused without regeneration/repackaging.
Plan change/next action: remove relink's discarded reader and second open by
handing off the verified candidate. Keep existing metadata and reader guards;
avoid a new fingerprint framework for this local ownership fix. Build time unknown.

Perform the [waste review](api-delivery-plan.md#task-closeout-and-waste-review)
after every task or failed attempt; update this card before continuing/retrying.
Only openly licensed readers/codecs are eligible. Source308 retains its own
platform scope. Biology, hardware, license and release support remain owner choices.
