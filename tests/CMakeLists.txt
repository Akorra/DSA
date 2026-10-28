# Test harness. Only included when dsa is the top-level project and
# DSA_BUILD_TESTS is ON (see the top-level CMakeLists.txt).

include(FetchContent)

# Catch2 v3: use an installed copy if there is one (find_package), otherwise
# download this exact tag. FIND_PACKAGE_ARGS needs CMake 3.24; SYSTEM (3.25)
# marks Catch2's headers as system headers, so our -Werror can't fire on them.
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1      # pinned: a branch name would change under us
    GIT_SHALLOW    TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 3)
FetchContent_MakeAvailable(Catch2)

include(Catch)   # catch_discover_tests()

# dsa_add_test(<name> <source>...)
#   One executable per module. Gets our warnings and the preset's sanitizers,
#   and every TEST_CASE is registered with CTest as its own test.
function(dsa_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE
        dsa::dsa dsa_warnings dsa_sanitizers Catch2::Catch2WithMain)
    # PRE_TEST: list the test cases when ctest runs, not at build time, so a
    # binary that crashes or leaks under ASan fails a test instead of the build.
    catch_discover_tests(${name} DISCOVERY_MODE PRE_TEST)
endfunction()

# dsa_add_death_test(<name> <source> PASS_REGEX <regex>)
#   For behaviour that must abort; Catch2 can't test that in-process.
#   cmake/DeathTest.cmake runs the binary and passes only if it did not exit
#   normally and its output matches the regex.
function(dsa_add_death_test name source)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "PASS_REGEX" "")
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE dsa::dsa dsa_warnings dsa_sanitizers)
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DEXE=$<TARGET_FILE:${name}>
            -DREGEX=${arg_PASS_REGEX}
            -P ${PROJECT_SOURCE_DIR}/cmake/DeathTest.cmake)
endfunction()

include(${PROJECT_SOURCE_DIR}/cmake/SelfContained.cmake)

dsa_add_test(test_smoke smoke/test_smoke.cpp)

add_subdirectory(detail)
