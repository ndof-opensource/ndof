#include "ndof/core/object.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

TEST(Object, DeductionGuidesInferCharacterTypeFromAllocatorsAndStrings) {
	const auto text_from_allocator = ndof::text_node{std::allocator<wchar_t>{}};
	const auto attribute_from_allocator = ndof::attribute_node{std::allocator<wchar_t>{}};
	const auto comment_from_allocator = ndof::comment_node{std::allocator<wchar_t>{}};
	const auto object_from_allocator = ndof::basic_object{std::allocator<wchar_t>{}};
	const auto object_from_kind_and_allocator =
		ndof::basic_object{ndof::node_kind::text, std::allocator<wchar_t>{}};
	const auto text_from_string = ndof::text_node{std::wstring{L"text"}};
	const auto attribute_from_strings = ndof::attribute_node{std::wstring{L"name"}, std::wstring{L"value"}};
	const auto comment_from_string = ndof::comment_node{std::wstring{L"comment"}};
	const auto object_from_string = ndof::basic_object{std::wstring{L"object"}};

	using allocator_type = std::allocator<wchar_t>;
	using text_type = ndof::text_node<wchar_t, std::char_traits<wchar_t>, allocator_type>;
	using attribute_type = ndof::attribute_node<wchar_t, std::char_traits<wchar_t>, allocator_type>;
	using comment_type = ndof::comment_node<wchar_t, std::char_traits<wchar_t>, allocator_type>;
	using object_type = ndof::basic_object<wchar_t, std::char_traits<wchar_t>, allocator_type>;

	static_assert(std::same_as<decltype(text_from_allocator), const text_type>);
	static_assert(std::same_as<decltype(attribute_from_allocator), const attribute_type>);
	static_assert(std::same_as<decltype(comment_from_allocator), const comment_type>);
	static_assert(std::same_as<decltype(object_from_allocator), const object_type>);
	static_assert(std::same_as<decltype(object_from_kind_and_allocator), const object_type>);
	static_assert(std::same_as<decltype(text_from_string), const text_type>);
	static_assert(std::same_as<decltype(attribute_from_strings), const attribute_type>);
	static_assert(std::same_as<decltype(comment_from_string), const comment_type>);
	static_assert(std::same_as<decltype(object_from_string), const object_type>);
}

TEST(Object, ExtractMemberRemovesAndReturnsTheNamedMember) {
	ndof::basic_object root;
	ASSERT_NE(root.add_member("member"), nullptr);

	const auto extracted = root.extract_member("member");

	ASSERT_TRUE(extracted.has_value());
	EXPECT_EQ(extracted->first, "member");
	EXPECT_FALSE(root.get_member("member").has_value());
	EXPECT_FALSE(root.extract_member("missing").has_value());
}

TEST(ObjectHelper, ProvidesTypeIndependentNodeBehavior) {
	EXPECT_EQ(ndof::node_kind_name(ndof::node_kind::mapping), NDOF_STR("mapping"));
	EXPECT_EQ(ndof::text_node{std::allocator<char>{}}.kind(), ndof::node_kind::text);
	EXPECT_EQ(ndof::attribute_node{std::allocator<char>{}}.kind(), ndof::node_kind::attribute);
	EXPECT_EQ(ndof::comment_node{std::allocator<char>{}}.kind(), ndof::node_kind::comment);

	auto expected = ndof::node_kind::text;
	auto location = std::source_location::current();
	const ndof::mismatch_state state{expected, 2, "text", location};
	const auto exception = state.to_exception();

	EXPECT_NE(std::string_view(exception.what()).find("expected 'text'"), std::string_view::npos);
	EXPECT_NE(std::string_view(exception.what()).find("variant index: 2"), std::string_view::npos);
}

} // namespace
