# Releasing and consuming ndof Conan packages

How a library version gets published and how other libraries consume it.
For the reasoning behind the setup, see `docs/decisions.md`.

## Where packages live

- Host: Cloudsmith (free open-source hosting; see attribution in the
  README). Workspace `ndof-opensource`, repository `packages`.
- Conan remote URL: `https://conan.cloudsmith.io/ndof-opensource/packages/`
- Client alias used everywhere in this project: `ndof-public` (a local
  nickname, like git's `origin`; Cloudsmith never sees it).
- Reads are anonymous. Writes need the `ndof-ci` service account, whose
  credentials exist as the org-level GitHub secrets `CLOUDSMITH_USER` and
  `CLOUDSMITH_API_KEY`. Publishing runs only on tag pushes in our own
  repos; fork PRs never see secrets.

We publish recipes and sources only, no prebuilt binaries. Every consumer
(including our CI) builds locally via `--build=missing`, exactly as they
already do for gtest.

Packages are published only by `.github/workflows/publish.yml`, on a
version tag naming one library (`core/v0.1.2`). `publish.yml` doesn't
build anything itself. Instead, it simply
exports that library's recipe and sources from the tagged commit and
uploads them. The build-and-test proof lives in CI's `package` job,
which runs on every pull request and on every merge to `main`, and must pass before a PR can merge.
That `package` job runs `conan create` for every library on every
change before it can reach
`main`. Each library is versioned and released on its own.

## Cutting a release

### Motivation for this section (Why would I do this?)

You have finished work on a new version of a library and want to publish that version to Cloudsmith for public consumption.

### Version Number

The `version` field in the associated `libs/<name>/conanfile.py` equals the library's
last published version until the first change to the library after that
release bumps it; from then on it names the version that will be published
*next*. A never-published library carries the initial `0.1.0`, and its
first release is simply `<name>/v0.1.0`; no bump is needed. CI enforces
the rule (the `format` job): a PR that changes a library's packaged files
(`CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `include/`, `src/`,
`tests/`, `conanfile.py`) while its version still equals its latest
release tag fails until the version is bumped, and a `requires` on another
ndof library must name that library's current working version.

### The Procedure

1. Confirm `main` builds green and `version` in `libs/<name>/conanfile.py`
   is the number you intend to publish. If the library `requires` other
   ndof libraries, every version it names must already be published
   (`conan list "ndof-core/0.1.1" -r ndof-public`); the publish workflow
   refuses otherwise, because a consumer could not resolve the result.
   Everything that ships is already on `main` via merged PRs; nothing is
   committed directly to `main` (the branch ruleset forbids it), so there
   is no local commit step.
2. Tag the merge commit on `main` with the library's name and version, and
   push the tag. Locally, `pull` fetches that commit before you tag it:

   ```sh
   git switch main && git pull
   git tag core/v0.1.2
   git push origin core/v0.1.2
   ```

   Or with no local git at all: GitHub → Releases → "Draft a new
   release" → choose tag `core/v0.1.2` targeting `main` → publish. A tag
   created there fires the same workflow.

3. The `publish` workflow runs automatically: it resolves the library from
   the tag, verifies the tag matches that library's conanfile version,
   verifies its ndof dependencies are published, exports the recipe and
   sources from the tagged commit (`conan export libs/<name>`), and
   uploads them to `ndof-public`.
4. Verify from any dev container:

   ```sh
   conan search "ndof-*" -r ndof-public
   ```

5. Do not bump the version now. The first PR that changes the library's
   packaged files bumps it (CI refuses that PR otherwise), so a release
   always has content behind its number, and a dependent can be released
   without an empty release of an unchanged dependency. Under semver's
   0.y.z rules any bump is allowed before 1.0.0; bump the middle number
   when the public API changes shape. Dependents' `requires` move to the
   new version in the same PR (CI enforces that they name the current working version), which is what makes every commit test the whole
   tree against itself.

Versions are immutable once published: never move or delete a tag; ship a
fix as the next version. The tag/version guard fails the workflow if the
two disagree, and the tag ruleset (`*/v*`: force-pushes and deletions
blocked) makes a published tag unmovable server-side. Releasing several
libraries is several tags; each runs its own publish.

## Consuming a released library

### Motivation for this section (Why would I do this?)

In working on one ndof library, you have established a new dependency
on another ndof library. This section builds that connection from
dependent library to dependency library. Steps 1-3 only need to be done
once per library dependency. Step 4, of course, must be done in each
source file that has a dependency on the other library.

Note: Instructions for how outside code should consume an ndof library are found in each library's individual README.

### The Procedure

Wiring one ndof library into another takes one edit in each of four
files in the consuming library, using core-into-error as the example:

1. `conanfile.py`: declare the dependency.

   ```python
   def requirements(self):
       self.requires("ndof-core/0.1.1")
   ```

   **Placement:** Position among the methods of `class Package(ConanFile)` does not
   matter to Conan; the convention here is right above
   `build_requirements()`, its test-dependency sibling.

2. Top-level `CMakeLists.txt`: locate the package and link it.

   ```cmake
   if(NOT TARGET ndof::core)
       find_package(ndof-core REQUIRED CONFIG)
   endif()
   target_link_libraries(error PUBLIC ndof::core)
   ```

   **Note about visibility:** Use `PUBLIC` when the consumer's own public headers include
   ndof-core headers (the requirement must propagate to *its*
   consumers); `PRIVATE` when only the consumer's `src/` files do.
   This follows the visibility tier structure established by
   [Decision 14](decisions.md#14-header-layout-three-visibility-tiers).

   **Note about the `if` guard:** The `if` is for outside consumers
   who build both ndof libraries from source in their own CMake
   project (via FetchContent or add_subdirectory). In such a
   scenario, the ndof::core target *already* exists but no installed
   config file does, so find_package would fail.

   **Placement:** `find_package` goes in the preamble, after `project()` (it needs
   the toolchain that `project()` activates); put it with the
   `include(...)` lines. The `target_link_libraries` call goes with
   the other `target_*(error ...)` calls, anywhere after
   `add_library(error ...)`. Placing that `target_link_libraries` call next to `target_compile_features` reads
   naturally. The only hard ordering rules: `find_package` after
   `project()`, and before any use of `ndof::core`.

3. `cmake/config.cmake.in`: make the installed package resolve its
   dependency for plain-CMake consumers (decision 6). Conan consumers
   get this from CMakeDeps; the installed package config does not.

   ```cmake
   @PACKAGE_INIT@

   include(CMakeFindDependencyMacro)
   find_dependency(ndof-core)

   include("${CMAKE_CURRENT_LIST_DIR}/ndof-error-targets.cmake")
   ```

4. The source file:

   ```cpp
   #include <ndof/core/strings.hpp>
   ```

Then run `scripts/build.sh error debug` from the repository root. It
packages core from `libs/core` into the local cache before building
error, so nothing is downloaded.
Version bumps in consumers are ordinary PRs whose
CI proves the chain.

How the include path reaches the compiler: during `conan install`, the
CMakeDeps generator writes a `ndof-core-config.cmake` into
`build/<Type>/generators/`, and the toolchain file steers
`find_package` to it. That generated config defines the imported
target `ndof::core` carrying the header and library locations;
`target_link_libraries` propagates them, and CMake emits the include
flag and the link line from there. The package and target names are
declared by ndof-core's own `package_info()` (`cmake_file_name`,
`cmake_target_name`), not by convention.

The same edits serve editable mode below unchanged. The only thing
that changes is where Conan finds core: the local Conan cache (packaged
from `libs/core`) by default, or the `libs/core` directory itself in
editable mode.

## Live cross-library development

### Motivation for this section (Why would I do this?)

This section describes two ways to work on multiple ndof libraries
simultaneously. As an example, if you were
working on `ndof-error` and realized you needed to make associated
changes to its dependency `ndof-core`, do one of these two
procedures. 

### 1. Default build

No special procedure is necessary. From the repository root, simply run:

```sh
scripts/build.sh error debug
```

This builds core from `libs/core` into the local Conan cache, then builds
error against it. Edit either library and rerun the same command as
you iterate.

### 2. Editable mode

Conan has an editable mode, in which the consuming library
(`ndof-error` in the example) is built against core's files in
`libs/core` and its build tree in `libs/core/build/Debug`, *instead of*
against the packaged copy in Conan cache. The difference is subtle,
but the advantage is in debugging. When debugging error and stepping
into core, breakpoints and step-into, and go-to-definition
land in the files you are
actually editing instead of a copy under `~/.conan2`. Our `build.sh`
automatically sets up all ndof libraries as `conan editable` with the
`--editable` flag:

```sh
scripts/build.sh --editable error debug
```

That registers every library as a Conan editable. Any later
scripts/build.sh run without --editable removes those registrations
and returns to the packaged copy, so there is no cleanup step. The
registrations and the Conan cache live in the container, so a container
rebuild resets them too. CI never uses editable mode.

One asymmetry to keep in mind: editable mode bypasses the recipe's
`package()` step entirely, serving headers straight from the
dependency's `libs/<name>` directory. A header accidentally left out of packaging (e.g. by being placed in `src/` rather than `include/`
or in a new top-level directory, which should not happen) therefore works
fine in editable mode and breaks only for real consumers. CI's `package`
job (`conan create` for every library, in dependency order, on every
change) exists to catch exactly that, so trust a green `ci / package` over
"it worked in editable".

Shipping: the dependency's changes and the consumer's land in the same PR.
A consumer that `requires` an unpublished dependency version builds and
tests green in CI (its dependencies are packaged from the same commit),
but cannot be *released* until that dependency version is published; the
publish workflow enforces the order.

## Cloudsmith setup notes (in case a new repository needs to be created)

- A repository's Open-Source type is chosen at creation: select
  "Broadcast" visibility first, which unlocks the Open-Source option.
  The type is what grants the free OSS tier; plain Public is not it.
- Repository slugs (URL-compatible names) are permanent in practice: they appear in every
  pinned URL. 
- The service account needs write access to the one repository only.
- Cloudsmith's OSS hosting policy requires attribution with a link back
  to cloudsmith.com; ours lives in this repo's README. It also requires
  that the served artifacts are the project's own, which the `ndof-$LIB`
  upload filter enforces mechanically.
