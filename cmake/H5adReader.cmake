include(PinnedZlib)
include(FetchContent)
if(CMAKE_VERSION VERSION_LESS 3.26)
  message(FATAL_ERROR "QUPATHXX_BUILD_H5AD requires CMake3.26 or newer for pinned HDF5")
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
# Upstream forbids a threadsafe static Windows build. Native H5AD calls use
# one cancellation-aware library gate; no HDF5 handle escapes that gate.
set(HDF5_ENABLE_THREADSAFE OFF CACHE BOOL "" FORCE)
set(HDF5_ENABLE_ZLIB_SUPPORT ON CACHE BOOL "" FORCE)
foreach(opt HDF5_BUILD_HL_LIB HDF5_BUILD_CPP_LIB HDF5_BUILD_FORTRAN HDF5_BUILD_JAVA
    HDF5_BUILD_TOOLS HDF5_BUILD_EXAMPLES HDF5_BUILD_DOC HDF5_ENABLE_PARALLEL
    HDF5_ENABLE_CONCURRENCY HDF5_ENABLE_SZIP_SUPPORT HDF5_ENABLE_PLUGIN_SUPPORT
    HDF5_ENABLE_ROS3_VFD HDF5_ENABLE_HDFS HDF5_ENABLE_MAP_API)
  set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(qpx_hdf5
  URL https://codeload.github.com/HDFGroup/hdf5/tar.gz/refs/tags/2.2.0
  URL_HASH SHA256=5b8d75125ae7b4fef55d3b39e8d3dbfd238cdf63b6518afd67fa69ac80f06542
  DOWNLOAD_NAME hdf5-2.2.0.tar.gz
  DOWNLOAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../dependency-downloads")
FetchContent_MakeAvailable(qpx_hdf5)
target_link_libraries(qupathxx_core PRIVATE hdf5-static)
target_compile_definitions(qupathxx_core PRIVATE QUPATHXX_HAS_H5AD)
set_target_properties(hdf5-static PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(FILES "${qpx_hdf5_SOURCE_DIR}/LICENSE" DESTINATION qupathxx/licenses/hdf5)
