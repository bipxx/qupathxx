include(FetchContent)
# Keep compression symbols independent of OpenCV/OpenSlide's private runtimes.
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
set(ZLIB_PREFIX ON CACHE BOOL "" FORCE)
FetchContent_Declare(qpx_zlib URL
  https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
  https://zlib.net/fossils/zlib-1.3.2.tar.gz
  URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16)
FetchContent_MakeAvailable(qpx_zlib)
set_target_properties(zlibstatic PROPERTIES POSITION_INDEPENDENT_CODE ON)
# zlib's Z_PREFIX renames its public API but leaves this internal exported
# data table unprefixed. Apple's linker rejects it beside OpenCV's static zlib.
# Scope the rename to our private target; do not alter fetched source files or
# propagate the internal identifier into TIFF/public consumer compilation.
target_compile_definitions(zlibstatic PRIVATE z_errmsg=qpx_tiff_z_errmsg)
if(TARGET ZLIB::ZLIB)
  message(FATAL_ERROR "TIFF requires its pinned, prefixed zlib target; an unrelated ZLIB::ZLIB already exists")
endif()
add_library(ZLIB::ZLIB ALIAS zlibstatic)
file(MAKE_DIRECTORY "${qpx_zlib_BINARY_DIR}/qpx-include")
configure_file("${qpx_zlib_SOURCE_DIR}/zlib.h" "${qpx_zlib_BINARY_DIR}/qpx-include/zlib.h" COPYONLY)
configure_file("${qpx_zlib_BINARY_DIR}/zconf.h" "${qpx_zlib_BINARY_DIR}/qpx-include/zconf.h" COPYONLY)
set(ZLIB_INCLUDE_DIR "${qpx_zlib_BINARY_DIR}/qpx-include" CACHE PATH "" FORCE)
set(ZLIB_LIBRARY zlibstatic CACHE STRING "" FORCE)
set(ZLIB_LIBRARY_RELEASE zlibstatic CACHE STRING "" FORCE)
set(ZLIB_LIBRARY_DEBUG zlibstatic CACHE STRING "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(tiff-static ON CACHE BOOL "" FORCE)
foreach(opt tiff-tools tiff-tests tiff-contrib tiff-docs tiff-install cxx jpeg old-jpeg jbig lerc lzma zstd webp libdeflate pixarlog)
  set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(zlib ON CACHE BOOL "" FORCE)
FetchContent_Declare(qpx_tiff URL https://download.osgeo.org/libtiff/tiff-4.7.1.tar.xz
  URL_HASH SHA256=b92017489bdc1db3a4c97191aa4b75366673cb746de0dce5d7a749d5954681ba)
FetchContent_MakeAvailable(qpx_tiff)
# ZLIB::ZLIB is our alias, so FindZLIB may not attach ZLIB_INCLUDE_DIR.
# Prefer the paired copied zlib.h/configured zconf.h over upstream source
# zconf.h, whose unprefixed declarations cannot link our prefixed archive.
target_include_directories(tiff BEFORE PRIVATE "${qpx_zlib_BINARY_DIR}/qpx-include")
set_target_properties(tiff PROPERTIES POSITION_INDEPENDENT_CODE ON)
FetchContent_Declare(qpx_xml URL https://codeload.github.com/leethomason/tinyxml2/tar.gz/refs/tags/11.0.0
  URL_HASH SHA256=5556deb5081fb246ee92afae73efd943c889cef0cafea92b0b82422d6a18f289
  SOURCE_SUBDIR qpx-source-only)
FetchContent_MakeAvailable(qpx_xml)
add_library(qupathxx_tiff_xml STATIC "${qpx_xml_SOURCE_DIR}/tinyxml2.cpp")
target_include_directories(qupathxx_tiff_xml PUBLIC "${qpx_xml_SOURCE_DIR}")
set_target_properties(qupathxx_tiff_xml PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(qupathxx_core PRIVATE TIFF::tiff qupathxx_tiff_xml)
target_compile_definitions(qupathxx_core PRIVATE QUPATHXX_HAS_TIFF)
install(FILES "${qpx_tiff_SOURCE_DIR}/LICENSE.md" DESTINATION qupathxx/licenses/libtiff)
install(FILES "${qpx_zlib_SOURCE_DIR}/LICENSE" DESTINATION qupathxx/licenses/tiff-zlib)
install(FILES "${qpx_xml_SOURCE_DIR}/LICENSE.txt" DESTINATION qupathxx/licenses/tinyxml2)
