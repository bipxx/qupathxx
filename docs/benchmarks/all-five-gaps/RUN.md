# Windows candidate7

Engineering prerelease for local Windows x64, CPython3.12 x64. Microsoft Visual
C++ x64 runtime and OpenGL3.2 driver are required for desktop use.

This candidate includes native density manifest/run/resume services, NuCLS
RGB8 PNG/CSV import, training cohort selection, thin Python controls and exact
nominal contour optimizations delivered after candidate5, plus reviewed native vertex-touch PolygonROI import and unnamed-object retention. Scientific/application
logic stays in shared C++; independent benchmark oracles remain development code.

```powershell
py -3.12 -m venv C:/data/qupathxx-candidate7-env
C:/data/qupathxx-candidate7-env/Scripts/python.exe -m pip install "C:/downloads/qupathxx-0.1.0-7-cp312-cp312-win_amd64.whl[arrays]"
C:/data/qupathxx-candidate7-env/Scripts/python.exe -m qupathxx --project C:/data/study.qpxproj
```

Standalone: extract the native ZIP together, preserving EXEs, DLLs, fonts and
licenses. Run `qupathxx_app.exe --project C:/data/study.qpxproj`. Standalone EXEs
need no Python runtime. Use the included source documentation for density and
NuCLS command options; the Python examples are thin controls over native APIs.
Use new output paths and preserve older projects before opening with a new build.
Cross-build density reuse is not promised; changed engines can recompute.

SHA256SUMS, candidate.json, exact compiler-input source ZIP, dependency-source
bundle and notices accompany this candidate. LodePNG source/license is vendored
in the application source ZIP. All required readers/codecs are openly licensed;
application license is AGPL-3.0-only. The embedded git revision has a historical
+dirty suffix; the exact571-input source-tree hash in candidate.json identifies
the tested native bytes. Packaging does not change their provenance or rebuild.

QuPath0.7.0 saved PolygonROI vertex-touch normalization reports an explicit loss;
review it and use the existing loss-admission API/checkbox to import. It preserves
coordinates and stored measurements; arbitrary self-crossings/spikes remain fatal.

Verified scope is the local Windows install/run/remove routes and named retained
cases, including CMU-1 canonical coarse tissue, a100,001-object viewer, ten-minute
stability, actual process GPU counters, the558-cell anisotropic NDPI import and
three independent CMU-2 crops with6,732 exact cells. No fresh-host, signing, stable-release, Linux/macOS, general biological
accuracy, level-zero whole-slide cell analysis or general speed superiority claim.
The benchmark's matched ROI median is an observation, not a speed guarantee.
