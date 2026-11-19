# Native ImageJ ROI/ZIP interoperability

The local Windows x64 engineering profile uses optional provider
`org.qupathxx.imagej-roi`1.0.0, contract `imagej-pixel-roi/1`, with separately
versioned annotation ABI3. C++ owns format interpretation, source facts, plane
resolution, even-odd geometry values, identity, preview/apply, persistence and
undo. ImGui/CLI/SDK consumers share these authorities. No Java or Python runtime
implements this format. The earlier base SDK/core/runtime/wheels stay frozen.

The [provider contract](../../../plugins/imagej_roi/README.md) describes the
selected versions, geometry, text, source styles/groups/counters and ZIP profile,
and its explicit refusals and budgets. Read it before interpreting a receipt as
broader ImageJ support. Native geometry/measurement semantics apply; this does
not establish ImageJ mask/measurement parity or complete catalogue closure.

Primary schema/author declarations are pinned at ImageJ revision
`a376e43fad41a7337f7f01433c9673ed376b40e6` in the
[reference lock](../../../plugins/imagej_roi/reference-lock.json). NIH's exact
federal-duty public-domain declaration is locally identified as
`LicenseRef-ImageJ-Public-Domain`; it is not an assertion that ImageJ is licensed
under Unlicense/CC0. No ImageJ runtime or reference Java source is linked.
PKWARE6.3.10 documentation is referenced, not redistributed. The selected ZIP
codec is unchanged private prefixed static zlib1.3.2 (`Zlib`), with36 exact source
members and notice checked before compilation. Provider/native services are
AGPL-3.0-only; the separate native source overlay includes MIT JSON3.12.0 and
the public-domain SQLite3.54.1 interface source/notices. Plugin separation
does not waive actual source, notice or distribution obligations.

Evidence is under build/gui-aggregation/catalogue-closure/imagej-roi/:

- `contract-validation-01/validation.json`:28 controlled ABI3 cases for names,
  UTF-8 source facts/namespace, field/pointer/count validation, source/geometry/
  property budgets, plane resolution on real Z1/T1 and Z2/T2 TIFF axes,
  cancellation/staleness, even-odd nested contours and persistence.
- `legacy-validation-02/validation.json`:57 affected native ABI1/2/runtime/CLI
  checks, including seven unchanged checks reused after a controller baseline
  lookup correction. The original point preview SHA is exactly
  `d837016608113f4acdcc8b82df66d21178e91ee85a82e8dac3c85149a4f61450`.
- `format-validation-03/validation.json`:26 independent authored positive and37
  refused binary/archive profiles, native preview/apply/persistence/undo and
  offline reopen. Twelve native process checks include exact CLI/native preview,
  receipts, source/native IDs and persisted objects, missing explicit class/axis,
  malformed input and atomic refusal of geometry outside the target image.
  The final set includes counter0 superseding legacy2, conflicting flat/CZT point
  constraints, ZIP payload CRC/arena/ZIP64 extents and a valid unsigned descriptor
  whose CRC equals the optional ZIP signature. Initial passing stages/checks and
  source revisions are retained under their own identities.

All fixtures are independently authored, with literal expected geometry/values
and stdlib ZIP/struct oracles. Product Python does not decode ROIs or decide
plane/class/state behavior. Restricted/NC assets and reference datasets are not
qualification inputs. Passing sample profiles are not a general corpus, speed,
platform or biological acceptance claim.

The source-matched185-member directory/ZIP package includes an exact ownership/
digest table, independent module source kit and separate matching native SDK
overlay/CLI. Its `SOURCE.json` records primary module SHA
`5efb0fee122152d7198044bba5c39682551bb40e77de9920cdb634a9a1f3d2d4`, native
archive SHA `052f9f1354b87a1a107636b41198d4423c9cdd53a4c7100226c8fd9a210929e5`
and exact source/kit/dependency members. The independently rebuilt module has
its distinct SHA `820e2988e84feda3562c58b7695299e804a20882b67af23b23aacde486445b2e`.

`offline-runtime.json` records eight selected positive/six refused profiles and
twelve process checks using the final offline module and native-source CLI.
The public-only SDK consumer gives the exact primary preview, receipt and saved
objects. The unchanged native host source/header/config/archive and consumer
inputs reuse their passing offline builds through
`reused-offline-native-builds.json`. Root CMake configuration verifies the ABI3
kit/provider/test wiring with Python OFF and no Python/nanobind discovery; no
base core/runtime rebuild is part of that check.

`package-final-admission.json`, `package-zip-admission.json`, `removal-proof.json`
and `results.json` record the final directory/ZIP ownership, admission and
deployment results. The removal check imports through the deployed CLI, verifies
every owned file's absolute path/digest and inactive-process use, removes only
those files, and reopens saved user results/receipts after removal. User image,
source ROI, input/result project, report and note hashes must remain exact.

The161 prior protected products,12 source-before snapshots,18 original dirty
snapshots and earlier QOI product sources retain their own identities. This is
a new ABI3 candidate; do not transfer the prior ABI2 package's identity to these
changed sources. The [current task](../../current-task.md) owns active scope.

## ImageJ rotated rectangle import in 1.1.0

The optional `org.qupathxx.imagej-roi` 1.1.0 provider accepts freehand subtype 5
from supported ROI versions as a standalone file or Deflate ZIP member. It
uses the original binary32 centerline endpoints and width to produce one
four-corner polygon. The retained original headers and vertex bytes identify
the source shape. Source names and planes resolve through existing ABI3 rules.

The independently authored fixtures use the pinned NIH reference to compare
exact corner bytes, reversed direction, source facts, native and CLI results,
undo and save/reopen. Malformed geometry and a damaged member in an otherwise
valid archive refuse without publishing any objects. No Java runtime or other
new dependency is required. The source kit retains original NIH and Zlib notices.

The built, relocated and offline-rebuilt modules each passed 28 supported
profiles, 47 refusals and 16 native/CLI checks. The 66-member archive includes
the matching provider, format kit, exact zlib source and notices. Its removal
check preserved the original ROI and both saved projects. See the
[local Windows receipt](../../../build/gui-aggregation/catalogue-closure/imagej-roi/rotated-candidate/results.json).
