# dsa_warnings: warning flags for OUR targets only (tests, benchmarks,
# self-containment TUs). Link it PRIVATE. Never attach it to dsa::dsa, or every
# consumer would inherit our warnings and -Werror.

option(DSA_WARNINGS_AS_ERRORS "Treat warnings as errors on dsa's own targets" ON)

set(_dsa_warn_common            # GCC and Clang
    -Wall -Wextra -Wpedantic
    -Wshadow                    # local hides another name
    -Wconversion                # implicit narrowing (size_t -> int, double -> float)
    -Wsign-conversion           # implicit signed <-> unsigned
    -Wnon-virtual-dtor          # polymorphic class without virtual dtor
    -Woverloaded-virtual        # derived overload hides base virtual
    -Wold-style-cast            # (T)x instead of a named cast
    -Wnull-dereference          # provable null deref (sees more with optimisation)
    -Wdouble-promotion          # float silently promoted to double
    -Wformat=2                  # printf-family format/argument checks
    -Wimplicit-fallthrough)     # switch case falls through without [[fallthrough]]

set(_dsa_warn_gcc               # GCC only
    -Wduplicated-cond           # if (a) ... else if (a)
    -Wduplicated-branches       # if/else with identical bodies
    -Wlogical-op                # suspicious && / || (e.g. x == 1 && x == 2)
    -Wcast-align=strict)        # cast increases required alignment; plain -Wcast-align
                                # is silent on x86-64 because misaligned loads work there

set(_dsa_warn_clang             # Clang only
    -Wcast-align)               # Clang rejects the =strict form

set(_dsa_warn_msvc /W4 /permissive-)

add_library(dsa_warnings INTERFACE)

# A generator expression must be ONE argument, so the lists go in quoted;
# the ';' inside them still expands to separate flags.
target_compile_options(dsa_warnings INTERFACE
    "$<$<CXX_COMPILER_ID:GNU,Clang>:${_dsa_warn_common}>"
    "$<$<CXX_COMPILER_ID:GNU>:${_dsa_warn_gcc}>"
    "$<$<CXX_COMPILER_ID:Clang>:${_dsa_warn_clang}>"
    "$<$<CXX_COMPILER_ID:MSVC>:${_dsa_warn_msvc}>"
    # -Werror / /WX, switchable with -DDSA_WARNINGS_AS_ERRORS=OFF
    "$<$<BOOL:${DSA_WARNINGS_AS_ERRORS}>:$<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>>")
