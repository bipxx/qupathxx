# Current implementation task

## Complete - simplify, organize and commit native integrations

Owner-selected cleanup is complete. The local source commit contains the pending
native label workflows, OMERO services and optional Blosc provider. OMERO CLI
parsing, preflight and execution now share one private frontend with native tests;
repeated atomic/new-only exports share one helper. New C++ is readable, label
CMake tests are grouped, and completed status detail is preserved exactly in the
existing [task history](current-task-history.md).

Decisive checks pass: four label/fluorescence/workflow/CLI suites, three OMERO
CTests, configured codec/mask ABI checks, all nine OMERO CLI routes, offline
preview reopening and both standalone command entry points. Tests use a Windows-
only PATH without Python environment variables. Scientific formatting-only
changes retain canonical source equivalence; no numeric or identity assertion
was weakened. The root Python-off configuration, local links and staged diff
checks pass. Verbatim upstream notice bytes/whitespace are preserved through a
file-specific Git attribute. All 112 preceding products remain byte-identical.

The existing integration cache builds this distinct source under
build/gui-aggregation/gap-closure/cleanup/. That directory retains the55-file
initial source snapshot, exact checks/products and cleanup receipt. Windows
MODULE outputs needed a library-output correction: the prior DLL was restored
from its verified copy, and output/PDB relocation cost one extra codec compile
pass. Targeted re-staging preserved original notice bytes after Git line-ending
normalization was detected. No engine archive, GUI, binding, SDK/wheel, release package or public/corpus
campaign was rebuilt/repeated. Git records the authorized local commit; no push
or publication is selected.

## Delivered workflow and support

- [Native labels](label-analysis.md): bounded SLIC, subcellular spots and unsigned
  TIFF masks; shared native capture, measurements, publication/undo, persistence,
  exact replay and protected exports. [Evidence](benchmarks/label-analysis/README.md).
- [Native OMERO](omero-integration.md): public metadata/previews, explicit linked
  NGFF raw blocks and original-coordinate ROI/result exchange. Public raw and4280
  point shapes have earlier source-scoped evidence; masks have native fixtures.
  IDR denies live mask pixels with HTTP403. [Evidence](benchmarks/omero/README.md).
- [Blosc provider](../plugins/blosc-bytes/README.md): existing native image ABI,
  pinned open sources, exact module identity and actual component notices.
- [Windows candidate9](windows-molecular-desktop-candidate.md): preserve its
  native/runtime/source identities and published assets unchanged.

The [format inventory](format-plugin-coverage-plan.md) and
[license policy](reader-license-policy.md) retain detailed support/admission
boundaries. Earlier receipts remain their own qualification authorities and do
not qualify changed source or instruct another campaign.

## Stop condition

Stop after the local commit with a clean working tree. Private OMERO auth/write-
back, live IDR masks, raster editing, nonidentity transforms, whole-image streaming,
desktop/bindings and packaging remain unselected. R0-R16 and the format census are
reference catalogues, not promises of full QuPath parity. Keep C++ authoritative,
Python optional and each plugin's actual license/source/distribution obligations.
