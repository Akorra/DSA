# DSA_ASSERT behaviour. Each variant forces DSA_ENABLE_ASSERTS, so the result
# is the same under every preset (release defines NDEBUG, debug doesn't).

dsa_add_death_test(assert_fires assert_fires.cpp
    PASS_REGEX "\\[DSA ASSERTION FAILED\\].*Condition: 1 \\+ 1 == 3.*Message:   math.*Source:    .*assert_fires\\.cpp:")
target_compile_definitions(assert_fires PRIVATE DSA_ENABLE_ASSERTS=1)

# Not a death test: must run to the end. A plain test passes on exit code 0;
# the regex also proves the line after the assert ran.
add_executable(assert_disabled assert_fires.cpp)
target_link_libraries(assert_disabled PRIVATE dsa::dsa dsa_warnings dsa_sanitizers)
target_compile_definitions(assert_disabled PRIVATE DSA_ENABLE_ASSERTS=0)
add_test(NAME assert_disabled COMMAND assert_disabled)
set_tests_properties(assert_disabled PROPERTIES PASS_REGULAR_EXPRESSION "assert skipped")
