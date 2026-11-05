# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. [PROJECTMETA1](benchmarks/project-metadata-r5/README.md)
is locally verified for its declared project-wide string-map workflow.

Current: **R4/R5-CLASS1 — class ancestry and project defaults** (in progress).

- Outcome: preserve recognized derived-class parent links and project-default
  classes, inspect/edit and save/reopen exact state, including zero-image projects.
- Evidence: qupath_saved_data.cpp emits classification_hierarchy_flattened;
  qupath_project.cpp emits project_classes_dropped when class defaults have no
  image to receive them. Entry/global metadata do not represent a class registry.
- First action: inspect ObjectClass/Document class commands/history, current
  saved-data classification and project class-list/GeoJSON wire contracts against
  pinned PathClass APIs. Establish identity versus display name, parent links,
  colors, duplicate/collision handling and rename/remove/undo before API changes.
  Never infer topology by splitting a display name or merge ambiguous classes.
- Route/reuse: extend the current class authority, importer and native history;
  expose thin owning Python/UI paths and normalized storage/snapshots. Settle
  compatible ancestry/default-registry contracts together before another public
  layout/binding edit. No parallel hierarchy model or second decoder.
- Missing reference: one class-only supplement using retained builders/geometry,
  with parent/sibling/deep chains, literal delimiters/Unicode and project defaults
  with zero images. Existing pixel/geometry/math evidence retains its scope.
- Checks/stop: exact class identity/parent/color and object association through
  import/edit/undo/save/reopen, project/entry boundaries, invalid cycles/orphans/
  collisions/refusals, owning state/snapshots/migration and affected actual GUI/SDK
  flow. Stop after this declared vertical workflow and hand off remaining gaps.
- Cost/dependencies: settle interfaces/consumers first, one grouped cached build,
  one stub/install after APIs settle; repeat only for changed input or unresolved
  risk with a recorded diagnosis. Engineering is ready independently of biology,
  license and support choices. Only openly licensed readers/codecs are eligible.
  Broader R4/R5 and R6-R16/milestone exits remain queued.

Latest task review (2026-11-05, PROJECTMETA1 closeout):
Outcome/evidence: source408f383e retains project maps with owning/atomic SDK,
shared validation, schema25, budgets, recovery/adoption and project-mode editor.
249 unique affected non-GUI cases pass with nine declared GUI skips; five native
roles/all24 older schemas, two actual desktop flows, two installed ownership/
budget/recovery/namespace checks and five stubs/typing controls pass. Installed
mouse remove/Apply/close/Ctrl+S/reopen verifies exact zero-image state; screenshot
inspected. All467 compiler inputs/65 installed hashed members match. Final Python
observer and SDK hashes remain separate; see the linked receipt/failures.
Waste/action: **reuse** one project-only2.77s capture, existing builder/source,
string validation, snapshot/store/editor, old affected selection and passed cases.
One557.16s grouped build, one stub pass and one install; no native repair. Fix
only failed observer setup/expectations: absent classifiers directory, correct
.qpxrecover suffix, held TIFF replaced by an absent manifest URI, exact
missing_image issue. Retain first failures; rerun only failed nodes. Avoid guessed
paths, spacing-sensitive bulk edits without preflight and repeated short compiler
polls; use discovered APIs and bounded live-session waits. No numerical/full
qualification campaign or alternate model/codec/editor was added.
Next: CLASS1 contract/reference inspection above; group related class contracts
before another expensive public-header/binding build. Full parity, biological
acceptance, whole-slide resources/speed, hosts and release remain open.

After every bounded task or failed/stopped attempt, record its scoped outcome,
waste action and changed next step here before continuing or retrying.

CLASS1 inspection review: current ObjectClass is name/RGB only; class registry
and object links already share native staged history. QuPath PathClass stores
explicit parent/leaf state and caches by a path, while default leaf names reject
colon/newline. The guessed standalone PathClassTypeAdapter URL failed and the
web tool could not open the repository tree API. Waste/action: discover adapter
locations from the pinned upstream tree rather than guessing more filenames.
No build or native edit; first settle wire/identity/null-color/reference behavior.

CLASS1 contract checkpoint: GeoJSON has explicit names arrays; project classes
use a pinned delimited name wire format whose reader calls PathClass.fromString.
That format-specific decoding is distinct from guessing ancestry in native or
arbitrary display labels. Palette order/duplicates, NULL sentinel removal and
nullable/ARGB colors need an actual reference. QuPath interns classes by path,
so one producer plus a fresh-process readback is necessary to detect cache-biased
color precedence; this is not a repeated numerical/corpus campaign. Reuse the
saved ROI/builder with lazy PathIO. No native header/build before these facts.

CLASS1 reference review: producer and fresh readback pass for29 retained files.
Palette order/repeated entries survive, NULL sentinel is omitted, explicit ARGB
survives, project palette wins older native/GeoJSON colors, and cleared color
becomes a generated default on fresh reload. Colorless GeoJSON fails upstream.
Old408f383e rejects valid duplicate/ARGB/absent-color palettes, flattens four
ancestry links without defaults and refuses names-array GeoJSON; baseline saved.
Waste/action: retain these references and both process scopes; no image/math
recapture or big native build. One small default-color oracle is now necessary
to match the observed fallback, including UTF16 hashing and parent-dependent
colors. Implement that shared primitive before freezing class fields/APIs, so
absence can remain distinct from resolved display color without a later ABI edit.

CLASS1 primitive checkpoint: five capture/identity/semantics tests pass. One small
extra process supplies38 default-color cases (Unicode UTF16, normalization,
standard/derived/Tumor/ARGB parent cases and a dark first random triplet), plus
fresh-cleared-color readback gives a39th native expected value. Implemented one
internal resolver; public ObjectClass/Project/Document APIs are still unchanged.
Waste/action: a targeted classes observer build is sufficient for this primitive;
no wheel/stubs/install or numerical campaign. Next: freeze/run that role, then
integrate explicit ancestry, raw/resolved colors and ordered project defaults
through the documented single class/history/import/storage authority.
