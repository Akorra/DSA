// With asserts on: must abort and print condition, message, file:line.
// With asserts off: the macro must compile to nothing and the program continues.
#include <dsa/detail/assert.hpp>

#include <cstdio>

int main()
{
    DSA_ASSERT(1 + 1 == 3, "math");
    std::puts("assert skipped");
    return 0;
}
