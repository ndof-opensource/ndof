# ndof

The ndof libraries: foundational C++23 components for the ndof framework
family, developed together in this repository and released individually
as Conan packages and plain CMake packages. Requires C++23 (GCC 13+,
Clang 17+, MSVC 19.36+). Licensed under the [Apache License 2.0](LICENSE).

| Library | Package | Description |
|---|---|---|
| [`libs/core`](libs/core) | `ndof-core` | Foundational utilities for the ndof framework family |
| [`libs/callable`](libs/callable) | `ndof-callable` | Callable traits, concepts, and utilities |
| [`libs/error`](libs/error) | `ndof-error` | Design-by-contract and error-handling framework |
| [`libs/patterns`](libs/patterns) | `ndof-patterns` | NDoF design patterns |

Each directory under `libs/` is a self-contained CMake project and Conan
recipe with its own version, tests, and README; libraries depend on each
other only through their published interfaces.

## Development

The canonical environment is the ndof dev container, defined and published
by [ndof-infra](https://github.com/ndof-opensource/ndof-infra) and pinned by
digest in `.devcontainer/devcontainer.json`. Prerequisites: Docker and a
devcontainer-capable editor (e.g. VS Code, Zed, JetBrains) or the
[devcontainer CLI](https://github.com/devcontainers/cli). Nothing else is installed on your host.

1. Clone this repository and open the folder in your editor.
2. Accept the "reopen in container" prompt. The first open downloads the
   pinned dev image (one-time).
3. In the container terminal, from the repository root, run one of:

```sh
scripts/build.sh core debug        # one library: resolve deps + configure + build + test
scripts/build.sh all debug         # every library, in dependency order
scripts/build.sh --editable error debug   # live cross-library work (see docs/releasing.md)
```

Build presets: `debug`, `release`, `asan`, `tsan`; switch compilers with
`--profile linux-clang19`. In the editor, Cmd/Ctrl+Shift+B builds the
library that owns the file you have open. The full guide, including how
the tooling fits together and the CI gates, is
[docs/onboarding.md](docs/onboarding.md).

## Consuming and releasing

Each library's README shows how to consume it as a Conan package or a
plain CMake package. Releases are per library, tag-driven, and CI-only:
[docs/releasing.md](docs/releasing.md).

## Design decisions

The reasoning behind the environment, the layout, the gates, and the
release model is recorded with alternatives in
[docs/decisions.md](docs/decisions.md). Read it before proposing to change
one of them.

## AI Usage and Attribution

LLM-based AI tools are used at various stages of the development of the
ndof libraries, including code generation, documentation, testing, and
infrastructure. In general, use of such tools need not be mentioned in commit
messages or otherwise attributed to the tools. The intent behind this
policy is not for humans to take credit for works of AI, but rather
to emphasize that a human is ultimately responsible for every change,
and expected to manually review every line of code or documentation.
Pull requests opened by AI tools without an associated human contributor will be
rejected. AI assistance may be mentioned if desired, but is not required.
Again, ultimately the human contributor will be held responsible for any
change.

## Package hosting

Conan packages for the ndof libraries are served from the public remote
`https://conan.cloudsmith.io/ndof-opensource/packages/` (anonymous read;
see [docs/releasing.md](docs/releasing.md) for the release process).

Package hosting is generously provided free of charge by
[Cloudsmith](https://cloudsmith.com) under its open-source hosting program.
