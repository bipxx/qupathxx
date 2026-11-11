if(NOT EXISTS "${TEST}" OR NOT DEFINED WORK)
  message(FATAL_ERROR "Supply TEST executable and WORK directory")
endif()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef run_id)
get_filename_component(directory "${WORK}/${run_id}" ABSOLUTE)
# Each process receives one immutable provider catalog. Invalid inputs and all
# authored saved projects remain available after any failure.
foreach(case IN ITEMS prepare create-base create-dependency create-compound missing exact
    canonical-options changed-module changed-dependency changed-options compound-exact corrupt compound)
  execute_process(COMMAND "${TEST}" "${case}" "${directory}"
    RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error TIMEOUT 20)
  if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Saved plugin case ${case} failed: ${result}; ${log}${error}")
  endif()
endforeach()
message(STATUS "Native saved plugin sources, exact/reviewed RGB identity, missing-provider metadata and compound raw forwarding passed: ${directory}")
