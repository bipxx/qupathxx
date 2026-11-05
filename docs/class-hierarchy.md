# Class ancestry and project defaults

CLASS1 is locally verified for its declared C++/Python/ImGui workflow at runtime
8722d96d. See the [installed closeout and source scopes](benchmarks/class-hierarchy-r4/README.md#installed-workflow-closeout-2026-11-05).
Broader R4/R5 workflows and dense history resources remain open; all M0-M7 stay
Partial. The numbered contract below describes the delivered authority, not
instructions to rebuild it.

Pinned0.7.0 facts from the class-only capture and fresh-process witness:

- PathClass has an explicit leaf and parent. Derived GeoJSON uses a names array;
  project class-list names are an encoded path read by PathClass.fromString.
  Only that pinned wire field may use its declared delimiter. Native flat names
  and arbitrary display labels never imply parent links.
- Project palettes preserve order and repeated entries. Their registry is unique
  by path; palette selection/order is separate. The NULL sentinel is omitted by
  the source writer. Root class leaves containing colon/newline are rejected.
- Explicit32-bit ARGB survives native/GeoJSON; byte patterns must remain distinct
  from the RGB view. Palette colors override older native/GeoJSON colors for the
  same path when loading a project, as the fresh-process reference demonstrates.
- A cleared color is absent in the project file and raw saved data, but a fresh
  QuPath reload assigns a default. Its colorless GeoJSON cannot be read back by
  QuPath. Preserve raw absence separately from resolved display color; do not
  label a same-process singleton-cache value as a saved-state reference.

Implemented contract:

1. The existing owning ObjectClass carries explicit parent and optional raw
   ARGB, retaining its existing name key and RGB view for compatibility. Known
   RGB definitions keep their old behavior. Resolve absent colors through one
   shared primitive, using explicit paths/parent context and independently
   captured default-color outputs. Never collapse None into a known raw color.
2. Keep canonical qualified keys for recognized QuPath paths and explicit parent
   links. Preserve legacy native flat keys, including delimiters, as flat. A
   distinct root/derived identity collision during foreign merge must refuse
   atomically; it must not silently reinterpret existing classes. Validate full
   parent closure, key/parent consistency, depth/text limits and RGB/ARGB state
   at publication/load boundaries.
3. Document uses its existing staged class history for definition/bulk registry
   edits and subtree rename. Update class equality, payload budgets, snapshots
   and native restoration once. Rename follows actual parent edges and remaps
   object associations in one transaction; it must not match unrelated string
   prefixes. Removal rejects used classes or surviving children. Color updates
   preserve ancestry. Existing classify/geometry/lock/probability commands remain
   the authorities; no second hierarchy or parallel edit journal.
4. Project owns the same ObjectClass definitions plus an ordered list of default
   keys (duplicates allowed), with owning getters and one atomic registry/default
   replacement. A Project default edit does not silently rewrite Document state;
   applying defaults to an image is explicit and uses one native class command.
   Include empty projects and snapshot/budget/recovery/workspace adoption.
5. The existing binary decoder and common GeoJSON reader retain explicit
   class paths/parent definitions. Use project-context precedence for known
   palette colors, while standalone GeoJSON honors its own definitions. Support
   signed/unsigned ARGB and absent colors. Retain legacy flat wire behavior;
   native extension data must disambiguate native flat keys from explicit paths.
   QuPath project path decoding follows its pinned format, not a general display
   heuristic. Keep full native binary decoding single-authority.
6. Document semantics are sparse alongside legacy RGB rows; project definitions/
   default positions use schema26. Snapshots/history, legacy migrations and thin
   SDK/GUI consumers retain the same values. Shared class controls edit image
   definitions and project defaults; GUI drafts are not another model/validator.

The constructor/method signatures and native/foreign wire versions are delivered
through the installed candidate. Python exposes Document.classes as an
owning list of ObjectClass values; use that established shape. The project
registry/default list must remain distinguishable so ancestor definitions do
not silently become extra palette entries. Retain duplicate palette entries,
but refuse inconsistent duplicate registry definitions.

Verification: captured paths/colors/associations and palette order, empty-project
import/edit/save/reopen, history/subtree rename/used-child refusal, namespace and
ownership, old schemas/corrupt rows/atomic failed writes/source guards, actual
Python/GUI workflows and installed SDK/typing. Reuse retained geometry/pixels and
unchanged numerical comparisons. A primitive check alone does not close CLASS1,
M3/M4, broader color/display behavior, scientific/resource/host or release scope.


Native foundation checkpoint: [source be834edf](benchmarks/class-hierarchy-r4/foundation-scope.json)
passes classes/project/save/spatial roles. ObjectClass now has explicit parent,
ARGB presence/alpha and a resolved RGB view. C++ definition/registry/path APIs,
sparse staged history and subtree rename, Project registry/ordered defaults and
schema26 sparse document semantics are present. Legacy flat RGB rows need no
semantic row. Stored unknown-color RGB views must agree with the reconstructed
graph. Default edits are Project-only; explicit application records one Document
edit. Palette repeats are retained but affected definitions are applied once.
The model/header is settled for the following integration; this is not a passing
Python/GUI or class interoperability claim.

Next implement the common reader/writer and existing binary decoder, preserving
explicit paths/parent closure, native flat identity, raw ARGB/absence and project
color precedence. Add the thin ObjectClass/Document/Project SDK and existing
class controls, then exercise the complete installed workflow. Cover new raw
class snapshot/recovery/budget/fault cases there rather than repeating unrelated
science, reference capture or native tests without changed inputs.


Native GeoJSON substep at de8f9889 is verified in the [wire scope](benchmarks/class-hierarchy-r4/wire-scope.json).
Version1 flat RGB output remains compatible. Version2 stores every selected
class's explicit parent, resolved RGB and nullable raw ARGB, including ancestor
closure; feature names arrays represent derived paths. Raw absence is retained
in metadata while the feature projects a readable resolved color. Native
metadata disambiguates literal flat keys and delimiter-bearing native leaves.
Import rejects identity/raw-cache conflicts atomically. Binary/project palette
integration and Python/editor exposure remain pending.


Binary/project substep at3d5156f2 is verified in the [project wire scope](benchmarks/class-hierarchy-r4/project-wire-scope.json).
The existing binary decoder emits explicit parent/raw definitions through the
same versioned GeoJSON route. Project palette context overrides older colors
without changing flat/derived identity. Unique definitions include required
ancestors; palette keys preserve order/repeats and empty projects. Pinned project
names alone use colon decoding and Java-strip whitespace. Repeated conflicting
definitions and incompatible identities refuse. Native and v1-assisted fixture
imports/save/reopen pass; installed SDK/editor and new-field fault checks remain.


Installed closeout: [source/package integrity](benchmarks/class-hierarchy-r4/candidate-integrity.json),
[new-field runtime checks](benchmarks/class-hierarchy-r4/staged-repair.xml),
[SDK](benchmarks/class-hierarchy-r4/installed.xml) and
[actual desktop readback](benchmarks/class-hierarchy-r4/editor-readback.json)
verify the complete declared route. Earlier checkpoint pending statements are
historical source scopes. The remaining class-only rename geometry/history
admission concern belongs to the next R4/R12 task, with a measured probe before
changing its journal. No broad performance/parity claim follows from CLASS1.
