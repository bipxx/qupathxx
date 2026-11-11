foreach(required MODULE_FILE MODULE_NAME PROFILE OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing reader-plugin manifest input: ${required}")
  endif()
endforeach()
if(NOT MODULE_NAME STREQUAL "qpx_p6_plugin.dll" OR NOT PROFILE MATCHES "^windows-x64-msvc[0-9]+-release-md$")
  message(FATAL_ERROR "Unexpected reader-plugin module name or profile")
endif()
file(SHA256 "${MODULE_FILE}" module_sha256)
file(WRITE "${OUTPUT}" "{\n"
  "  \"schema\": \"qpx.reader-plugins/1\",\n"
  "  \"plugins\": [{\n"
  "    \"provider_id\": \"org.qupathxx.p6\",\n"
  "    \"display_name\": \"P6 display example\",\n"
  "    \"version\": \"1.0.0\",\n"
  "    \"contract\": \"p6-max255-display/1\",\n"
  "    \"abi\": 1,\n"
  "    \"profile\": \"${PROFILE}\",\n"
  "    \"capabilities\": {\"display_rgba\": true, \"raw_samples\": false},\n"
  "    \"module\": {\"path\": \"${MODULE_NAME}\", \"sha256\": \"${module_sha256}\"},\n"
  "    \"dependencies\": []\n"
  "  }]\n"
  "}\n")
