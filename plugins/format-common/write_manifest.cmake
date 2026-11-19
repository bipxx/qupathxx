# SPDX-License-Identifier: AGPL-3.0-only
foreach(required MODULE_FILE MODULE_NAME PROVIDER_ID FORMAT_ID DOMAIN DISPLAY_NAME CONTRACT PROFILE OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing typed TSV plugin manifest input: ${required}")
  endif()
endforeach()
if(NOT PROFILE STREQUAL "windows-x64-msvc1944-release-md" OR
   NOT DOMAIN MATCHES "^(matrix|annotation)$")
  message(FATAL_ERROR "Unexpected typed TSV plugin profile or family")
endif()
if(NOT DEFINED ABI)
  set(ABI 1)
endif()
if(NOT DEFINED PROVIDER_VERSION)
  set(PROVIDER_VERSION 1.0.0)
endif()
if(NOT PROVIDER_VERSION MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
  message(FATAL_ERROR "Invalid typed provider version")
endif()
if(NOT ABI MATCHES "^[123456]$" OR (DOMAIN STREQUAL "matrix" AND ABI GREATER 5))
  message(FATAL_ERROR "Unsupported typed provider ABI")
endif()
get_filename_component(module_name "${MODULE_FILE}" NAME)
if(NOT module_name STREQUAL MODULE_NAME)
  message(FATAL_ERROR "Unexpected typed TSV module name")
endif()
file(SHA256 "${MODULE_FILE}" digest)
file(WRITE "${OUTPUT}" "{\n"
  "  \"schema\": \"qpx.format-plugins/1\",\n"
  "  \"plugins\": [{\n"
  "    \"provider_id\": \"${PROVIDER_ID}\",\n"
  "    \"format_id\": \"${FORMAT_ID}\",\n"
  "    \"domain\": \"${DOMAIN}\",\n"
  "    \"display_name\": \"${DISPLAY_NAME}\",\n"
  "    \"version\": \"${PROVIDER_VERSION}\",\n"
  "    \"contract\": \"${CONTRACT}\",\n"
  "    \"abi\": ${ABI},\n"
  "    \"profile\": \"${PROFILE}\",\n"
  "    \"module\": {\"path\": \"${module_name}\", \"sha256\": \"${digest}\"},\n"
  "    \"dependencies\": []\n"
  "  }]\n}\n")
