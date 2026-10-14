# Developer onboarding

How to go from a fresh machine to building, testing, and contributing — and
enough theory about each moving part that nothing feels like magic. If you
want the *reasons* behind the setup, read [decisions.md](decisions.md).

## The mental model in one paragraph

This repository holds the ndof libraries, one self-contained CMake project
and Conan recipe per directory under `libs/`. The team **environment** (a
Dockerfile and Conan profiles) is defined in
[ndof-infra](https://github.com/ndof-opensource/ndof-infra) and published as
a container image identified by an immutable digest; this repository pins
that digest in `.devcontainer/devcontainer.json`, and CI reads it from
there. So every commit records exactly which environment built it, any
machine can reproduce it, and changing the environment is a reviewed pull
request in ndof-infra followed by a one-line pin update here.

## Day one

1. Install Docker and clone this repository.
2. Open the folder in a devcontainer-capable editor or the `devcontainer` CLI. Accept
   the "open in container" prompt. Your terminal is now inside the pinned
   environment: GCC 14, Clang 19, CMake, Ninja, Conan 2, team profiles.
3. Build and test a library, from the repository root:

   ```sh
   scripts/build.sh core debug
   ```

   Both scripts print every command they run. The root script packages the
   library's ndof dependencies into the local Conan cache (none, for core),
   then runs the shared `scripts/lib-build.sh debug` inside `libs/core/`,
   which wraps this raw sequence:

   ```sh
   conan install . --build=missing -pr:a /opt/conan/profiles/linux-gcc14 -s build_type=Debug
   cmake --preset debug          # configure
   cmake --build --preset debug  # compile
   ctest --preset debug          # run unit tests
   ```

   `scripts/build.sh all debug` builds every library in dependency order.
   In the editor, Cmd/Ctrl+Shift+B builds the library that owns the file
   you have open.

That is the entire onboarding. Nothing is installed on your host.

For editing a library and one of its ndof dependencies together, see
"Live cross-library development" in [releasing.md](releasing.md); simply it is
`scripts/build.sh --editable`.

## Conan in five minutes

C++ has no built-in package ecosystem, and compiled C++ libraries are only
linkable against binaries built with a *compatible configuration* — same
architecture, compatible compiler, same standard library, same build type.
So a C++ package manager must track versions × configurations, not versions
alone.

Conan's model:

- Each repo's `conanfile.py` declares what it needs
  and how this library itself is packaged.
- A **profile** (see `conan/profiles/`, baked into the image at
  `/opt/conan/profiles/`) is the written answer to "compatible with what?" —
  one line per ABI axis: `os`, `arch`, `compiler`, `compiler.version`,
  `compiler.libcxx` (which C++ standard library — both our profiles use
  libstdc++ so GCC and Clang builds share one), `compiler.cppstd=23`.
- `conan install` hashes recipe + settings into a **package ID** per
  dependency, then finds a matching binary: local cache → remotes → build
  from source (`--build=missing`). Debug and Release gtest are different
  package IDs; both live in the cache side by side.
- It then generates `build/<Type>/generators/conan_toolchain.cmake`, which
  carries the compiler choice and standard into our own build. **The profile
  is the single source of truth for the toolchain** — switching compilers is
  switching profiles, nothing else.
- A **remote** is a package server. ConanCenter (public, anonymous)
  supplies third-party packages such as gtest. The public `ndof-public`
  remote supplies the ndof libraries themselves, so
  `requires = "ndof-core/x.y.z"` resolves across repos (see
  [releasing.md](releasing.md)). Inside this repository nothing is
  downloaded from that remote: `scripts/build.sh` runs `conan create`
  on each ndof dependency from `libs/` first, so the consumer library finds it
  in the local cache. The `--editable` option goes one step further for
  live cross-library editing: Conan reads the dependency's `libs/<name>`
  directory directly, so edits there are seen on the next build without
  the need to re-run `conan create`, as would otherwise be the case.
- Cross-compilation, when it arrives, is a profile *pair*:
  `-pr:h <target> -pr:b <build machine>` — host profile describes where
  artifacts run, build profile the machine building them. Same file format,
  more files in `conan/profiles/`.

## CMake presets

`CMakePresets.json` names the sanctioned configurations so humans and CI type
identical commands: `debug`, `release`, and sanitizer variants `asan`, `tsan`
(these deliberately reuse the *Debug* Conan install — their toolchain file
points into `build/Debug/generators/`). Each has matching build and test
presets; `ctest` is CMake's test runner, which discovers the gtest suites.

## How editor tooling fits together

The chain behind code intelligence, in order:

1. `conan install` (the first step of `scripts/build.sh`) generates the
   toolchain file; `cmake --preset debug` then writes
   `build/Debug/compile_commands.json`, which records the exact compiler
   command for every `.cpp` file.
2. **clangd**, the team language server, reads that file. It embeds
   the Clang compiler frontend, so its diagnostics are the compiler's
   opinion, fed by the real build's flags. The repo's `.clangd` file
   points it at the owning library's `libs/<name>/build/Debug` (its default search never looks there) and
   forces `-std=c++23` for brand-new headers that no compiled file
   includes yet. Every clangd-capable editor honors `.clangd`; nothing
   editor-specific is required for code intelligence.
3. clangd also surfaces the checks in `.clang-tidy` inline, and
   `.clang-format` drives formatting. Together those three files are the
   editor-agnostic tooling config; `customizations.vscode` in
   `devcontainer.json` carries VS Code niceties only (the cpptools
   extension for its gdb debugger, with its own IntelliSense engine
   disabled so clangd stays the single language-services engine).
4. There are **two build lanes** in an IDE. The task lane
   (Ctrl/Cmd+Shift+B, or "Tasks: Run Build Task") runs
   `scripts/build.sh` for the library owning the active file and is the
   canonical path; it can never build
   against a stale or unbootstrapped tree, as it checks for and runs
   these steps itself. The CMake-integration lane
   ("CMake: Build", the status-bar button) calls CMake directly and only
   works after the first `build.sh` run has generated the toolchain
   file; on an unbootstrapped tree it fails with a one-line message
   saying to run `build.sh`, and auto-configure on folder open is
   disabled for the same reason.

Fresh-clone rule of thumb: run `scripts/build.sh <library> debug` once, then
restart the language server if the editor still shows stale errors.

## Where does a new header go?

Three questions, asked in order (decision 14 has the full reasoning):

1. **Will users call what is in it?** Then
   `include/ndof/<lib>/`. This is the public API: documented,
   semver-governed, reviewed as a promise.
2. **Do public headers need to include it, even though users will never
   call it directly?** Then `include/ndof/<lib>/details/`, in namespace
   `ndof::<lib>::details`. Template machinery, traits, storage types: it
   ships, because consumers' compilers must be able to read it, but
   nothing under `details/` is supported API and all of it may change
   without notice.
3. **Neither?** Then `src/`, next to the sources that use it. It is
   never installed; consumers cannot include what does not exist on
   their machines.

The install rule is the enforcement: `install(DIRECTORY include/)`
ships tiers 1 and 2 and nothing else. Promoting a header from `src/`
into `details/` (because a public header now needs it) grows the shipped
surface; review it like an API change.

## The CI gates, and how to reproduce each locally

CI lives in `.github/workflows/ci.yml` and runs every gate over every library
under `libs/`, in dependency order (computed by `scripts/build.sh --list`
from each conanfile's `requires`; there is no hand-kept list). Every push and pull request runs:

| Job | Proves | Reproduce locally (in the container, from the repository root) |
|---|---|---|
| `format` | Code matches `.clang-format`; implementation-tier directories are spelled `details/`; versions and `requires` bump properly (see releasing.md) | `git ls-files '*.cpp' '*.hpp' \| xargs clang-format --dry-run -Werror` |
| `linux` (GCC & Clang × Debug & Release) | Every library builds and tests in the canonical environment, against the packaged form of its ndof dependencies | `scripts/build.sh all debug` / `--profile linux-clang19` / `release` |
| `sanitize` | No memory errors or undefined behavior at runtime (ASan + UBSan) | `scripts/build.sh all asan` |
| `tidy` | Static-analysis clean per `.clang-tidy`, warnings as errors | `run-clang-tidy -p libs/<name>/build/Debug -warnings-as-errors='*'` |
| `cppcheck` | A second, independent static analyzer also finds nothing | `cppcheck --project=libs/<name>/build/Debug/compile_commands.json --library=googletest --enable=warning,style,performance,portability --error-exitcode=1` |
| `package` | Each Conan recipe works on its own: export, build, test, and install as a package, in dependency order | `conan create libs/<name> --build=missing -pr:a /opt/conan/profiles/linux-gcc14 -s build_type=Release` |
| `plain-cmake` | Every library installs and is consumable with plain CMake and no package manager (decision 6): all libraries installed to one prefix, then a scratch consumer built with `find_package` alone | `cmake -S libs/<name> -B build-plain -DNDOF_BUILD_TESTS=OFF && cmake --build build-plain && cmake --install build-plain --prefix /tmp/ndof` |
| `macos` / `windows` | Portability to AppleClang and MSVC | native runners only — CI is the arbiter |

The `package` job is also the gate that keeps libraries honest with each
other: it builds each library from its own exported sources, so an include
that reaches into a sibling's tree (anything outside what that sibling
installs) compiles in the editor and fails here.

The objective of CI (**Continuous Integration**): every change is built and
tested automatically before it lands, so integration breakage surfaces in
minutes and quality is the default path, not a discipline. The **CD**
(Continuous Delivery) side is `build-image.yml` in ndof-infra, which
publishes the environment image on merge there, and `publish.yml` here,
which publishes one library's Conan package when its version tag is pushed
(see [releasing.md](releasing.md)).

## Changing the environment: the one pin

The environment reaches this repository through exactly one pinned
reference: the image digest in `.devcontainer/devcontainer.json`. Local
editors start the container from it, and CI's `digest-sync` job reads the
same line to choose the container for every Linux job, so local and CI
environments cannot disagree. Nothing else in this repository pins the
environment.

What each kind of change requires:

| Change | Where | Then |
|---|---|---|
| compilers, tools, Conan profiles | a PR to ndof-infra (`image/Dockerfile`, `conan/profiles/`); `build-image.yml` there publishes the new image and prints its digest | one PR here changing the digest line; its CI running green against the new image *is* the adoption test |
| CI logic | a PR here to `.github/workflows/ci.yml` | nothing; it takes effect on merge |
| tool versions for the native macOS/Windows jobs (`env:` in `ci.yml`) | updated by hand to match any changes made to the devcontainer image Dockerfile, ideally in the same PR | |

Rollback = revert the pin PR. Dependabot covers the third-party actions
used by the workflows here (github-actions ecosystem); it never touches
the digest, which is why the adoption PR is a deliberate, reviewed step.

## Common pitfalls (each learned the hard way, once)

- **Switching compiler profiles requires a clean build tree.** CMake caches
  the compiler and silently ignores a changed toolchain in an existing
  `build/`. `scripts/build.sh` detects the switch and cleans the cache automatically.
  When building by hand, run `rm -rf build` first to clear out the old profile.
- **A build tree remembers the absolute path it was created at.** Using one
  checkout from different mount points — plain `docker run` at `/work`, a
  devcontainer at `/workspaces/<name>` — fails with "CMakeCache.txt directory
  … is different". `scripts/build.sh` detects the move and cleans the system
  automatically. Again, if building by hand, `rm -rf build` to clear first.
- **Zed's dev containers are local-only (v1).** Zed cannot chain SSH remoting
  with dev containers — an SSH-remote project shows no "reopen in container"
  prompt. On remote machines, use the devcontainer CLI
  (`npx @devcontainers/cli up --workspace-folder .`) or VS Code, or open the
  repo on the machine where Docker runs.
- **One-shot `docker run` loses the Conan cache** (it lives in the container
  home, not the mounted repo) while `build/` persists on the mount — a
  mismatch that yields confusing "missing gtest" errors. Persist it with
  `-v ndof-conan-cache:/home/dev/.conan2`, or just use a devcontainer opened
  through your editor.
- **Linux hosts: the checkout must be writable by the container user.**
  Bind mounts preserve numeric uids, and the image's `dev` user is uid 1000.
  Devcontainer tools bridge the gap by remapping `dev`'s uid to yours at
  create time (`updateRemoteUserUID` — spec default, stated explicitly in
  our devcontainer.json); VS Code and the devcontainer CLI do this
  reliably, Zed currently has open bugs. Symptom: `PermissionError` on the
  first write, usually deep inside Conan; `scripts/build.sh` fails fast
  with the explanation. Diagnose with `id` (container user) vs `ls -ldn .`
  (checkout owner). Do not `chmod -R 777` around it. git would then see every
  tracked file as modified (exec-bit mode changes that must not be
  committed), and container-created files still end up owned by the wrong
  uid. Fix the remapping, then re-clone if the tree was already chmodded.
- **One checkout used from host and container makes git's status lie.**
  Git decides "possibly modified" from cached stat data (inode, device,
  uid, ctime) that differs between the host filesystem and the same
  files seen through the container mount, so running git on both sides
  of one checkout makes every tracked file show as modified with empty
  diffs, flickering as each side refreshes the index in turn. Fix, once
  per clone (the setting lives in `.git/config`, which both sides
  share): `git config core.checkStat minimal` and
  `git config core.trustctime false`, then one `git status` to settle;
  from then on both sides judge freshness by size and mtime only, which
  the mount reports identically.
- **Editor code intelligence is configured by `.clangd`, and needs one
  build first.** clangd finds each library's `compile_commands.json` via the
  repo's `.clangd` file (`libs/<name>/build/Debug` — clangd's default
  search never looks there). That
  file is honored by every editor; anything under `customizations.vscode`
  in devcontainer.json reaches VS Code *only* — Zed ignores it entirely.
  On a fresh clone there is no `compile_commands.json` until
  `scripts/build.sh <library> debug` completes, so the editor shows
  include errors until the first build finishes and the language server
  restarts.
- **A forgotten editable registration shadows the real package.** In
  `--editable` mode the root `scripts/build.sh` registers every library as
  a Conan editable, which persists in the container. The script's default
  mode removes those registrations before building, so running any build
  without `--editable` restores package resolution; `conan editable list`
  shows what is active.
- **Format only with the pinned clang-format** (i.e., inside the container).
  Different clang-format majors disagree; CI enforces the pinned one.
- **`ctest` passing while the build failed** usually means a stale test
  binary from a previous configuration — clean and rebuild.

## Glossary

| Term | Meaning |
|---|---|
| ABI | Application Binary Interface — the binary-compatibility contract between compiled artifacts |
| ASan / UBSan / TSan | Address / UndefinedBehavior / Thread Sanitizers — runtime error detectors compiled into test builds |
| CD | Continuous Delivery — green commits automatically produce the deliverable artifact |
| CI | Continuous Integration — every change automatically built and tested before it lands |
| clangd | LLVM's language server (code intelligence in Zed/VS Code); reads `compile_commands.json` from `libs/<name>/build/Debug` |
| CTest | CMake's test runner |
| Dev Container | A development environment defined by `devcontainer.json` + a container image; supported by VS Code, Zed, JetBrains, Codespaces |
| digest | Immutable `sha256:` content fingerprint of a container image — the reproducibility anchor |
| GHA | GitHub Actions — the CI/CD service executing `.github/workflows/*.yml` |
| GHCR | GitHub Container Registry (`ghcr.io`) — hosts the `ndof-dev` image |
| GCC / Clang / MSVC | The three compiler families in the CI matrix (GNU, LLVM, Microsoft) |
| Ninja | Fast build executor invoked by CMake |
| OCI | Open Container Initiative — standards for container images (incl. the `org.opencontainers.image.*` labels baked into ours) |
| package ID | Conan's hash of recipe + settings identifying one binary variant of a dependency |
| profile | Conan's description of a build configuration (the ABI axes + tool configuration) |
| remote | A Conan package server (ConanCenter for third-party packages; `ndof-public` for the ndof libraries; a private remote is planned for the private layer) |
| SPDX | Standardized machine-readable license identifiers (`SPDX-License-Identifier: Apache-2.0` headers) |
