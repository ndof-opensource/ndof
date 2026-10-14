# ndof-error

Design-by-contract and error-handling framework for the ndof framework family

Depends on `ndof-core`; see [its README](../core/README.md) to consume it.

Part of the ndof framework family. Requires C++23 (GCC 13+, Clang 17+, MSVC 19.36+).

Licensed under the [Apache License 2.0](../../LICENSE).

## Development

### Motivation for this section (Why would I do this?)

This section explains how to initially build and test this library
for local development and contribution.

This library lives in the [ndof repository](https://github.com/ndof-opensource/ndof)
under `libs/error/`, alongside the other ndof libraries. The environment,
build, and contribution guide are in that repository's README and
[docs/onboarding.md](../../docs/onboarding.md). To get started, after
cloning this repository, from the repository root run:

```sh
scripts/build.sh error debug   # resolve deps + configure + build + test
```

Other presets:
`release`, `asan`, `tsan`; switch compilers with `--profile linux-clang19`.
CI runs the same presets on Linux (GCC + Clang), macOS, and Windows, plus
clang-format, clang-tidy, and cppcheck gates.

If this library's ndof dependencies are already built, the library-specific
build.sh script can also be run from `libs/error`: `scripts/build.sh debug`.

## Consuming

### Motivation for this section (Why would I do this?)

This section explains how a third-party developer can consume the
`ndof-error` library as a dependency, either as a Conan package, as
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
    self.requires("ndof-error/0.1.0")
```

and in CMake, with Conan's `CMakeDeps` generator:

```cmake
find_package(ndof-error REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE ndof::error)
```

Only recipes and sources are published; `conan install --build=missing`
builds the library with your profile.

### As a plain CMake package (no package manager)

Build and install the library from a release tag, then point your
project at the install prefix:

```sh
git clone --branch error/v0.1.0 https://github.com/ndof-opensource/ndof
cmake -S ndof/libs/error -B build -DNDOF_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /opt/ndof
```

```cmake
# configure your project with -DCMAKE_PREFIX_PATH=/opt/ndof
find_package(ndof-error REQUIRED CONFIG)
target_link_libraries(your_target PRIVATE ndof::error)
```

If this library depends on other ndof libraries, install those first
the same way; `find_package` resolves them from the same prefix.

### From source inside your build

```cmake
include(FetchContent)
FetchContent_Declare(ndof-error
    GIT_REPOSITORY https://github.com/ndof-opensource/ndof
    GIT_TAG error/v0.1.0
    SOURCE_SUBDIR libs/error)
FetchContent_MakeAvailable(ndof-error)
target_link_libraries(your_target PRIVATE ndof::error)
```

Tests are not built and warnings are not escalated when the library is
not the top-level project. Fetch dependencies before dependents.
