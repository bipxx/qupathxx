# SPDX-License-Identifier: AGPL-3.0-only
foreach(field MODULE_FILE PROFILE DUMPBIN OUTPUT)
  if(NOT DEFINED ${field} OR "${${field}}" STREQUAL "")
    message(FATAL_ERROR "Missing NGFF label manifest field: ${field}")
  endif()
endforeach()
get_filename_component(module_dir "${MODULE_FILE}" DIRECTORY)
get_filename_component(module_name "${MODULE_FILE}" NAME)
if(NOT module_name STREQUAL "qpx_ngff_labels_plugin.dll" OR
   NOT PROFILE MATCHES "^windows-x64-msvc[0-9]+-release-md$")
  message(FATAL_ERROR "Unexpected NGFF label module/profile")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../image-common/stage_sdk_runtime.cmake")
file(SHA256 "${MODULE_FILE}" module_digest)
file(WRITE "${OUTPUT}" "{\n"
  "  \"schema\": \"qpx.reader-plugins/1\",\n"
  "  \"plugins\": [{\n"
  "    \"provider_id\": \"org.qupathxx.ngff-labels\",\n"
  "    \"display_name\": \"OME NGFF integral labels\",\n"
  "    \"version\": \"1.0.0\", \"contract\": \"ngff04-zarr2-integral-labels/1\", \"abi\": 1,\n"
  "    \"profile\": \"${PROFILE}\",\n"
  "    \"capabilities\": {\"display_rgba\": true, \"raw_samples\": true, \"source_store\": true, \"label_description\": true},\n"
  "    \"module\": {\"path\": \"${module_name}\", \"sha256\": \"${module_digest}\"},\n"
  "    \"dependencies\": [\n${dependencies}\n    ]\n"
  "  }]\n}\n")
