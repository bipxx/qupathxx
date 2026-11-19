cmake_minimum_required(VERSION 3.24)
project(qupathxx_ngff_labels_host LANGUAGES CXX)
# Source-scoped application overlay over the exact frozen FP04 SDK. This recipe
# neither rebuilds that SDK nor discovers Python/bindings or builds a provider.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR
   NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64" OR
   NOT MSVC_VERSION STREQUAL "1944" OR
   NOT CMAKE_CXX_COMPILER_VERSION STREQUAL "19.44.35225.0")
  message(FATAL_ERROR "Use the recorded Windows x64 MSVC1944/compiler19.44.35225.0 profile")
endif()
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release CACHE STRING "Native label configuration" FORCE)
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "The native label host requires Release /MD")
endif()
find_package(qupathxx 0.1.0 CONFIG REQUIRED COMPONENTS core)
get_target_property(base_core qupathxx::core IMPORTED_LOCATION_RELEASE)
if(NOT EXISTS "${base_core}")
  message(FATAL_ERROR "The frozen qupathxx SDK core is missing")
endif()
file(SHA256 "${base_core}" base_core_sha)
if(NOT base_core_sha STREQUAL "4bf957c9ea6e46294c11b9bfb7d9c2496cf4fca607a9c28ba837768d301eb6bb")
  message(FATAL_ERROR "This overlay requires the exact retained FP04 SDK core; use root CMake for another native context")
endif()

get_filename_component(source_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(QUPATHXX_LABELS_JSON_INCLUDE "${source_root}/third_party/native-overlay/json/include" CACHE PATH "Matching nlohmann-json3.12.0 headers")
set(QUPATHXX_LABELS_SQLITE_INCLUDE "${source_root}/third_party/native-overlay/sqlite" CACHE PATH "Matching SQLite3.54.1 header")
foreach(header "${QUPATHXX_LABELS_JSON_INCLUDE}/nlohmann/json.hpp" "${QUPATHXX_LABELS_SQLITE_INCLUDE}/sqlite3.h")
  if(NOT EXISTS "${header}")
    message(FATAL_ERROR "Provide the recorded native dependency header: ${header}")
  endif()
endforeach()

# The same14 application/observer/reader/project sources and three frame/NGFF/
# archive authorities used by the source-scoped native delivery library.
add_library(qupathxx_ngff_labels_native STATIC
  "${source_root}/cpp/directory_inventory.cpp"
  "${source_root}/cpp/directory_store.cpp"
  "${source_root}/cpp/ome_zarr_store.cpp"
  "${source_root}/cpp/label_codes.cpp"
  "${source_root}/cpp/label_mask.cpp"
  "${source_root}/cpp/label_workflow.cpp"
  "${source_root}/cpp/label_cli.cpp"
  "${source_root}/cpp/reader_plugin_catalog.cpp"
  "${source_root}/cpp/reader_plugin_loader.cpp"
  "${source_root}/cpp/reader_plugin_bridge.cpp"
  "${source_root}/cpp/reader_plugin_link.cpp"
  "${source_root}/cpp/reader_plugin_runtime.cpp"
  "${source_root}/cpp/project.cpp"
  "${source_root}/cpp/project_source.cpp"
  "${source_root}/cpp/label_mask_frame.cpp"
  "${source_root}/cpp/ngff_label_mask.cpp"
  "${source_root}/cpp/label_mask_archive.cpp")

set(QUPATHXX_LABELS_OBJECT_DIR "" CACHE PATH "Optional directory of the five exact retained scientific objects")
if(QUPATHXX_LABELS_OBJECT_DIR)
  # Exact identities from the retained ngff-labels/native-object-reuse.json.
  # These objects remain separate from newly compiled application authorities.
  set(QPX_LABELS_label_raster_SHA256 "0e3247feee2cc44361e01267bf58974aedc89c7968e17b9b34eb009264ccdfaa")
  set(QPX_LABELS_superpixels_SHA256 "ab1fc9ffad072d5445b4db65f43d332f200bedfbb592c59c7b12564123b986c4")
  set(QPX_LABELS_subcellular_SHA256 "e9d0195d248e9be7516e1a509745c013f26f409881ed80c84e0f30c496d7341f")
  set(QPX_LABELS_tissue_contours_SHA256 "5e4ba627aa058cae385510856873ee796100aadfe3b94495a29f5d3bf3073559")
  set(QPX_LABELS_fluorescence_cells_SHA256 "7077d223d66322687d5c67f6b9ac3cc005a12b867c383ba3973c228428f0e963")
  foreach(name label_raster superpixels subcellular tissue_contours fluorescence_cells)
    set(object "${QUPATHXX_LABELS_OBJECT_DIR}/${name}.obj")
    if(NOT EXISTS "${object}")
      message(FATAL_ERROR "Missing retained scientific object: ${name}")
    endif()
    file(SHA256 "${object}" actual_sha)
    if(NOT actual_sha STREQUAL "${QPX_LABELS_${name}_SHA256}")
      message(FATAL_ERROR "Retained scientific object identity differs: ${name}")
    endif()
    set_source_files_properties("${object}" PROPERTIES EXTERNAL_OBJECT YES GENERATED YES)
    target_sources(qupathxx_ngff_labels_native PRIVATE "${object}")
  endforeach()
else()
  # Corresponding source can rebuild the existing authorities without a core
  # rebuild. This does not transfer qualification of the retained objects.
  set(QUPATHXX_LABELS_GEOS_INCLUDE "" CACHE PATH "Matching GEOS3.15.0 public C header directory")
  if(NOT EXISTS "${QUPATHXX_LABELS_GEOS_INCLUDE}/geos_c.h")
    message(FATAL_ERROR "Provide the matching GEOS3.15.0 header or exact retained scientific objects")
  endif()
  file(READ "${QUPATHXX_LABELS_GEOS_INCLUDE}/geos_c.h" geos_header)
  foreach(part MAJOR MINOR PATCH)
    if(part STREQUAL "MAJOR")
      set(expected 3)
    elseif(part STREQUAL "MINOR")
      set(expected 15)
    else()
      set(expected 0)
    endif()
    if(NOT geos_header MATCHES "#[ \t]*define[ \t]+GEOS_VERSION_${part}[ \t]+${expected}([ \t\r\n]|$)")
      message(FATAL_ERROR "The scientific source route requires the recorded GEOS3.15.0 C header")
    endif()
  endforeach()
  target_sources(qupathxx_ngff_labels_native PRIVATE
    "${source_root}/cpp/label_raster.cpp"
    "${source_root}/cpp/superpixels.cpp"
    "${source_root}/cpp/subcellular.cpp"
    "${source_root}/cpp/tissue_contours.cpp"
    "${source_root}/cpp/fluorescence_cells.cpp")
  target_include_directories(qupathxx_ngff_labels_native PRIVATE "${QUPATHXX_LABELS_GEOS_INCLUDE}")
  target_compile_definitions(qupathxx_ngff_labels_native PRIVATE QUPATHXX_HAS_GEOS)
endif()

target_include_directories(qupathxx_ngff_labels_native PUBLIC "${source_root}/cpp/include"
  PRIVATE "${source_root}/cpp" "${QUPATHXX_LABELS_JSON_INCLUDE}" "${QUPATHXX_LABELS_SQLITE_INCLUDE}")
target_compile_definitions(qupathxx_ngff_labels_native PRIVATE QUPATHXX_HAS_READER_PLUGINS QUPATHXX_HAS_PROJECT)
target_compile_options(qupathxx_ngff_labels_native PRIVATE /utf-8 /W4 /permissive- /bigobj)
target_link_libraries(qupathxx_ngff_labels_native PUBLIC qupathxx::core)
add_library(qupathxx::ngff_labels ALIAS qupathxx_ngff_labels_native)

add_executable(qupathxx_labels "${source_root}/cpp/label_main.cpp")
target_include_directories(qupathxx_labels PRIVATE "${source_root}/cpp")
target_compile_options(qupathxx_labels PRIVATE /utf-8 /W4 /permissive- /bigobj)
target_link_libraries(qupathxx_labels PRIVATE qupathxx_ngff_labels_native)
qupathxx_stage_runtime(qupathxx_labels)
