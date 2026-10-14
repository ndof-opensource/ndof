cmake_minimum_required(VERSION 3.28)

# Conan generates the toolchain file this project is configured with. When a
# tool (an IDE's CMake integration, a bare cmake invocation) configures an
# unbootstrapped tree, fail with the fix instead of cryptic errors.
if(CMAKE_TOOLCHAIN_FILE AND NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(FATAL_ERROR
        "Conan has not generated ${CMAKE_TOOLCHAIN_FILE} yet. Run "
        "scripts/build.sh debug (or: conan install . --build=missing) first. "
        "See this repository's README, or the ndof-infra onboarding guide.")
endif()

project(ndof-core
    VERSION 0.1.1
    DESCRIPTION "Foundational utilities for the ndof framework family"
    HOMEPAGE_URL "https://github.com/ndof-opensource/ndof"
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# This project does not currently use C++20 modules;
# CMake 3.28+ module scanning also injects compiler flags that
# clang-tidy cannot parse.
# If C++20 modules are introduced, delete this line.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(NDOF_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
# OFF by default so source builds by consumers never break on a newer
# compiler's warnings; the team's CMake presets and CI turn it ON.
option(NDOF_WERROR "Treat compiler warnings as errors" OFF)

add_library(core
    src/configs.cpp
    src/object.cpp
    src/strings.cpp
    src/version.cpp)
add_library(ndof::core ALIAS core)

set_target_properties(core PROPERTIES
    OUTPUT_NAME ndof-core)

target_compile_features(core PUBLIC cxx_std_23)

target_include_directories(core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_definitions(core PRIVATE
    NDOF_LIBRARY_NAME="ndof-core"
    NDOF_LIBRARY_VERSION="${PROJECT_VERSION}")

target_compile_options(core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- $<$<BOOL:${NDOF_WERROR}>:/WX>>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Wconversion -Wshadow $<$<BOOL:${NDOF_WERROR}>:-Werror>>)

if(NDOF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- installation (plain CMake package; no package-manager assumptions) -----
install(TARGETS core EXPORT ndof-core-targets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ndof-core-targets
    NAMESPACE ndof::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ndof-core)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ndof-core-config-version.cmake
    COMPATIBILITY SameMinorVersion)

configure_package_config_file(
    cmake/config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ndof-core-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ndof-core)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ndof-core-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ndof-core-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ndof-core)
