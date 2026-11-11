# libjpeg-turbo explicitly requires a standalone build, not add_subdirectory.
# Keep it static/PIC, with hidden symbols on ELF/Mach-O so OpenSlide's private
# shared codecs cannot interpose this libtiff dependency. OpenCV core/ml is
# built without image codecs by tools/build_opencv_sdk.py.
include(ExternalProject)
FetchContent_Declare(qpx_jpeg URL
  https://codeload.github.com/libjpeg-turbo/libjpeg-turbo/tar.gz/refs/tags/3.1.3
  URL_HASH SHA256=3a13a5ba767dc8264bc40b185e41368a80d5d5f945944d1dbaa4b2fb0099f4e5
  SOURCE_SUBDIR qpx-source-only)
FetchContent_MakeAvailable(qpx_jpeg)
file(MAKE_DIRECTORY "${qpx_jpeg_BINARY_DIR}")
set(qpx_jpeg_args
  -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF
  -DWITH_TOOLS=OFF -DWITH_TESTS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DCMAKE_C_VISIBILITY_PRESET=hidden
  "-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}"
  "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}")
foreach(variable CMAKE_TOOLCHAIN_FILE CMAKE_SYSROOT CMAKE_OSX_SYSROOT
                 CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
                 CMAKE_MSVC_RUNTIME_LIBRARY)
  if(DEFINED ${variable} AND NOT "${${variable}}" STREQUAL "")
    list(APPEND qpx_jpeg_args "-D${variable}:STRING=${${variable}}")
  endif()
endforeach()
if(WIN32)
  # Upstream otherwise overrides even a forwarded MSVC runtime with /MT.
  # Follow the parent CRT; CMake's unspecified default is the dynamic CRT.
  if(MSVC)
    if(CMAKE_MSVC_RUNTIME_LIBRARY AND NOT CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
      list(APPEND qpx_jpeg_args -DWITH_CRT_DLL=OFF)
    else()
      list(APPEND qpx_jpeg_args -DWITH_CRT_DLL=ON)
    endif()
  endif()
  set(qpx_jpeg_name jpeg-static.lib)
else()
  set(qpx_jpeg_name libjpeg.a)
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(qpx_jpeg_archive "${qpx_jpeg_BINARY_DIR}/$<CONFIG>/${qpx_jpeg_name}")
else()
  set(qpx_jpeg_archive "${qpx_jpeg_BINARY_DIR}/${qpx_jpeg_name}")
endif()
ExternalProject_Add(qupathxx_tiff_jpeg_build
  SOURCE_DIR "${qpx_jpeg_SOURCE_DIR}"
  BINARY_DIR "${qpx_jpeg_BINARY_DIR}"
  DOWNLOAD_COMMAND "" UPDATE_COMMAND "" PATCH_COMMAND ""
  CMAKE_ARGS ${qpx_jpeg_args}
  BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config $<CONFIG>
    --target jpeg-static
  BUILD_BYPRODUCTS "${qpx_jpeg_archive}"
  INSTALL_COMMAND "")
if(TARGET JPEG::JPEG)
  message(FATAL_ERROR "TIFF requires its pinned JPEG dependency before an unrelated JPEG::JPEG")
endif()
add_library(qupathxx_tiff_jpeg STATIC IMPORTED GLOBAL)
set_target_properties(qupathxx_tiff_jpeg PROPERTIES
  IMPORTED_LOCATION "${qpx_jpeg_archive}"
  INTERFACE_INCLUDE_DIRECTORIES "${qpx_jpeg_SOURCE_DIR}/src;${qpx_jpeg_BINARY_DIR}")
# Generator expressions aren't expanded in IMPORTED_LOCATION.
if(CMAKE_CONFIGURATION_TYPES)
  foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${config}" upper)
    string(REPLACE "$<CONFIG>" "${config}" archive "${qpx_jpeg_archive}")
    set_property(TARGET qupathxx_tiff_jpeg PROPERTY IMPORTED_LOCATION_${upper} "${archive}")
  endforeach()
endif()
add_dependencies(qupathxx_tiff_jpeg qupathxx_tiff_jpeg_build)
add_library(JPEG::JPEG ALIAS qupathxx_tiff_jpeg)
# libtiff's finder uses these, while JPEG::JPEG supplies generated jconfig.h
# and the build-order dependency. The pinned 3.1 API provides both entry points;
# a configure-time link probe cannot run before this external target builds.
set(JPEG_INCLUDE_DIR "${qpx_jpeg_SOURCE_DIR}/src" CACHE PATH "" FORCE)
set(JPEG_LIBRARY qupathxx_tiff_jpeg CACHE STRING "" FORCE)
set(HAVE_JPEGTURBO_DUAL_MODE_8_12 TRUE CACHE INTERNAL "Pinned libjpeg-turbo 3.1 API" FORCE)
install(FILES "${qpx_jpeg_SOURCE_DIR}/LICENSE.md" "${qpx_jpeg_SOURCE_DIR}/README.ijg"
  DESTINATION qupathxx/licenses/tiff-jpeg)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/tiff-jpeg-NOTICE.txt"
  DESTINATION qupathxx/licenses/tiff-jpeg RENAME NOTICE.txt)
