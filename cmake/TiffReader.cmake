include(PinnedZlib)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(tiff-static ON CACHE BOOL "" FORCE)
foreach(opt tiff-tools tiff-tests tiff-contrib tiff-docs tiff-install cxx old-jpeg jbig lerc lzma zstd webp libdeflate pixarlog)
  set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(zlib ON CACHE BOOL "" FORCE)
include(TiffJpeg)
set(jpeg ON CACHE BOOL "" FORCE)
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
install(FILES "${qpx_xml_SOURCE_DIR}/LICENSE.txt" DESTINATION qupathxx/licenses/tinyxml2)
