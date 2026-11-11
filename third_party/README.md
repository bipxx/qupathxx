# Third-party notices

- Optional native replicate DE uses only the standalone include tree from
  [Boost.Math boost-1.88.0](https://github.com/boostorg/math/tree/boost-1.88.0),
  pinned archive SHA256 `bb753392c3908292da46034a5435107f9e0546fda54da4d47b438aff5e4d9c48`.
  `cmake/ExpressionStatistics.cmake` forces `BOOST_MATH_STANDALONE` for the single
  numerical translation unit; no other Boost library, test or documentation
  project is built. Header copyright notices are retained. The unmodified
  [root release license](https://github.com/boostorg/boost/blob/boost-1.88.0/LICENSE_1_0.txt)
  is `boost-math-LICENSE_1_0.txt`, SHA256
  `c9bff75738922193e67fa726fa225535870d2aa1059f91452c411736284ad566`,
  installed as `qupathxx/licenses/boost-math/LICENSE_1_0.txt`. BSL-1.0 permits
  use/modification/distribution and is compatible with this AGPL project;
  retain source notices and license with corresponding-source distribution.
  `QUPATHXX_BUILD_STATISTICS=OFF` by default needs no Boost headers or runtime.

These files describe third-party code; they do not select a license for qupathxx.

- `ProggyClean-LICENSE.txt` is the unmodified license from
  [bluescan/proggyfonts at 139ec08a38096161291792313ef5803fc4f0e37b](https://github.com/bluescan/proggyfonts/blob/139ec08a38096161291792313ef5803fc4f0e37b/LICENSE).
  Dear ImGui v1.91.9b embeds ProggyClean and retains its copyright in
  `imgui_draw.cpp`.
- `Khronos-LICENSE.txt` preserves the license comment from
  [KHR/khrplatform.h at e964211ff5f7c2bcada786954a16d5b813379395](https://github.com/KhronosGroup/EGL-Registry/blob/e964211ff5f7c2bcada786954a16d5b813379395/api/KHR/khrplatform.h).
  ImGui's embedded loader uses adapted KHR platform types and OpenGL registry
  declarations. Their separate copyright/SPDX notice is also extracted from
  the actual loader used in the build.
- `sqlite-LICENSE.txt` records the SQLite amalgamation's public-domain statement.

CMake installs GLFW/Dear ImGui notices from the fetched sources, both license
alternatives from the three embedded STB headers, and the embedded GL loader's
public-domain notice. Python builds install the license belonging to the actual
nanobind distribution and robin-map's notice from its header. A source checkout
without nanobind distribution metadata can provide `NANOBIND_LICENSE_FILE`.
Missing extraction markers or a missing nanobind license stop configuration.

GEOS, nlohmann/json and Windows OpenSlide SDK notices are installed by their
existing dependency rules. Standalone candidates copy the wheel's notice tree.
The optional Linux GEOS bundle requires the SDK's `COPYING` notice and retains
the shared C API/C++ runtime bytes; the installed-runtime probe verifies the
notice and runtime hashes against that SDK.

Quantitative TIFF builds also install libtiff's `LICENSE.md`, the separate
TIFF zlib runtime's `LICENSE`, and tinyxml2's `LICENSE.txt`. OpenCV builds retain
its `LICENSE`, embedded softfloat/zlib notices and source notice inventory.
The OpenSlide SDK notice tree covers its bundled dependencies as well as
OpenSlide itself. These are dependency notices; their presence does not resolve
the outstanding project license or the release distribution review.

- Native NuCLS PNG decoding uses unmodified [LodePNG sources and pin](../cpp/vendor/lodepng/README.md)
  under the [Zlib license](../cpp/vendor/lodepng/LICENSE). The decoder is compiled
  without ancillary metadata conversion or platform codecs; its license is
  installed alongside the application notices. The vendored sources belong in
  the matching application source archive.


The optional NativeSDK install component retains exact enabled native dependency
notices under `share/qupathxx/licenses`, separately from app/wheel paths. Its
private link closure copies only actual selected libraries/import libraries and
required DLLs. Header-only JSON/Boost notices are retained without exposing their
headers as consumer requirements. SDK contents/byte-origin receipts are local P01
evidence; L01 corresponding-source/release membership has its own
[native material profile](../docs/native-release-materials.md).
The Windows JPEG wrapper follows the parent's CRT through upstream WITH_CRT_DLL;
this changes build flags, not its admitted source/license or codec behavior.

The native-only L01 build uses ImGui at
`b916186e66aa3c02b72c6ca17455516512ed8049` and GLFW at immutable commit
`7b6aead9fb88b3623e3b3725ebb42670cbe4c579` (3.4). The source companions in
`sources/` retain selected unmodified OpenCV core/ML and ImGui code, licenses and
embedded notices; their selection manifests record every exact member. They omit
unused sample images/models, extra font files and legacy prebuilt libraries.
The OpenCV builder consumes that code-only artifact, with no full-carrier fallback.
`freetype-NOTICE.txt` supplies the selected FTL acknowledgement; the installed
FTL/GPL texts preserve the upstream alternative without changing that selection.
