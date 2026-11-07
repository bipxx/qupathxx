# ImGui project-density delivery evidence

Completed2026-11-07: the new standalone Windows app selects images/classes,
previews native cohorts/calibration and runs project-wide density in a background
owner, with cancellation, per-image saved results and combined CSV/error report.
[Guide](../../gui-project-density.md); [14/14 coverage](../../gui-project-density-coverage.md).

Actual desktop route used a separately saved copy of the preserved public NuCLS
project. The preview shows32/28 selected Cell markers at0.2um/px; native analysis
counts31/27 centroids inside the existing annotation FOVs, with areas104006/
101124px2. Both checkpoints save and reopen; source bytes stay unchanged. These
remain annotated nuclei per annotation-FOV area, not whole-cell or tissue-mask
validation. [Original public sources/oracles](../research-density-pilot/README.md).

Evidence:

- [desktop.json](desktop.json): actual app environment/source tree, binary/source
  hashes, original/source preservation, exact checkpoint identity and successful
  reopening/native verification through unchanged installed candidate4 Python.
- [desktop-preview.png](desktop-preview.png): both image IDs, selected cohorts
  and calibration readable. [desktop-invalidated.png](desktop-invalidated.png):
  changing image selection clears the reviewed preview and disables Run.
  [desktop-completed.png](desktop-completed.png):2 saved images,0 failures,
  explicit run source, CSV/report paths and file-opening controls.
- [density.csv](density.csv), [report.json](report.json): actual desktop outputs.
  Local checkpoint paths identify retained files under main build/gui-project-density.
- [tests.log](tests.log): affected native controller suite passes in0.82s,
  including exact native CSV/persisted hashes/replay, selector/calibration errors,
  stale source, SQLite sidecars, active cancellation, CSV quota, existing output
  protection and continuing to a third image after a checkpoint collision.
  [public-controller.log](public-controller.log): separate native public-project
  route passes using the prior independent count/area oracles.
- [source-inputs.sha256](source-inputs.sha256):519 raw compiler inputs aggregate
  to23111b16241308cab967c5ae11dd5c760fe385c3c5f5446dca7a4711394c99d1,
  exactly matching the environment retained in both desktop results.
  [application-files.json](application-files.json) records56 local bundle files.

Local development application:
main `build/gui-project-density/application/qupathxx_app.exe`, about40.6MB with
its three reader DLLs, fonts, project license and unchanged dependency notices.
The reader DLL hashes match published candidate4. This is a new source-scoped
desktop consumer; its binary is not substituted into any frozen release/wheel.
Its build recorded a9edd64e+dirty; the exact519-input tree above is authoritative
and is included with this public source change. No clean-tag or fresh-host result
is claimed. Windows/MSVC runtime and OpenGL prerequisites remain as previously
documented; no new host/platform support is inferred from this local run.

The results-folder button opened Explorer. Open CSV reached Windows' application
chooser because no CSV viewer was assigned; the chooser was dismissed without
changing associations. The panel delegates file opening to the OS, rather than
installing or selecting a default application during qualification.

One controller fixture initially compared a current replay's revision-bearing
SHA to its historical result. The repair retains exact stored identity and uses
native numeric/geometry verification for replay. A receipt observer initially
used WindowsPath's case-insensitive sorting for the compiler manifest; sorting
POSIX relative strings exactly like CMake fixed it without any product rebuild.
Raw failures remain in main build/gui-project-density/controller-tests.log and
verify.log. GUI-only refinements reused passing kernel/controller evidence and
cached objects. No binding, wheel, dependency install, download, segmentation,
unrelated platform/corpus suite or full native qualification was performed.

Reproduce controller checks with the guide's cached build/test commands. The
controller test executable also accepts `source.qpxproj new-output-directory`
for the pinned two-image public project. After executing the documented desktop
route, [verify.py](verify.py) checks the local bundle and actual outputs and writes
a new receipt/source manifest into main build/gui-project-density. It does not
automate UI, build, download data or repeat numerical qualification.

No resume control is added to this panel; the separate Python runner retains its
existing resume workflow. No new kernel, SDK schema, clinical/biological acceptance,
whole-cell/tissue segmentation, speed comparison or public binary release is
claimed. This selected outcome is closed; further scope needs a concrete user need.
