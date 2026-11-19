# SPDX-License-Identifier: AGPL-3.0-only
foreach(required MODULE_FILE MODULE_NAME PROVIDER_ID DISPLAY_NAME CONTRACT VERSION PROFILE OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing image provider manifest field: ${required}")
  endif()
endforeach()
get_filename_component(actual "${MODULE_FILE}" NAME)
if(NOT actual STREQUAL MODULE_NAME OR NOT MODULE_NAME MATCHES "^qpx_[a-z0-9_]+_plugin\\.dll$" OR
   NOT PROVIDER_ID MATCHES "^org\\.qupathxx\\.[a-z0-9-]+$" OR
   NOT DISPLAY_NAME MATCHES "^[A-Za-z0-9 .-]+$" OR NOT CONTRACT MATCHES "^[a-z0-9/-]+$" OR
   NOT VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$" OR
   NOT PROFILE MATCHES "^windows-x64-msvc[0-9]+-release-md$")
  message(FATAL_ERROR "Image provider manifest values do not match the declared native profile")
endif()
set(store_capability "")
if(SOURCE_STORE)
  set(store_capability ", \"source_store\": true")
endif()
set(files_capability "")
if(SOURCE_FILES)
  if(SOURCE_STORE)
    message(FATAL_ERROR "Image reader cannot declare both source files and a store")
  endif()
  set(files_capability ", \"source_files\": true")
endif()
set(dependencies "")
if(SDK_RUNTIME_FILES)
  include("${CMAKE_CURRENT_LIST_DIR}/stage_sdk_runtime.cmake")
endif()
file(SHA256 "${MODULE_FILE}" digest)
file(WRITE "${OUTPUT}" "{\n"
  "  \"schema\": \"qpx.reader-plugins/1\",\n"
  "  \"plugins\": [{\n"
  "    \"provider_id\": \"${PROVIDER_ID}\", \"display_name\": \"${DISPLAY_NAME}\",\n"
  "    \"version\": \"${VERSION}\", \"contract\": \"${CONTRACT}\", \"abi\": 1,\n"
  "    \"profile\": \"${PROFILE}\",\n"
  "    \"capabilities\": {\"display_rgba\": true, \"raw_samples\": true${store_capability}${files_capability}},\n"
  "    \"module\": {\"path\": \"${MODULE_NAME}\", \"sha256\": \"${digest}\"},\n"
  "    \"dependencies\": [${dependencies}]\n"
  "  }]\n}\n")
