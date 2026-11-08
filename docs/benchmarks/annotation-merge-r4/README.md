# R4-MERGE1 annotation-object merging

Native source `acbdc3884a4bab3d99c62e383e9e42de89c79523` adds a shared atomic
merge command to Document, ViewerSession and ImGui. Observer
`64acd1305fc585ea77890b2487e11620ff90dc92` publishes generated stubs and final
consumer/typing checks. Nine of435 compiler inputs differ from R2-CODEC1;
the fingerprint/native/wheel identities remain distinct in [validation](validation.json).

The [contract](../../geometry-editing.md#merge-annotation-objects) describes new
identity, class inference, preserved children, strict admission and history.
The existing Boolean operation retains its behavior and enum values are appended.
There is no new project schema or duplicate per-frontend object model.

The [pinned reference](../../../tests/fixtures/qupath-annotation-merge-0.7.0/manifest.json)
captures ten synthetic QuPath0.7.0 mergeAnnotations workflows using the verified
runtime and [probe](../../../tools/upstream/qupath_annotation_merge.groovy).
Inputs, geometry, fresh identity, class/name/measurement outcomes, surviving
objects/parents and selected identity are retained. It covers same/mixed/empty
classes, classified plus unclassified, disjoint areas, points, mixed dimensions,
surviving children, nested selections and a singleton. This is object-workflow
engineering evidence, not biological or general geometry acceptance.

QuPath's mixed-dimensional union drops point geometry, including outside points;
the native command requires explicit discard_points for that behavior. It also
rejects unsupported kinds/duplicate IDs instead of silently filtering them.
The native default name is Annotation rather than QuPath's unset name. Surviving
direct children move to root, matching the capture; undo restores original
parents and redo restores the same merged identity. Selection remains separate
presentation state, with controllers selecting the result and undo pruning it.

Evidence:

- [Before](before.xml): one capture-integrity check passes; all ten merge
  workflow checks fail because the prior installed SDK lacks the operation.
- [Model](model.xml):74 passes,15 declared skips, covering the selected geometry,
  edits, selection, project and managed submission surfaces. The skips include
  three new desktop variants plus unrelated existing desktop opt-ins.
- [Desktop](desktop.xml):four passes: the three new merge variants (selection,
  undo/redo and managed stale/save/reopen) plus existing Boolean compatibility.
  Combined by named identity, all24 new merge cases pass. Native model tests
  independently check masks/areas, metadata, hierarchy, rejection atomicity,
  next-ID preservation, history budgets and source-project persistence.
- [Native](native.xml):geometry_edits, viewer_session and selection pass,
  including actual merge/child/point/history and managed stale/result/selection
  markers in system-out. A local proof initially looked for these in the concise
  ctest console log; reading the retained XML fixed the proof without rerunning
  the successful suites.
- [Typing](typing-valid.log) accepts the additive APIs;
  [negative typing](typing-invalid.log) rejects exactly four intended diagnostics
  on three statements. The [installed stub check](stubs.log) passes.
- [Identity/package](validation.json):all62 installed members match the final
  wheel; RECORD hashes/sizes verify. Stubs were generated once from a staged full
  copy of the exact new wheel, then repackaged without changing native bytes.
  The final wheel was installed once. Earlier JPEG/TIFF archives remain unchanged.
- [Inspected UI capture](merge-0.png):one selected Annotation with the merged
  outline and Tumor class. It is a lossless conversion of the retained PPM
  capture, not another rendering run.

Waste review: reused immutable union, the command journal, selection, project
schema and consumer controllers. Mixed-input UI detection is revision-cached;
changing the selection/document clears point-discard acknowledgement. One cached
seven-target build was necessary; changing the shared public header caused a
broad core/binding rebuild. Its total elapsed cost was not captured. API changes
were settled together; generated stubs required only byte-preserving repackaging,
not a second native build or intermediate installation. Selected model/native/
desktop durations were2.10s/2.20s/4.04s. No passing tests, reader campaign,
standalone numerical comparison per frontend or full platform matrix was repeated.
Keep future observer fixes on the retained product and use explicit cache_path
when checking retained external archives. No generic efficiency framework added.

This is local installed Windows engineering verification. Source308 retains its
own platform scope; all M0-M7 and broader R4/R5 requirements remain Partial.
Next: R5-QPPROJ1 direct QuPath project-manifest import. Pin actual qpproj/qpdata
examples, reuse image adapters, loss-aware GeoJSON exchange and project snapshots,
and define preview/apply losses for unsupported builders, missing sources, planes
and annotation payloads. Direct qpdata decoding remains a distinct R5 contract;
manifest-only import cannot be presented as full project migration.
