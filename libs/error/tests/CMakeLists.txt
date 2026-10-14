find_package(GTest REQUIRED CONFIG)

add_executable(error_unit_tests
    allocate_unique_test.cpp
    conditions_check_test.cpp
    version_test.cpp)

target_link_libraries(error_unit_tests PRIVATE
    ndof::error
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(error_unit_tests)

add_executable(no_exception_allocate_unique_test
    no_exception_allocate_unique_test.cpp)

target_link_libraries(no_exception_allocate_unique_test PRIVATE
    ndof::error)

target_compile_definitions(no_exception_allocate_unique_test PRIVATE
    NDOF_EXCEPTIONS_ENABLED=0)

target_compile_options(no_exception_allocate_unique_test PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/EHs-c->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions>)

add_test(NAME AllocateUnique.NoExceptions
    COMMAND no_exception_allocate_unique_test)
