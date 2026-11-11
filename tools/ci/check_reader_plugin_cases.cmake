if(NOT EXISTS "${TEST}" OR NOT DEFINED WORK)
  message(FATAL_ERROR "Supply TEST executable and WORK directory")
endif()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef run_id)
set(directory "${WORK}/${run_id}")
file(MAKE_DIRECTORY "${directory}")
# Each process owns one immutable catalog. Rejected module cases never reload
# or unload an admitted module in the same host process.
foreach(case IN ITEMS valid missing-module bad-hash changed-module descriptor collision
    profile abi entry-abi entry-size entry-null entry-throw describe-throw valid-dependency missing-dependency
    changed-dependency unexpected-dependency preloaded-dependency)
  execute_process(COMMAND "${TEST}" --loader-case "${case}" "${directory}/${case}"
    RESULT_VARIABLE result OUTPUT_VARIABLE log ERROR_VARIABLE error TIMEOUT 15)
  if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Native plugin loader case ${case} failed: ${result}; ${log}${error}")
  endif()
endforeach()
message(STATUS "Fresh-process native module/ABI/hash/descriptor/dependency admission and built-in survival passed: ${directory}")
