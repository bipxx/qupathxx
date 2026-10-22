# Third-party notices

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
