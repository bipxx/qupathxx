foreach(required MODULE_FILE PROFILE OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing PNM plugin manifest input: ${required}")
  endif()
endforeach()
if(NOT PROFILE MATCHES "^windows-x64-msvc[0-9]+-release-md$")
  message(FATAL_ERROR "Unexpected PNM plugin profile")
endif()
get_filename_component(module_name "${MODULE_FILE}" NAME)
if(NOT module_name STREQUAL "qpx_pnm_plugin.dll")
  message(FATAL_ERROR "Unexpected PNM module name")
endif()
file(SHA256 "${MODULE_FILE}" digest)
file(WRITE "${OUTPUT}" "{\n"
  "  \"schema\": \"qpx.reader-plugins/1\",\n"
  "  \"plugins\": [{\n"
  "    \"provider_id\": \"org.qupathxx.pnm\",\n"
  "    \"display_name\": \"PNM\",\n"
  "    \"version\": \"1.0.0\",\n"
  "    \"contract\": \"pnm-pam-uint-samples/1\",\n"
  "    \"abi\": 1,\n"
  "    \"profile\": \"${PROFILE}\",\n"
  "    \"capabilities\": {\"display_rgba\": true, \"raw_samples\": true},\n"
  "    \"options\": {\"transfer\": \"bt709\"},\n"
  "    \"module\": {\"path\": \"${module_name}\", \"sha256\": \"${digest}\"},\n"
  "    \"dependencies\": []\n"
  "  }]\n}\n")
