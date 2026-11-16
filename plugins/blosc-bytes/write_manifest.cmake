foreach(required MODULE_FILE PROFILE OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing Blosc decoded byte array plugin manifest input: ${required}")
  endif()
endforeach()
if(NOT PROFILE MATCHES "^windows-x64-msvc[0-9]+-release-md$")
  message(FATAL_ERROR "Unexpected Blosc decoded byte array plugin profile")
endif()
get_filename_component(module_name "${MODULE_FILE}" NAME)
if(NOT module_name STREQUAL "qpx_blosc_bytes_plugin.dll")
  message(FATAL_ERROR "Unexpected Blosc decoded byte array module name")
endif()
file(SHA256 "${MODULE_FILE}" digest)
file(WRITE "${OUTPUT}" "{\n"
  "  \"schema\": \"qpx.reader-plugins/1\",\n"
  "  \"plugins\": [{\n"
  "    \"provider_id\": \"org.qupathxx.blosc-bytes\",\n"
  "    \"display_name\": \"Blosc decoded byte array\",\n"
  "    \"version\": \"1.0.0\",\n"
  "    \"contract\": \"blosc1-lossless-flat-bytes/1\",\n"
  "    \"abi\": 1,\n"
  "    \"profile\": \"${PROFILE}\",\n"
  "    \"capabilities\": {\"display_rgba\": true, \"raw_samples\": true},\n"
  "    \"module\": {\"path\": \"${module_name}\", \"sha256\": \"${digest}\"},\n"
  "    \"dependencies\": []\n"
  "  }]\n}\n")
