# Source/view-aware object planes — R5-PLANE1

Locally verified declared Windows workflow; all M0-M7 remain Partial.
Native product: `695158a80e9cc7ebb6d16d35437d7ab7156d2869`. Main workflow/model/desktop checks:
`774abdc46e31799fb0518370dc587a7a59492c99`. The final native delta is exactly one private
`interchange.cpp` exception-type correction. [Validation](validation.json) checks
all 452 compiler inputs, 62 installed package members and wheel RECORD.
Unchanged checks retain their actual source/configuration scope; this is not a
full platform, biological-accuracy, overall-speed or release qualification.

## Delivered behavior

| Workflow | Decisive evidence / boundary |
| --- | --- |
| Import and view | GeoJSON and pinned version3 saved data retain channel/Z/time. Live imports check actual source axes. Matching display/query/pick/selection uses the shared object plane; one document remains one series |
| Edit and publish | Explicit creation planes, parent inheritance, geometry-preserving plane identity and compound undo/redo; tissue/RGB/raw/streamed contour output retains its source plane even when another plane is displayed |
| Analyze and retain | Independent NumPy expectations for the actual selected plane, channel-specific masks, calibration, retained raw records and replay. Explicit raw requests survive display-only changes; object/source changes reject stale publication |
| Save and exchange | Sparse schema18 object-plane rows; all 17 legacy versions migrate. Save failure rolls back version/table/data. Immutable snapshots and missing-source reopen retain planes. GeoJSON body/nucleus and conditional CSV plane columns round-trip |
| Unsupported meanings | Invalid/unavailable axes refuse even with loss permission; body/nucleus and hierarchy planes must agree. Channel-specific RGB masks need a single matching display channel; mixed-Z/time 2D distances refuse. Axisless QPTMA imports mean the default plane; QPTMA export refuses non-default plane loss |

Actual regression/consumer definitions are in
[test_object_planes.py](../../../tests/test_object_planes.py), alongside existing
reader/raw/project/interchange/classifier tests. The original saved-plane fixture
still fails because its source has only Z0. The separate derived positive changes
only its synthetic source, matching declared Z count and same-length serialized
metadata scalar; original ROI bytes and pinned upstream fixtures stay unchanged.
Its existing saved-calibration-override loss is explicitly acknowledged; it is
not a new upstream capture or lossless retention of all ImageData settings.

## Results and reuse

- [Final staged probe](final-probe.xml):50 passes/two opt-in desktop skips.
- [Affected installed model checks](model.xml):921 passes,24 declared skips and
  one preserved compatibility failure. That same named case [passes](compat-probe.xml)
  on the corrected uninstalled product before its installation. There are
  **972 distinct headless passing identities** across these scoped results.
- [Nine native suites](native.xml) pass at774abdc4. The affected native
  [interchange suite](compat-native.xml) passes at695158a8.
- [Three real desktop flows](desktop.xml) pass at774abdc4: ImGui import/view/
  selection/measure/keep/save, managed hidden-plane merge and explicit-axis batch.
  [The capture](plane-view.png) was visually inspected: ROI #2, Z1/time1,
  30 valid pixels, channel means 3183/3567 and calibrated area 3.75um2.
- [Stubs](stubs.log) and [valid typing](typing-valid.log) pass;
  [invalid typing](typing-invalid.log) has13 intended diagnostics. Generated stubs
  are byte-identical across the private exception correction and are reused.
- [Migration observer controls](migration-guard-before-build.json):60 tool-only
  passes before the native builds, reused with unchanged observer inputs.

Native pyd SHA256: `7a0b444c37c5a286d08da151b9073c43b789c446a04923fad06479fe35b9363a`. Final wheel SHA256:
`bf70c1b9dadfb094ae04073bd21d2498448a10b45c410f9a27886d846433aace`. Wheels, installed environment, native interchange test
product and object files remain under root `build/object-planes-r5` and
`build/ci/object-planes-r5-env`; binaries are not committed here.

## Retained failures and waste review

[First candidate](first-probe.xml), source4265ff15:41 passes/nine failures/one
skip, never installed. Publication/explicit creation/lazy editing/QPTMA gaps were
corrected together. Three changed probe inputs [pass](first-probe-harness.xml)
on that retained product; the adjusted streamed case exposes the actual wrong
plane. The initial assumption that a display-only change invalidates explicit raw
sampling was removed; source/object-change checks remain. The derived source's
reported calibration loss and the nested contour work budget are explicit.
Initial probe observers were working files before774abdc4; preserve the logs and
commands without relabeling them as that commit's exact observer snapshot.

The later model failure remains in model.xml: a malformed GeoJSON plane index
raised IndexError instead of its prior ValueError contract. Fixing the private
parser guard preserves the existing assertion. Only that failed case and the
affected native interchange suite were repeated. All other results above keep
their source/dependency scope, verified against the one-file compiler-input delta.

Builds succeeded in791.53s,724.83s and45.03s. There were **three cached builds,
two installations and one stub generation**. No full campaign or independent
review loop was added. Two avoidable sequencing costs inform the next task:
review all object-producing consumers before freezing a shared object API, and
run older affected contracts on the staged product before installation. Reuse
unchanged stubs/products/checks; do not repair a real failure by weakening a test.
An attempted removal of four redundant extracted wheel stages was rejected
before execution as **blocked by policy**. [The copies remain](cleanup-blocked.json);
no deletion ran or was retried. Original wheels, raw logs, failed cases and source
identities remain available.

## Remaining work

R5-SOURCEAXES1 is next: existing source compatibility checks compare display
width/height/pyramid/type/layout, and reviewed replacement publishes its candidate
without comparing retained object planes against replacement raw axes. Reproduce
a reduction in raw axis counts with unchanged XY dimensions on this installed product, then extend the shared
relink/review-adoption guard while retaining existing reader handoff and no-op
behavior. This is a remaining R5 source-replacement boundary, not a reason to
repeat delivered import, recovery or property mechanisms. Other R4/R5 gaps and
R6-R16/science/resources/release remain in the single delivery queue.

The helpers here are archived as-run scripts from root `build/ci`; their recorded
paths identify the executed workflow. They are evidence, not an instruction to
repeat the completed task or dispatch another campaign.
