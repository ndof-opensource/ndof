#ifndef NDOF_CORE_OBJECT_STREAM_HPP
#define NDOF_CORE_OBJECT_STREAM_HPP
#include "object.hpp"
#include <cstddef>
#include <istream>

namespace ndof { 


    // TODO: Figure out how to covert to xml, yaml or json.  Maybe add a formatter for each format and then use the formatter to write to the stream.

    template<typename CharT = char, typename Allocator = std::allocator<std::byte>>
    struct basic_object_iostream :
        public std::basic_iostream<CharT, std::char_traits<CharT>> {
    public:
        using allocator_type = Allocator;
        using object_type = basic_object<CharT, allocator_type>;
        using char_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<CharT>;
        using allocated_string = std::basic_string<CharT, std::char_traits<CharT>, char_allocator>;

        explicit basic_object_iostream(const object_type& object, const allocator_type& allocator = allocator_type())
            : std::basic_iostream<CharT, std::char_traits<CharT>>(nullptr),
              allocator_(allocator),
              object_(object) {
        }

        allocator_type get_allocator() const noexcept {
            return allocator_;
        }

        const object_type& get_object() const noexcept {
            return object_;
        }

    private:
        allocator_type allocator_;
        object_type object_;
    };

    using object_iostream = basic_object_iostream<>;
    

}

#endif // NDOF_CORE_OBJECT_STREAM_HPP
