# GUI-only dependencies: native/headless configurations do not discover them.
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
# Keep all frontend dependencies static, without changing the parent's policy.
set(_qpx_shared_defined FALSE)
if(DEFINED BUILD_SHARED_LIBS)
  set(_qpx_shared_defined TRUE)
  set(_qpx_shared_saved "${BUILD_SHARED_LIBS}")
endif()
set(BUILD_SHARED_LIBS OFF)
FetchContent_Declare(qpx_freetype
  URL https://codeload.github.com/freetype/freetype/zip/534ad3456055ee1f65ecde3bcf22a656a31514d1
  URL_HASH SHA256=fa3be35dcd192c98c29531ecea16ad4cbc24b4d24694f52db905c83fcc0e3fa5)
FetchContent_MakeAvailable(qpx_freetype)
set_property(DIRECTORY "${qpx_freetype_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
if(NOT TARGET Freetype::Freetype)
  add_library(Freetype::Freetype ALIAS freetype)
endif()
set_target_properties(freetype PROPERTIES POSITION_INDEPENDENT_CODE ON)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_FONT_ENGINE freetype CACHE STRING "" FORCE)
FetchContent_Declare(qpx_rmlui
  URL https://codeload.github.com/mikke89/RmlUi/zip/ba95ffe8bfb6370efb2cdcca927eaad4710c5413
  URL_HASH SHA256=ed3162ef67d24b7d91557d710b3734433cfc2a8659c55b4d68a11de38b178487)
FetchContent_MakeAvailable(qpx_rmlui)
set_property(DIRECTORY "${qpx_rmlui_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
set_target_properties(rmlui_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(_qpx_shared_defined)
  set(BUILD_SHARED_LIBS "${_qpx_shared_saved}")
else()
  unset(BUILD_SHARED_LIBS)
endif()
set(QPX_GUI_RESOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui")
install(DIRECTORY "${QPX_GUI_RESOURCE_DIRECTORY}/" DESTINATION qupathxx/ui)
install(FILES "${qpx_rmlui_SOURCE_DIR}/LICENSE.txt" DESTINATION qupathxx/licenses/rmlui)
qupathxx_header_notice("${qpx_rmlui_SOURCE_DIR}/Backends/RmlUi_Include_GL3.h"
  "** Copyright (c) 2008-2018 The Khronos Group Inc." rmlui Khronos-NOTICE.txt)
install(FILES "${qpx_freetype_SOURCE_DIR}/docs/FTL.TXT" "${qpx_freetype_SOURCE_DIR}/docs/GPLv2.TXT"
  DESTINATION qupathxx/licenses/freetype)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype-NOTICE.txt"
  DESTINATION qupathxx/licenses/freetype RENAME NOTICE.txt)
function(qupathxx_copy_gui_resources target)
  add_custom_target(${target}_ui_resources
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${QPX_GUI_RESOURCE_DIRECTORY}" "$<TARGET_FILE_DIR:${target}>/ui"
    COMMENT "Copying reloadable RmlUi resources")
  add_dependencies(${target} ${target}_ui_resources)
endfunction()
