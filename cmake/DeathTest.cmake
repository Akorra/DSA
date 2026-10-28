# Run as a script: cmake -DEXE=<binary> -DREGEX=<regex> -P DeathTest.cmake
# Used by dsa_add_death_test. CTest reports any process killed by a signal
# (abort()) as a failure, even with WILL_FAIL, so this script runs the binary
# itself and passes only if it did NOT exit with 0 and its output matches REGEX.

execute_process(COMMAND "${EXE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE  out)   # same variable: stdout and stderr together

if(result EQUAL 0)
    message(FATAL_ERROR "expected the program to abort, but it exited with 0.\nOutput:\n${out}")
endif()
if(NOT out MATCHES "${REGEX}")
    message(FATAL_ERROR "program stopped (${result}) but output doesn't match\n  ${REGEX}\nOutput:\n${out}")
endif()
message(STATUS "aborted as expected (${result})")
