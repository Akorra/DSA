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

# --- 03_review adversarial tests (Claude, p0-infra round 1) -----------------

# Same Catch2 file three ways; TEST_PREFIX keeps the CTest names distinct.
foreach(variant IN ITEMS on off default)
    set(tgt review_assert_${variant})
    add_executable(${tgt} review_assert.cpp)
    target_link_libraries(${tgt} PRIVATE
        dsa::dsa dsa_warnings dsa_sanitizers Catch2::Catch2WithMain)
    if(variant STREQUAL "on")
        target_compile_definitions(${tgt} PRIVATE DSA_ENABLE_ASSERTS=1)
    elseif(variant STREQUAL "off")
        target_compile_definitions(${tgt} PRIVATE DSA_ENABLE_ASSERTS=0)
    else()
        target_compile_definitions(${tgt} PRIVATE REVIEW_DEFAULT_CONFIG)
    endif()
    catch_discover_tests(${tgt} DISCOVERY_MODE PRE_TEST TEST_PREFIX "assert[${variant}]: ")
endforeach()

dsa_add_death_test(review_assert_noexcept_aborts review_assert_death.cpp
    PASS_REGEX "\\[DSA ASSERTION FAILED\\].*Condition: size > 0.*Message:   pop_front on empty container")
target_compile_definitions(review_assert_noexcept_aborts PRIVATE DSA_ENABLE_ASSERTS=1)

# Compile-parity tests. A target that doesn't compile would break the whole
# build, so these targets are left out of `all` and each CTest test builds
# one of them. RESOURCE_LOCK stops two ninja runs in the same tree at once.
foreach(mode IN ITEMS 0 1)
    foreach(case IN ITEMS BITFIELD NOT_TESTABLE)
        string(TOLOWER ${case} lc)
        set(tgt review_assert_compile_${lc}_${mode})
        add_executable(${tgt} EXCLUDE_FROM_ALL review_assert_compile.cpp)
        target_link_libraries(${tgt} PRIVATE dsa::dsa dsa_warnings)
        target_compile_definitions(${tgt} PRIVATE DSA_ENABLE_ASSERTS=${mode} REVIEW_CASE_${case})
        add_test(NAME ${tgt}
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${tgt})
        set_tests_properties(${tgt} PROPERTIES RESOURCE_LOCK dsa_build_tree)
        if(case STREQUAL "NOT_TESTABLE")
            set_tests_properties(${tgt} PROPERTIES WILL_FAIL TRUE)   # must be rejected
        endif()
    endforeach()
endforeach()
