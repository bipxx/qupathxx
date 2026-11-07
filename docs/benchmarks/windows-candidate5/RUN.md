# qupathxx Windows candidate5

Signed Cell-to-annotation boundary distances through C++, Python, CLI and ImGui,
plus the delivered project-density panel with previous-run resume/reuse.
This is a Windows x64 engineering prerelease. Python support is CPython3.12 x64;
desktop use requires the Microsoft Visual C++ x64 runtime and OpenGL3.2 driver.

Use a new environment; candidate wheels share version0.1.0:

```powershell
py -3.12 -m venv C:/data/qupathxx-candidate5-env
C:/data/qupathxx-candidate5-env/Scripts/python.exe -m pip install "C:/downloads/qupathxx-0.1.0-5-cp312-cp312-win_amd64.whl[arrays]"
```

For standalone use, extract the whole native ZIP. Keep EXEs, DLLs, fonts and
licenses together; native executables need no Python installation.

```powershell
qupathxx_app.exe --project C:/data/signed-example.qpxproj
```

Open Spatial..., select saved record6, Inspect and Verify. The synthetic example
contains three Cell centroids and a rectangular Annotation, with independent
X/Y calibration2um/.5um. Signed distances are **-5,10,0um**: negative inside,
positive outside, zero on the boundary. No segmentation or biological claim.

```powershell
qupathxx_batch.exe --verify-spatial C:/data/signed-example.qpxproj --image 1 --record 6
qupathxx_batch.exe --export-spatial C:/data/signed-example.qpxproj C:/data/new-distances.csv --image 1 --record 6
qupathxx_batch.exe --measure-spatial C:/data/signed-input.qpxproj C:/data/new-result.qpxproj --image 1 --operation signed-boundary --units um --source 2 --source 3 --source 4 --target 1
```

See signed-distances.md for holes/multipart, overlap/tie rules, centroid policy,
calibration, explicit curve approximation and quotas. New signed records use
QSD2; older published candidates cannot open projects containing those records.
Existing QSD1 records remain readable with exact stored identity. Loading performs
structural checks; explicit Verify recomputes the complete nearest search.

Project density: open a saved project, choose Project density..., preview explicit
classes/images, then run to a new output folder. A previous-run folder can reuse
eligible unchanged images. Different engine builds recompute; old candidate4 or
pre-receipt runs are not promised cache reuse. The density guide ships alongside.

SHA256SUMS covers the release assets. Matching application source, pinned open
dependency sources and notices accompany both packages. Project license is
AGPL-3.0-only. Support is local Windows x64 install/run/removal and static package
dependency closure; no fresh-host, signing, additional-platform, full QuPath
parity, indexed-search speed or biological validation claim. Earlier candidates
remain separately available.
