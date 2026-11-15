foreach(required MODULE_FILE PROFILE OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing ICS plugin manifest input: ${required}")
  endif()
endforeach()
if(NOT PROFILE MATCHES "^windows-x64-msvc[0-9]+-release-md$")
  message(FATAL_ERROR "Unexpected ICS plugin profile")
endif()
get_filename_component(module_name "${MODULE_FILE}" NAME)
if(NOT module_name STREQUAL "qpx_ics_plugin.dll")
  message(FATAL_ERROR "Unexpected ICS module name")
endif()
file(SHA256 "${MODULE_FILE}" digest)
file(WRITE "${OUTPUT}" "{\n"
  "  \"schema\": \"qpx.reader-plugins/1\",\n"
  "  \"plugins\": [{\n"
  "    \"provider_id\": \"org.qupathxx.ics\",\n"
  "    \"display_name\": \"ICS\",\n"
  "    \"version\": \"1.0.0\",\n"
  "    \"contract\": \"ics-raw-channel-window/1\",\n"
  "    \"abi\": 1,\n"
  "    \"profile\": \"${PROFILE}\",\n"
  "    \"capabilities\": {\"display_rgba\": true, \"raw_samples\": true, \"source_files\": true},\n"
  "    \"module\": {\"path\": \"${module_name}\", \"sha256\": \"${digest}\"},\n"
  "    \"options\": {\"channel\": 0, \"window\": [0, 255]},\n"
  "    \"dependencies\": []\n"
  "  }]\n}\n")
