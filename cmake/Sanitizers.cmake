# dsa_sanitizers: sanitizer flags for OUR targets only. Link it PRIVATE.
# Which sanitizers are on is chosen by the preset, e.g.
#   -DDSA_SANITIZERS="address;undefined"
# Empty (the default) means no sanitizers: release, bench, debug-nosan.

set(DSA_SANITIZERS "" CACHE STRING
    "Sanitizers for dsa's own targets, ;-separated (e.g. address;undefined)")

add_library(dsa_sanitizers INTERFACE)

if(DSA_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "DSA_SANITIZERS is only wired up for GCC/Clang")
    endif()

    # "address;undefined" -> "address,undefined"
    list(JOIN DSA_SANITIZERS "," _dsa_san_list)

    set(_dsa_san_flags
        -fsanitize=${_dsa_san_list}
        -fno-sanitize-recover=all   # UBSan: abort on the first error instead of print-and-continue
        -fno-omit-frame-pointer)    # readable stack traces in sanitizer reports

    # Both lines: compile inserts the checks, link pulls in the runtime.
    target_compile_options(dsa_sanitizers INTERFACE ${_dsa_san_flags})
    target_link_options(dsa_sanitizers INTERFACE ${_dsa_san_flags})

    message(STATUS "dsa: sanitizers enabled: ${_dsa_san_list}")
endif()
