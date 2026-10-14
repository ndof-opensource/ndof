#ifndef NDOF_CORE_DEFINITIONS_HPP
#define NDOF_CORE_DEFINITIONS_HPP
// Core definitions for ndof-core
// Add declarations or macro definitions here as needed.
#include <type_traits>

namespace ndof {
        template<class T>
        concept bounded_array = std::is_bounded_array_v<T>;

        template<class T>
        concept unbounded_array = std::is_unbounded_array_v<T>;
} // namespace ndof


#endif // NDOF_CORE_DEFINITIONS_HPP
