# Native release material admission

The L01 profile is `release_profiles.windows-native` in the existing
[material inventory](../tests/fixtures/manifest.json), with the existing
[font provenance](../assets/fonts/font-source.json). It names finite exact
source, application and SDK members. It does not admit historical screenshots,
training/evaluation data, model weights or arbitrary files added to a checkout.
Native source identity is checked against the installed `sdk.hpp` and the actual
CMake input manifest. Python is used only for development packaging; native
applications and SDK consumers do not require it.

The selected native build uses Windows x64, MSVC19.44/v143, C++17, Release `/MD`.
P01's relocated SDK consumer and numerical evidence keep their original source
identity. L01's changed GLFW pin and FreeType credit have their own matched
source/native products. P02 separately owns final installation, use, removal
and publication. No wheel or stable/signing claim is made here.

## Freeze and stage

Install the existing matched build once with components `Unspecified` and
`NativeSDK` into an empty prefix. The first contains the standalone application,
batch/region CLI, runtime DLLs, fonts, UI resources and notices under `qupathxx`;
the second contains `include`, `lib`, `bin`, `share`. Then run:

```text
python tools/ci/stage_native_release.py --installed INSTALL_PREFIX --output NEW_OUTPUT
python tools/ci/collect_candidate_sources.py --output NEW_SOURCE_OUTPUT --cache SOURCE_CACHE --extra-cache third_party/sources --source-id COMPILED_SHA256
```

Repeat `--extra-cache` for already cached dependency source archives. Collection
defaults offline and fails on absent or mismatched bytes. Code-only OpenCV and
ImGui artifacts must be supplied from `third_party/sources`; downloading the
full upstream carrier is forbidden even with `--allow-download`.

The stager rejects changed inputs, unexpected install files and links/reparse
paths before copying. It verifies each output directory and every actual ZIP
member. A passing `admission.json` records the three archive hashes/counts and
one compiled source ID. Generated metadata has explicit first-party lineage in
`generated-materials.json`; it cannot authorize unknown payloads. The canonical
inventory snapshot is copied into the source kit without attempting to hash
itself recursively. Preserve the inventories and both receipts beside the
archives. Run the member guard again after any later repackaging.

Changing product bytes requires an explicit new reviewed inventory/profile and
source identity as applicable. The scripts do not automatically reclassify new
files or reuse an old candidate's qualification. The dedicated material-admission
CI job tests these boundaries with authored inputs, without product compilation
or external dataset downloads.

## Corresponding source and notices

The application source kit contains the exact CMake fingerprint inputs, public
and private C++ sources, optional Python control sources, native SDK example,
build/release helpers, selected small authored oracles and required notices.
Its dependency companion contains fifteen reviewed source artifacts for the
actual native configuration, including HDF5, Boost.Math, RmlUi and FreeType.
Per-file upstream notices remain in the source archives. The application selects
the FreeType License and installs its required acknowledgement alongside FTL/GPL
texts. An alternative license text does not change the selected runtime terms.
Mixed OpenSlide components and embedded OpenCV/STB/Khronos/font notices retain
separate material records; an archive's wrapper license is insufficient.

OpenCV's full upstream archive contains Lena sample images outside the build's
reviewed inputs. The unmodified code-only core/ML source companion excludes all
sample images, cascade/model XML and unused prebuilt artifacts. Its two required
`doc`/`data` CMake scripts remain; no data/model content is included. A configure
check uses the unchanged core/ML build options. ImGui's companion includes its
compiled core, embedded font/loader sources and selected GLFW/OpenGL3 backends,
excluding unused font files and legacy prebuilt libraries. Selection manifests
record every retained member's hash/size and omitted paths. The independently
licensed retained code is not derived from omitted photographs or models.

Extract dependency sources and point CMake's `FETCHCONTENT_SOURCE_DIR_` overrides
at the extracted trees: `JSON`, `SQLITE`, `GLFW`, `IMGUI`, `QPX_HDF5`, `QPX_TIFF`,
`QPX_JPEG`, `QPX_XML`, `QPX_ZLIB`, `QPX_BOOST_MATH`, `QPX_RMLUI`, `QPX_FREETYPE`.
Build GEOS/OpenCV SDKs with the included `tools/build_geos_sdk.py` and
`tools/build_opencv_sdk.py`; set `GEOS_DIR`, `OpenCV_DIR`, and `OpenSlide_ROOT`
for the corresponding dependencies. The OpenSlide source distribution retains
its upstream cross-build recipes; its Windows binary SDK/toolchain is external
to the source companion. The source kit is corresponding source, not an offline
Windows compiler/SDK distribution or a bit-for-bit build guarantee.

Set `QUPATHXX_OPENCV_NOTICES` to the OpenCV SDK's `share/licenses/opencv` and
`GEOS_LICENSE_FILE` to the GEOS SDK's `share/licenses/geos/COPYING`. For a fully
offline GUI configure, prime the new build's `qpx-gui-fonts` directory with the
exact admitted CJK/emoji faces and associated notice files from the profile;
otherwise CMake fetches those immutable open font assets. Source overrides plus
`FETCHCONTENT_FULLY_DISCONNECTED=ON` suppress dependency source downloads.

Use `QUPATHXX_BUILD_PYTHON=OFF`; enable the native backends selected in
[the SDK guide](native-sdk.md) and `QUPATHXX_BUILD_GUI=ON` for the desktop product.
Tests are optional; only the named small authored oracles are included, not the
historical acceptance corpus. Windows SDK/MSVC/CMake, system/OpenGL drivers and
OS runtime prerequisites retain their own installation terms and are not bundled
by these archives.
