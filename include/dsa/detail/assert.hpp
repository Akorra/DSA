#pragma once

#include <dsa/config.hpp>
#include <cstdio>
#include <cstdlib>


namespace dsa::detail{

// assert_fail:
// - [[noreturn]] promises function never returns. compiler can treat code after failed check as unreachable.
// - [[gnu::cold]] tells GCC/Clang this function rarely runs, so it can be moved out of hot code.
// - inline tells linker copies are identical and to keep one..
[[noreturn]] [[gnu::cold]] inline void assert_fail(
    const char* condition,
    const char* message,
    const char* file,
    int line)
{
    // std::fprintf to avoid iostream overhead
    // stderr unbuffered guarantees write before abort()
    std::fprintf(stderr,
                 "[DSA ASSERTION FAILED]\n"
                 "Condition: %s\n"
                 "Message:   %s\n"
                 "Source:    %s:%d\n",
                 condition, message, file, line);
    std::abort();
}
} //namespace dsa::detail

// macro inside do{}while(0) so it behaves as a single statement.
// cond between () to maintain operation order 
// conditions with a comma, outside parentheses has to be wrapped in an extra pair, example: '(std::pair<int,int>)'
#if DSA_ENABLE_ASSERTS
#   define DSA_ASSERT(cond, msg) \
        do { \
            if (!(cond)) [[unlikely]] { \
                ::dsa::detail::assert_fail(#cond, msg, __FILE__, __LINE__); \
            } \
        } while (0)
#else
#   define DSA_ASSERT(cond, msg) ((void)sizeof(cond), (void)sizeof(msg)) // compile cond silently, but discard code generation or runtime evaluation
#endif