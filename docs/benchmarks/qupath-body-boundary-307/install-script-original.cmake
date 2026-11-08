# Install script for directory: C:/Users/HEQ/Projects/qupathxx/build/ci/scientific-grid-159

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/HEQ/AppData/Local/Temp/tmpzqh6locl/wheel/platlib")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/imagej-shape" TYPE FILE RENAME "NOTICE.txt" FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/scientific-grid-159/third_party/imagej-shape-NOTICE.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/_deps/qpx_zlib-build/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/_deps/qpx_tiff-build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/libtiff" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/_deps/qpx_tiff-src/LICENSE.md")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/tiff-zlib" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/_deps/qpx_zlib-src/LICENSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/tinyxml2" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/_deps/qpx_xml-src/LICENSE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/opencv" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/deps/opencv-sdk/share/licenses/opencv/LICENSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/opencv" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/deps/opencv-sdk/share/licenses/opencv/softfloat-notice.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/opencv" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/deps/opencv-sdk/share/licenses/opencv/zlib-notice.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/opencv" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/deps/opencv-sdk/share/licenses/opencv/SOURCE-NOTICES.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/_deps/json-build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/nlohmann-json" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/_deps/json-src/LICENSE.MIT")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/sqlite" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/scientific-grid-159/third_party/sqlite-LICENSE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE FILE FILES
      "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/bin/geos_c.dll"
      "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/bin/geos.dll"
      )
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE FILE FILES
      "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/bin/geos_c.dll"
      "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/bin/geos.dll"
      )
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE FILE FILES
      "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/bin/geos_c.dll"
      "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/bin/geos.dll"
      )
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE FILE FILES
      "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/bin/geos_c.dll"
      "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/bin/geos.dll"
      )
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/geos" TYPE FILE RENAME "COPYING" FILES "C:/Users/HEQ/Projects/qupathxx/build/deps/geos-sdk/share/licenses/geos/COPYING")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/deps/openslide/openslide-bin-4.0.1.2-windows-x64/bin/libopenslide-1.dll")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/openslide-sdk" TYPE DIRECTORY FILES "C:\Users\HEQ\Projects\qupathxx\build\deps\openslide\openslide-bin-4.0.1.2-windows-x64/licenses/")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/Debug/qupathxx_region.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/Release/qupathxx_region.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/MinSizeRel/qupathxx_region.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/RelWithDebInfo/qupathxx_region.exe")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/Debug/qupathxx_batch.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/Release/qupathxx_batch.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/MinSizeRel/qupathxx_batch.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/RelWithDebInfo/qupathxx_batch.exe")
  endif()
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/_deps/glfw-build/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/fonts" TYPE FILE FILES
    "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/qpx-gui-fonts/NotoSansCJKsc-Regular.otf"
    "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/qpx-gui-fonts/NotoEmoji.ttf"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/noto-cjk" TYPE FILE RENAME "OFL.txt" FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/qpx-gui-fonts/cjk-OFL.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/noto-cjk" TYPE FILE RENAME "NOTICE.md" FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/qpx-gui-fonts/cjk-NOTICE.md")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/noto-emoji" TYPE FILE RENAME "OFL.txt" FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/qpx-gui-fonts/emoji-OFL.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/glfw" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/cp312-cp312-win_amd64/_deps/glfw-src/LICENSE.md")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/imgui" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/gui-fonts-282/inputs/imgui193/LICENSE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/imgui" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/generated/third-party-notices/imgui/imstb_rectpack.h-LICENSE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/imgui" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/generated/third-party-notices/imgui/imstb_textedit.h-LICENSE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/imgui" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/generated/third-party-notices/imgui/imstb_truetype.h-LICENSE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/imgui" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/generated/third-party-notices/imgui/imgl3w-LICENSE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/imgui" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/generated/third-party-notices/imgui/Khronos-NOTICE.txt")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/imgui" TYPE FILE FILES
    "C:/Users/HEQ/Projects/qupathxx/build/ci/scientific-grid-159/third_party/ProggyClean-LICENSE.txt"
    "C:/Users/HEQ/Projects/qupathxx/build/ci/scientific-grid-159/third_party/ProggyForever-LICENSE.txt"
    "C:/Users/HEQ/Projects/qupathxx/build/ci/scientific-grid-159/third_party/Khronos-LICENSE.txt"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/Debug/qupathxx_app.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/Release/qupathxx_app.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/MinSizeRel/qupathxx_app.exe")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE EXECUTABLE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/RelWithDebInfo/qupathxx_app.exe")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/nanobind" TYPE FILE RENAME "LICENSE" FILES "C:/Users/HEQ/Projects/qupathxx/.venv/Lib/site-packages/nanobind-3.1.0.dist-info/licenses/LICENSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx/licenses/robin-map" TYPE FILE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/generated/third-party-notices/robin-map/LICENSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE MODULE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/Debug/_native.cp312-win_amd64.pyd")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE MODULE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/Release/_native.cp312-win_amd64.pyd")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE MODULE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/MinSizeRel/_native.cp312-win_amd64.pyd")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/qupathxx" TYPE MODULE FILES "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/RelWithDebInfo/_native.cp312-win_amd64.pyd")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
