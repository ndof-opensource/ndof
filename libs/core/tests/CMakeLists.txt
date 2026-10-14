find_package(GTest REQUIRED CONFIG)

add_executable(unit_tests
    strings_test.cpp
    version_test.cpp)

target_link_libraries(unit_tests PRIVATE
    ndof::core
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
