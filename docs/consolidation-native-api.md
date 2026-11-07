# Native workflow contract

NC04 contract for the shared density/import services. The [checklist](consolidation-coverage.md)
retains consumer and installed integration work; [native evidence](benchmarks/consolidation/README.md#settled-native-workflow-contract)
proves the behavior below at the C++ authority. No new binary delivery is implied.

## Ownership and controls

ProjectDensityJob and NuclsImportJob belong to their construction thread. Submit,
poll, result/error/progress access, event drain/acknowledgement, control failure and
finish require that thread. Cross-thread cancel is allowed. Destruction always
cancels and joins, including destruction on another thread; it does not invoke
owner-only methods. One operation is active per job. Finish cancels, joins and
collects the terminal state; rethrow_error separately delivers the saved exception.

Submissions validate and own their native arguments before dispatch. Explicit
movable captures avoid a second deep copy of const recipe/options/report arguments.
Workers open/create their own native projects/documents and never retain Python
objects or callbacks. Loaded manifests share an immutable native implementation
and owned source-version snapshot. Returned job reports are shared const snapshots;
resubmission clears the job's current result without invalidating retained snapshots.

Density preview and class-manifest generation use the same selection authority.
Generate through ProjectDensityJob.generate_manifest and inspect manifest_result;
preview/execution use result. Both futures participate in the same busy/cancel/join
contract. Direct synchronous SDK functions remain available. A successful preview
is a validated value, not a capability token: execution rechecks the saved source
and captures every scientific request through SpatialDensityRequest.

Progress events contain owned native JSON. Optional pause_after_image/pause_after_pair
allows the caller to inspect an event before acknowledging further work. No growing
report is copied per event. Control failure after an observed paused event is sent
through abort_control; density publishes a partial historical report, rather than
turning a callback failure into cancellation or false completion. Empty control
errors receive an explicit failure message. Cancellation is cooperative between
bounded native stages and directory/file chunks; it cannot interrupt an OS call
or arbitrary third-party topology operation in the middle.

## Failure delivery

Jobs keep exception_ptr and diagnostic text. Poll does not erase type information;
rethrow_error rethrows on the owner thread. Per-image density failures and NuCLS
input issues remain native report results under their existing schemas. Setup,
integrity and publication failures remain exceptions.

| Native failure | Consumer mapping to implement/check in NC19-NC21 |
| --- | --- |
| WorkflowInputChanged | Dedicated changed-input exception |
| WorkflowOutputExists | FileExistsError |
| ProjectDensityCsvBudget | ValueError-derived CSV budget exception |
| WorkflowCaptureLimit / invalid_argument / length_error | ValueError-derived admission failure |
| WorkflowCancelled | Explicit setup cancellation; do not fabricate a completed report |
| filesystem_error | OSError |
| Owner-thread logic_error / other runtime_error | RuntimeError |

Cancellation after work begins retains density's completed historical rows and
checkpoints in a cancelled report. NuCLS cancellation never publishes a project.
A cancellation during setup may end with WorkflowCancelled and no report/output.
Existing report/receipt dialects, reuse and scientific numeric/export identities
are preserved by native serializers and requests.

## Admission and bounds

These are logical payload/workflow bounds, not a measured RSS or timing guarantee.
Native scientific requests and Project.save_new retain their own exact admission
checks. Resource refusal never advertises a partial scientific output as complete.

| Surface | Bound / authority |
| --- | --- |
| Captured operational paths | Nonempty where required, no NUL, at most32768 bytes before worker copies; filesystem normalization/identity remains native |
| Density recipe |128 MiB logical captured payload, plus existing1..1000 images, exact selector/ID/role bounds |
| Density preview/report capture |256 MiB conservative recipe/rows/strings charge; validate before worker copy; native per-image science admission still applies |
| Density manifest |16 MiB encoded strict JSON; existing schema bounds fit the aggregate recipe/report allowances |
| Generated manifest preview |64 MiB JSON report; prepare/admit before optional no-replace manifest commit |
| Density output | Existing CSV/source/checkpoint limits1 byte..1 GiB; report16 MiB; existing native snapshot/work limits unchanged |
| NuCLS selection | At most100 exact-case stems; filter explicit subsets and reject oversized default inventory while scanning |
| NuCLS retained import | capture_bytes defaults/max512 MiB, lowerable to1 MiB; cumulative RGB/object/geometry/property and bookkeeping charge uses native payload sizes plus conservative node overhead |
| NuCLS transient history | Disabled on worker-created import documents; undo history is not imported scientific data or persisted by this workflow |
| NuCLS sources/scratch | Existing32 MiB PNG,16 MiB CSV,16M aggregate pixels,100k rows,1M vertices,10000 polygon vertices; decoder128 MiB per allocation and bounded inflate; CSV field131072 characters / row64 MiB |
| NuCLS publication/report | Native snapshot/file limits, report max16 MiB, all admitted before atomic save-new; all blocking pair/aggregate issues prevent publication |
| Events/control diagnostics |1000 events,16 MiB queued JSON, bounded8 KiB control-error input; draining frees queued storage |

For bindings, hold the GIL while validating/copying mutable Python option objects
into a job. Workers already run without the GIL. Release it for waits/joins and
appropriate synchronous native work after owned capture. Return copies of nested
mutable options/recipes from immutable manifest/result snapshots. These consumer
obligations are checked in NC19-NC26, not inferred from the native tests.
