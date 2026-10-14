#include "ndof/core/object.hpp"

namespace ndof {

detail::node_helper::node_helper(node_kind kind) noexcept
	: kind_(kind) {
}

node_kind detail::node_helper::kind() const noexcept {
	return kind_;
}

default_string_view node_kind_name(node_kind kind) noexcept {
	switch (kind) {
		case node_kind::undefined: return NDOF_STR("undefined");
		case node_kind::null:      return NDOF_STR("null");
		case node_kind::element:   return NDOF_STR("element");
		case node_kind::attribute: return NDOF_STR("attribute");
		case node_kind::text:      return NDOF_STR("text");
		case node_kind::sequence:  return NDOF_STR("sequence");
		case node_kind::mapping:   return NDOF_STR("mapping");
		case node_kind::comment:   return NDOF_STR("comment");
	}
	return NDOF_STR("invalid");
}

std::logic_error mismatch_state::to_exception() const {
	return std::logic_error(
		std::string("Node kind does not match variant type: expected '")
		+ std::string(expected_name)
		+ "', variant index: "
		+ std::to_string(variant_index)
		+ " at "
		+ location.file_name()
		+ ": "
		+ std::to_string(location.line())
		+ ": "
		+ std::to_string(location.column()));
}

} // namespace ndof

