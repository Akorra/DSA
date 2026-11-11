#include <dsa/algorithm/search.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <forward_list>
#include <iterator>
#include <vector>


template <class Container>
void test_against_std(const Container& c, const typename Container::value_type& value)
{
    const auto first = c.begin();
    const auto last  = c.end();

    {
        INFO("Testing lower_bound");
        const auto std_result = std::distance(first, std::lower_bound(first, last, value));
        const auto dsa_result = std::distance(first, dsa::lower_bound(first, last, value));
        REQUIRE(std_result == dsa_result);
    }
    {
        INFO("Testing upper_bound");
        const auto std_result = std::distance(first, std::upper_bound(first, last, value));
        const auto dsa_result = std::distance(first, dsa::upper_bound(first, last, value));
        REQUIRE(std_result == dsa_result);  
    }
}

TEST_CASE("lower_bound and upper_bound match std for every size 0..64", "[search]")
{
    // k = 1   : strictly increasing (no duplicates)
    // k = 2,3 : short runs of duplicates
    // k = 5   : longer runs, run length doesn't divide most n
    // k = 64  : every element is 0, the whole array is one run

    constexpr int run_lengths[] = {1, 2, 3, 5, 64};
    constexpr int max_n = 64;

    for(const int k : run_lengths)
    {
        for(int n = 0; n <= max_n; ++n)
        { 
            //sorted data with duplicates:element i is i/k
            std::vector<int> v(static_cast<std::size_t>(n));
            for(int i=0; i<n; ++i)
                v[static_cast<std::size_t>(i)] = i/k;

            const std::forward_list<int> fl(v.begin(), v.end());

            // Smallest element is 0, largest is (n - 1) / k <= n / k.
            // Searching [-1, n/k + 1] covers "below all", every present value,
            // and "above all". For n = 0 it searches -1, 0, 1 on an empty
            // range, so no special case is needed.
            for(int value = -1; value <= n/k + 1; ++value)
            {
                CAPTURE(k, n, value);
                
                {
                    INFO("Testing vector");
                    test_against_std(v, value);
                }
                {
                    INFO("Testing forward_list");
                    test_against_std(fl, value);
                }
            }
        }
    }
}
