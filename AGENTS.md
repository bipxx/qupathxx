# Working on qupathxx

## Result fidelity and scope

The owner explicitly does not require100% QuPath parity. Deliver useful selected
research workflows; feature-matrix gaps and R0-R16 are a reference catalogue,
not a promise to implement everything. Choose additional work only for a real
user need or demonstrated correctness/usability blocker. This supersedes older
instructions to preserve the entire parity target or close every M0-M7 row.

Small visual differences in fonts, spacing, icons, themes, antialiasing and layout
are acceptable when they do not affect results or use of the workflow. Do not
chase screenshot/pixel/layout equivalence with QuPath. UI checks should establish
readable values, usable controls and correct geometry/overlay registration.
Fix misleading colors/channels/scales, clipped result values, hidden errors or
misaligned masks when they can affect interpretation or decisions.

Keep scientific inputs, calibration, measurements, result identities, persistence
and export/replay correct under the workflow's declared contracts/tolerances.
Cosmetic tolerance does not authorize changing raw pixel data, weakening numeric
checks or silently relaxing exact stored identity. Classify screenshot checks by
purpose; retain those proving meaningful behavior, not cosmetic sameness.

## C++ owns application logic; Python controls it

Owner decision: all shipped scientific and application/workflow logic belongs in
C++. Python is a simple control layer: parse arguments, construct typed native
options, submit native operations, poll/cancel jobs and display native reports.
Put segmentation, geometry, measurements, selection/cohort rules, identity/reuse
checks, import interpretation, persistence and result export in shared native
services. ImGui, CLI and nanobind must call those same authorities.

Plain CMake builds default QUPATHXX_BUILD_PYTHON=OFF. Python packaging explicitly
opts in; native GUI/CLI/workflow targets must configure, build and run without
Python/nanobind discovery or Python runtime dependencies. Do not shell out to
Python as a native implementation. Keep new application-owned entry points and
pipeline recipes in shared C++ services callable without the bindings.

Do not add Python per-pixel/per-object analysis or duplicate recipe/reuse/state
machines. Density/manifest/NuCLS/cohort consolidation is delivered; desktop startup
and the complete raw-classifier recipe now have shared native services. Reuse
[the boundary audit](docs/consolidation-python-boundary.md) and its source-scoped
native-only evidence rather than reopening completed migrations.
Independent test/reference oracles and development benchmark analysis may remain
Python; they are not product fallbacks or an alternate scientific authority.
Keep native file/data validation at its actual trust boundary. Python argument
parsing never replaces native validation. Release the GIL for native work and
use owned snapshots/typed results rather than callbacks in hot loops.

## Deliver the selected user workflow

Read [current-task.md](docs/current-task.md) and the
[Windows checkpoint](docs/api-delivery-plan.md#next-delivery-checkpoint-windows-research-workflow)
before implementation. They own current state and delivery scope; the R0-R16
inventory records optional expansion opportunities. Historical packets and receipts are
evidence, not instructions to replay campaigns. Follow newer user steering.

- Keep one in-progress user outcome. For the selected checkpoint, finish open
  the workflow named in current-task.md using existing native authorities.
  Candidates2/3/4/5, the thin Python project-density runner and class-based
  manifest generator and standalone NuCLS importer are delivered.
  The ImGui project-density panel, including resume/reuse, is delivered in a
  separate standalone app;
  the published candidate4 wheel/SDK and existing Python consumers stay frozen.
  Signed Cell-to-annotation boundary distances are delivered separately through
  the existing distance pipeline and their own staged runtime/source receipt.
  Windows candidate5 publishes that workflow and the project-density/resume panel
  with matching wheel/native/source assets and local install/run/removal evidence.
  Preserve the frozen native runtime. No new kernel, binding rebuild, batch UI,
  wheel or further feature phase is selected without a new owner need.
- Pull new work into that outcome only for a demonstrated blocker, correctness
  failure or invalidated evidence. Put other findings in their existing R row.
  Park optional PRESET1 consumers unless this workflow requires named presets.
- Start with existing APIs and source-scoped receipts. If they already satisfy
  the behavior, reuse them and advance; do not rewrite a delivered mechanism.
- Keep the passing MODEL1 seed distinct from later active source. Changed code
  requires its own candidate identity; do not transfer old qualification to it.
- Stop when the declared outcome and applicable delivery conditions are met.
  Report remaining scope. Do not automatically start another parity extension
  or claim full project, biological or speed acceptance from this checkpoint.

## Control build and validation cost

- Before an expensive operation, name the changed inputs or unresolved risk and
  decisive check in the current card. Reuse compatible evidence; check an
  existing session/result before restarting. A failed command needs a diagnosed
  cause and changed action before retry, not another identical campaign.
- Settle known native/consumer interfaces before one combined cached build,
  binding/stub generation and install where changes require them. Do not package
  each intermediate substep. Keep SDK, UI and CLI thin over the same native logic.
- Follow the [validation policy](docs/api-delivery-plan.md#validation-and-build-policy).
  Order cheap changed CLI/frontend targets before the large binding target in
  a combined cached build. Preserve passing binding objects for later consumer
  repairs; rebuild/relink only affected inputs.
  Documentation needs link/diff checks, not product builds. Use affected native
  tests for changed kernels and missing installed-route checks for consumers.
  Full platform/corpus/qualification runs require a release checkpoint or named
  concrete risk. Do not replay accepted qualification308 to refresh status.
- Never save time by weakening assertions, hiding failures, dropping required
  validation or relaxing exact persisted identity without a justified contract
  decision. Numeric closeness alone does not establish model identity.
- After each task or failed attempt, update the existing current card with the
  outcome, decisive result, actual waste/correction and next action in a few
  lines. No separate review campaign, approval ritual or duplicate cost ledger.
  Append only durable new lessons to compilexx's canonical notebook.

## Keep the workspace small and recoverable

Use one active integration checkout/cache/runtime; create temporary comparisons
only for a named need and retire them at handoff. Preserve passing products,
fixtures, durable evidence and failures. Before recursive cleanup, verify absolute
paths, source/worktree boundaries and active process use. Never delete `build`
wholesale. Preserve unique commits and verified dirty-source snapshots before
retiring checkouts. Keep cleanup archives until an explicit retention decision.
Do not regenerate retired products merely because a historical receipt names them.

Readers and required codecs must be openly licensed. Keep owner scope/license/
support/publication decisions attached to their actual claims and continue
independent authorized work; do not invent additional permission gates.
For new fluorescence/spatial-omics work, the owner additionally requires open
formats: use the selected formats in docs/fluorescence-omics-plan.md and the
format admission policy. An open-source reader alone does not admit a closed
vendor format. Keep implementation native and Python optional.

The eight-gap follow-up is planned in docs/feature-expansion-plan.md; its22
items do not reopen the completed fluorescence/omics slice or replace active UI
work. Keep proprietary and NC material in distinct external pools/metadata
registers (docs/licensing/restricted-materials.md), outside open dependencies,
fixtures, model training, package inputs and release acceptance evidence. An open
wrapper/container does not license restricted contents or derivatives. Unknown
terms are unreviewed, not admitted; separation does not authorize a closed adapter.
