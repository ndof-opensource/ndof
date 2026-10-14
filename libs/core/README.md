# ndof-core

Foundational utilities for the ndof framework family

Part of the ndof framework family. Requires C++23 (GCC 13+, Clang 17+, MSVC 19.36+).

Licensed under the [Apache License 2.0](../../LICENSE).

## Development

### Motivation for this section (Why would I do this?)

This section explains how to initially build and test this library
for local development and contribution.

This library lives in the [ndof repository](https://github.com/ndof-opensource/ndof)
under `libs/core/`, alongside the other ndof libraries. The environment,
build, and contribution guide are in that repository's README and
[docs/onboarding.md](../../docs/onboarding.md). To get started, after
cloning this repository, from the repository root run:

```sh
scripts/build.sh core debug   # resolve deps + configure + build + test
```

Other presets:
`release`, `asan`, `tsan`; switch compilers with `--profile linux-clang19`.
CI runs the same presets on Linux (GCC + Clang), macOS, and Windows, plus
clang-format, clang-tidy, and cppcheck gates.

If this library's ndof dependencies are already built, the library-specific
build.sh script can also be run from `libs/core`: `scripts/build.sh debug`.

## Consuming

### Motivation for this section (Why would I do this?)

This section explains how a third-party developer can consume the
`ndof-core` library as a dependency, either as a Conan package, as
a plain CMake package, or from source inside your build.

### As a Conan package

The ndof libraries are published to a public Conan remote (reads are
anonymous). Add it once per machine:

```sh
conan remote add ndof-public https://conan.cloudsmith.io/ndof-opensource/packages/
```

Then in your `conanfile.py`:

```python
def requirements(self):
    self.requires("ndof-core/0.1.1")
```

and in CMake, with Conan's `CMakeDeps` generator:

```cmake
find_package(ndof-core REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE ndof::core)
```

Only recipes and sources are published; `conan install --build=missing`
builds the library with your profile.

### As a plain CMake package (no package manager)

Build and install the library from a release tag, then point your
project at the install prefix:

```sh
git clone --branch core/v0.1.1 https://github.com/ndof-opensource/ndof
cmake -S ndof/libs/core -B build -DNDOF_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /opt/ndof
```

```cmake
# configure your project with -DCMAKE_PREFIX_PATH=/opt/ndof
find_package(ndof-core REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE ndof::core)
```

If this library depends on other ndof libraries, install those first
the same way; `find_package` resolves them from the same prefix.

### From source inside your build

```cmake
include(FetchContent)
FetchContent_Declare(ndof-core
    GIT_REPOSITORY https://github.com/ndof-opensource/ndof
    GIT_TAG core/v0.1.1
    SOURCE_SUBDIR libs/core)
FetchContent_MakeAvailable(ndof-core)
target_link_libraries(your_target PRIVATE ndof::core)
```

Tests are not built and warnings are not escalated when the library is
not the top-level project. Fetch dependencies before dependents.
