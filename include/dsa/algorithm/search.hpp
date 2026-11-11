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

template < std::forward_iterator ForwardIt, class T, 
    std::predicate<std::iter_reference_t<ForwardIt>, const T&> Compare = std::less<> >
[[nodiscard]] constexpr ForwardIt lower_bound(ForwardIt first, ForwardIt last, const T& value, Compare comp = {}) 
{
    using difference_type = typename std::iterator_traits<ForwardIt>::difference_type;
    
    // Calculate the length of the range [first, last)
    difference_type len = std::distance(first, last);
    
    while (len > 0) 
    {
        ForwardIt it = first;          // Create a copy of 'first' to avoid modifying it directly
        difference_type mid = len / 2; // Calculate the midpoint of the current range
        std::advance(it, mid);         // Move 'it' to the midpoint position
        
        // Compare the value at 'it' with the target 'value' using the provided comparator
        // If 'comp(*it, value)' is true,  it means the target value is in the upper half of the range
        // If 'comp(*it, value)' is false, it means the target value is in the lower half of the range
        if (comp(*it,value))
        {
            first = ++it;   // Move 'first' to the position after 'it' since the target value is in the upper half
            len -= mid + 1; // Update the length of the range to reflect the new search space
        } 
        else 
        {
            len = mid; // Update the length of the range to reflect the new search space in the lower half
        }
    }

    return first; // Return the position of the first element that is not less than 'value'
}

template < std::forward_iterator ForwardIt, class T, 
    std::predicate<const T&, std::iter_reference_t<ForwardIt>> Compare = std::less<> >
[[nodiscard]] constexpr ForwardIt upper_bound(ForwardIt first, ForwardIt last, const T& value, Compare comp = {}) 
{
    using difference_type = typename std::iterator_traits<ForwardIt>::difference_type;
    
    // Calculate the length of the range [first, last)
    difference_type len = std::distance(first, last);
    
    while (len > 0) 
    {
        ForwardIt it = first;          // Create a copy of 'first' to avoid modifying it directly
        difference_type mid = len / 2; // Calculate the midpoint of the current range
        std::advance(it, mid);         // Move 'it' to the midpoint position
        
        // Compare the value at 'it' with the target 'value' using the provided comparator ( *it <= target value ) = !( *it > target value )
        // If 'comp(value, *it)' is false, it means the target value is in the upper half of the range
        // If 'comp(value, *it)' is true,  it means the target value is in the lower half of the range
        if (!(comp(value,*it)))
        {
            first = ++it;   // Move 'first' to the position after 'it' since the target value is in the upper half
            len -= mid + 1; // Update the length of the range to reflect the new search space
        } 
        else 
        {
            len = mid; // Update the length of the range to reflect the new search space in the lower half
        }
    }

    return first; // Return the position of the first element that is not less than 'value'
}
} //namespace dsa