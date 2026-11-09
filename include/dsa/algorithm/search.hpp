#pragma once

#include <iterator>
#include <functional>
#include <concepts>
#include <dsa/config.hpp>

namespace dsa
{

/**
 * @brief returns the first element for which comp(*it, value) is false, or last if there's none.
 * 
 * @pre The range [first, last) must be partitioned with respect to the expression 
 *      'comp(*it, value)'. That is, all elements for which 'comp(*it, value)' 
 *      is true must precede all elements for which it is false.
 */

template <
    std::forward_iterator ForwardIt, 
    class T, 
    std::predicate<std::iter_reference_t<ForwardIt>, const T&> Compare = std::less<>
>
[[nodiscard]] constexpr ForwardIt lower_bound(ForwardIt first, ForwardIt last, const T& value, Compare comp = {}) 
{
    using difference_type = typename std::iterator_traits<ForwardIt>::difference_type;
    difference_type len = std::distance(first, last);
    while (len > 0) {
        ForwardIt it = first;
        difference_type mid = len / 2;
        std::advance(it, mid);
        if (comp(*it,value)) {
            first = ++it;
            len -= mid + 1;
        } else {
            len = mid;
        }
    }
    return first;
}
} //namespace dsa