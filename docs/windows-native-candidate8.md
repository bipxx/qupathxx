# Windows native engineering candidate8

This candidate delivers the selected native fluorescence/spatial-expression
workflows, desktop export/review improvements and relocatable C++ SDK. It is a
Windows x64 engineering prerelease, qualified on the named local Windows host.
Its validation and removal checks are in the
[deployment receipt](benchmarks/windows-native-candidate8/README.md).

Use the four matching assets for `v0.1.0-windows-candidate.8`:

- `qupathxx-windows-x64-native-candidate8.zip`: desktop application, batch/region
  CLI, three required native DLLs, fonts, UI resources and notices.
- `qupathxx-windows-x64-sdk-candidate8.zip`: C++ headers, Release libraries,
  prefix-relative CMake targets, runtime DLLs, notices and a standalone example.
- `qupathxx-windows-x64-source-candidate8.zip`: exact corresponding application
  sources, build recipes, selected small authored oracles and source identity.
- `qupathxx-native-dependency-sources-candidate8.zip`: fifteen reviewed native
  dependency source artifacts with original component notices.

[Artifact hashes](benchmarks/windows-native-candidate8/artifacts.json) bind these
assets to native source SHA256
`68bf2d43fbbb0bd0bcfac19f598bbca91496e737f3c50d5d223ae1905d000be0`.
The native build records Git39823dee+dirty. The release Git commit also includes
later validation tooling/docs; the source ZIP and805-input manifest retain the
exact compiled bytes independently of Git text/newline normalization. Preserve
the release manifest and checksum list with the downloaded files.

Extract the application ZIP into its own new directory and launch
`qupathxx_app.exe --empty`, or open an existing project with
`qupathxx_app.exe --project study.qpx`. Keep projects, matrices, fitted `.qpr`
archives, exports and preferences in a separate research directory. The package
uses the host's compatible Microsoft VC runtime and OpenGL driver; neither is
installed or replaced. No Python runtime is required. This portable ZIP does not
register an MSI installation or change system PATH/registry entries.

For the SDK, extract into a separate prefix and follow the
[C++ consumer guide](native-sdk.md). Required ABI: MSVC19.44/v143, Windows x64,
C++17, Release `/MD`, iterator level0. The SDK refuses incompatible CRT/debug or
missing required components. Other compilers, platforms and configurations are
outside this package's qualification. Existing Python wheels/candidates retain
their own feature/API identities; this release supplies no new Python binary.

The installed native SDK example creates a small authored count matrix, runs
QC/normalization/pseudobulk, seeded PCA/clustering and replicate Welch/BH, then
saves/reopens exact native results. The installed batch CLI verifies/replays and
exports those same records, the region CLI checks exact RGB samples, and the GUI
opens the saved project without modifying it. These deployment checks reuse the
source-scoped scientific receipts; they do not establish biological discoveries,
full QuPath parity, speed superiority or a new cross-platform qualification.

Removal is file-based. Remove only unchanged package-owned files; preserve all
research outputs and any files added inside the package directory. The checked
development removal tool preflights every member before deletion and refuses a
modified file or an out-of-prefix path. The actual handoff removed251 package
members while retaining18 user outputs, including notes inside the install tree.
An installer, signing/stable status and a fresh VM/OS acceptance are not claimed.

The application is AGPL-3.0-only; individual dependency/font notices and source
terms remain attached. Code-only OpenCV/ImGui source companions exclude unused
sample images, models, fonts and prebuilt binaries. See
[material admission](native-release-materials.md) for the exact selection and
offline rebuild configuration. Historical products and restricted/unreviewed
materials are excluded from this candidate's membership.
