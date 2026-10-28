// Proves the harness works: Catch2 links, dsa headers are found, CTest sees
// each TEST_CASE. Nothing about the library itself is tested here.
#include <catch2/catch_test_macros.hpp>

#include <dsa/config.hpp>

TEST_CASE("harness: catch2 runs", "[smoke]")
{
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("harness: dsa headers are on the include path", "[smoke]")
{
    STATIC_REQUIRE(DSA_VERSION == DSA_VERSION_MAJOR * 10000
                                  + DSA_VERSION_MINOR * 100
                                  + DSA_VERSION_PATCH);
}
