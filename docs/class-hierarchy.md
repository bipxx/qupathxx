# Class ancestry and project defaults

CLASS1 is in progress. The [reference/default-color preflight](benchmarks/class-hierarchy-r4/README.md)
is verified; the captured reference and default-color primitive do not
establish native hierarchy/palette support. The current product still flattens
native ancestry and refuses names-array GeoJSON and several valid class files.

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

Implementation route (public model changes remain pending):

1. Extend the existing owning ObjectClass with explicit parent and optional raw
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
3. Extend existing Document staged class history for definition/bulk registry
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
5. Extend the existing binary decoder and common GeoJSON reader for explicit
   class paths/parent definitions. Use project-context precedence for known
   palette colors, while standalone GeoJSON honors its own definitions. Support
   signed/unsigned ARGB and absent colors. Retain legacy flat wire behavior;
   native extension data must disambiguate native flat keys from explicit paths.
   QuPath project path decoding follows its pinned format, not a general display
   heuristic. Keep full native binary decoding single-authority.
6. Store document semantics sparsely alongside legacy RGB rows and normalize
   project definitions/default positions in one schema migration. Extend only
   affected snapshots/history/SDK/GUI routes and legacy downgrade helpers. Reuse
   current class controls for document editing and the same values/validation
   for project defaults; GUI drafts are not another model or validator.

Settle constructor/method signatures and native/foreign wire versioning before
one grouped cached candidate. Python currently exposes Document.classes as an
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
