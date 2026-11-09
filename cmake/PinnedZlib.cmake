include_guard(GLOBAL)
include(FetchContent)
# Keep compression symbols independent of OpenCV/OpenSlide's private runtimes.
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
set(ZLIB_PREFIX ON CACHE BOOL "" FORCE)
# Pinned zlib 1.3.2 assigns this normal variable only when it first writes
# zconf.h.cmakein; propagate it on every configure, including cached builds.
set(Z_PREFIX ON)
FetchContent_Declare(qpx_zlib URL
  https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
  https://zlib.net/fossils/zlib-1.3.2.tar.gz
  URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16)
FetchContent_MakeAvailable(qpx_zlib)
file(READ "${qpx_zlib_BINARY_DIR}/zconf.h" qpx_zlib_configured_header)
if(NOT qpx_zlib_configured_header MATCHES "#define Z_PREFIX 1")
  message(FATAL_ERROR "Pinned TIFF zlib must retain Z_PREFIX on every configure")
endif()
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
install(FILES "${qpx_zlib_SOURCE_DIR}/LICENSE" DESTINATION qupathxx/licenses/tiff-zlib)
