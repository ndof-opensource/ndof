#!/usr/bin/env bash
# Build one ndof library, or all of them, from the repository root.
#
# usage: scripts/build.sh [options] <library|all> [debug|release|asan|tsan]
#        scripts/build.sh [options] --from <path> [debug|release|asan|tsan]
#   <library>         a directory name under libs/
#   --from <path>     the library owning <path> (editors pass the open file)
#   --editable        live cross-library development (see below)
#   --remote          build against the published ndof packages (see below)
#   --override <lib>=<version>
#                     with --remote: use this published version of <lib>
#                     instead of the one the target's requires names
#                     (repeatable)
#   --profile <p>     Conan profile: a name under /opt/conan/profiles or a path
#   --clean           delete the target library's build/ first
#   --list            print the libraries in build order, one per line, and exit
#
# Libraries are discovered from libs/*/conanfile.py and built in dependency
# order, computed from the requires each conanfile declares (--list prints
# it; CI uses that). scripts/lib-build.sh, run in a library's directory,
# does the actual install, configure, build, and test with its guards; this
# script only decides where a library's ndof dependencies come from:
#
#   default     mirrors CI. Every ndof dependency is packaged into the local
#               Conan cache (conan create) before the target builds, so the
#               target resolves it exactly as a consumer would. Any ndof
#               editable registrations are removed first, so a stale one
#               cannot silently shadow the cache.
#   --editable  live cross-library work with debugging support. Every library
#               is registered as a Conan editable, so a dependent resolves
#               its ndof dependencies to the sibling trees under libs/ and
#               sees uncommitted changes after a rebuild (header-only changes
#               included: rebuild the dependent). Never used by CI: the
#               package gate (conan create) is what proves the packaged
#               form still works.
#   --remote    the opposite of both: nothing is packaged from libs/. Every
#               ndof package is purged from the local cache and the target's
#               ndof dependencies are downloaded from ndof-public and built,
#               exactly as an outside consumer gets them. The version is the
#               one the target's requires names, which must be published;
#               --override <lib>=<version> substitutes any published version
#               (via a Conan profile with [replace_requires]), so a dependent
#               can be tried against an older release without editing a
#               conanfile.
set -euo pipefail

# Repository root, derived from this script's location so the script works
# from any current directory.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# direct_deps <library>: the ndof libraries named by self.requires(...) in
# the library's conanfile.py, one per line. The conanfile is the single
# source of truth for dependencies; nothing is declared twice here.
direct_deps() {
    grep -oE 'self\.requires\("ndof-[a-z]+/' "$ROOT/libs/$1/conanfile.py" 2>/dev/null \
        | sed -E 's/.*ndof-([a-z]+)\//\1/' || true
}

# order_libs: every directory under libs/ that holds a conanfile.py, printed
# in build order: each library after all of its ndof dependencies, ties
# broken alphabetically so the order is stable. Walks the requires graph
# depth first; a cycle, or a requires naming a library that is not under
# libs/, is an error.
order_libs() {
    local -A state=()
    local -a order=()
    local l
    visit() {
        local lib=$1 d
        case ${state[$lib]:-} in
            done) return 0 ;;
            active) echo "error: dependency cycle through libs/$lib" >&2; return 1 ;;
        esac
        state[$lib]=active
        for d in $(direct_deps "$lib"); do
            if [[ ! -f "$ROOT/libs/$d/conanfile.py" ]]; then
                echo "error: libs/$lib requires ndof-$d, but there is no libs/$d/conanfile.py" >&2
                return 1
            fi
            visit "$d" || return 1
        done
        state[$lib]=done
        order+=("$lib")
    }
    for l in $(cd "$ROOT/libs" && for d in */; do [[ -f $d/conanfile.py ]] && echo "${d%/}"; done | sort); do
        visit "$l" || return 1
    done
    printf '%s\n' "${order[@]}"
}

# LIBS is the computed build order. Everything below treats it as the
# authoritative list of libraries.
if ! ordered=$(order_libs); then
    exit 1
fi
LIBS=($ordered)

# Option parsing. Flags are recorded; anything else is a positional
# argument (library name, then preset).
EDITABLE=0
REMOTE=0
OVERRIDES=()
PROFILE_OPT=""
CLEAN=0
FROM=""
LIST=0
POSITIONAL=()
while [[ $# -gt 0 ]]; do
    case $1 in
        --list) LIST=1; shift ;;
        --editable) EDITABLE=1; shift ;;
        --remote) REMOTE=1; shift ;;
        --override) OVERRIDES+=("$2"); shift 2 ;;
        --profile) PROFILE_OPT=$2; shift 2 ;;
        --clean) CLEAN=1; shift ;;
        --from) FROM=$2; shift 2 ;;
        -h|--help) sed -n '2,44p' "$0"; exit 0 ;;
        -*) echo "error: unknown option $1" >&2; exit 2 ;;
        *) POSITIONAL+=("$1"); shift ;;
    esac
done

# --list: print the build order and stop, before any Conan or profile work.
if [[ $LIST == 1 ]]; then
    printf '%s\n' "${LIBS[@]}"
    exit 0
fi

# The three modes are exclusive, and overrides only mean something when the
# dependencies come from the remote.
if [[ $EDITABLE == 1 && $REMOTE == 1 ]]; then
    echo "error: --editable and --remote are exclusive" >&2
    exit 2
fi
if [[ ${#OVERRIDES[@]} -gt 0 && $REMOTE == 0 ]]; then
    echo "error: --override requires --remote" >&2
    exit 2
fi
for o in "${OVERRIDES[@]:-}"; do
    [[ -z $o ]] && continue
    if [[ $o != *=* ]]; then
        echo "error: --override expects <lib>=<version>, got '$o'" >&2
        exit 2
    fi
done

# --from <path>: This option allows you to pass a path to any file in the repo,
# and this section will work out which library owns the path. This
# functionality enables our default build task in IDEs, which allows the editor
# to pass the path of the file currently open, so we build the corresponding
# library. The path may be absolute or relative to the
# current directory; it must lie under libs/<library>/.
if [[ -n $FROM ]]; then
    abs=$FROM
    [[ $abs != /* ]] && abs="$PWD/$abs"
    rel=${abs#"$ROOT"/}
    if [[ $rel == libs/*/* ]]; then
        TARGET=${rel#libs/}
        TARGET=${TARGET%%/*}
    else
        echo "error: $FROM is not inside libs/<library>/ (open a file in a library first)" >&2
        exit 2
    fi
    PRESET=${POSITIONAL[0]:-debug}
else
    TARGET=${POSITIONAL[0]:-}
    PRESET=${POSITIONAL[1]:-debug}
fi

# Validate the target library and the preset before doing any work.
if [[ -z $TARGET ]]; then
    sed -n '2,10p' "$0" >&2
    exit 2
fi
if [[ $TARGET != all ]]; then
    known=0
    for l in "${LIBS[@]}"; do [[ $l == "$TARGET" ]] && known=1; done
    if [[ $known == 0 ]]; then
        echo "error: unknown library '$TARGET' (known: ${LIBS[*]})" >&2
        exit 2
    fi
fi
# The sanitizer presets build on top of the Debug Conan install, so they
# package dependencies as Debug too.
case $PRESET in
    debug|asan|tsan) BUILD_TYPE=Debug ;;
    release) BUILD_TYPE=Release ;;
    *) echo "error: unknown preset '$PRESET' (debug|release|asan|tsan)" >&2; exit 2 ;;
esac

# This section ensures ndof dependencies are built with the same Conan profile
# that the dependent library is about to be built with. Conan requires that
# a dependency be built with the same settings in order to be included for
# the dependent package. The choice mirrors
# scripts/lib-build.sh exactly (explicit --profile, else the image's gcc14
# profile, else Conan's detected default with C++23).
PROFILE_ARGS=()
if [[ -n $PROFILE_OPT ]]; then
    profile=$PROFILE_OPT
    [[ -f /opt/conan/profiles/$profile ]] && profile=/opt/conan/profiles/$profile
    PROFILE_ARGS=(-pr:a "$profile")
elif [[ -d /opt/conan/profiles ]]; then
    PROFILE_ARGS=(-pr:a /opt/conan/profiles/linux-gcc14)
else
    if [[ ! -f "$(conan config home)/profiles/default" ]]; then
        conan profile detect
    fi
    PROFILE_ARGS=(-s compiler.cppstd=23)
fi

# Register the public ndof remote once per machine. Normal builds never use
# it (every ndof dependency is packaged from libs/, and CI enforces that a
# requires names the working version), but having it configured lets you
# compare against a published version by hand when you want to.
if ! conan remote list | grep -q "^ndof-public:"; then
    conan remote add ndof-public https://conan.cloudsmith.io/ndof-opensource/packages/
fi

# deps_of <library>: every ndof library the given one depends on, directly
# or through another dependency, printed in LIBS order so they can be built
# in sequence. Uses a stack so a dependency shared by two paths appears once.
deps_of() {
    local -a wanted=()
    local -a stack=("$1")
    local l d
    while [[ ${#stack[@]} -gt 0 ]]; do
        l=${stack[-1]}
        unset 'stack[-1]'
        for d in $(direct_deps "$l"); do
            if [[ " ${wanted[*]:-} " != *" $d "* ]]; then
                wanted+=("$d")
                stack+=("$d")
            fi
        done
    done
    for l in "${LIBS[@]}"; do
        [[ " ${wanted[*]:-} " == *" $l "* ]] && echo "$l"
    done
    return 0
}

# lib_build <library>: run scripts/lib-build.sh (conan install, configure,
# build, test) in the library's directory, passing the preset and options
# through. --clean applies only to the library that was asked for, not to
# its dependencies.
lib_build() {
    local lib=$1
    local -a args=("$PRESET")
    [[ -n $PROFILE_OPT ]] && args+=(--profile "$PROFILE_OPT")
    [[ $CLEAN == 1 && $lib == "$TARGET" ]] && args+=(--clean)
    echo "==> libs/$lib: lib-build.sh ${args[*]}"
    (cd "$ROOT/libs/$lib" && "$ROOT/scripts/lib-build.sh" "${args[@]}")
}

# ensure_created <library>: package the library into the local Conan cache
# with conan create, so a dependent's conan install finds it there instead
# of asking the remote. Tests are skipped because lib_build runs them when
# the library itself is built. CREATED remembers what this run has already
# packaged, so a dependency shared by several targets is created once.
CREATED=()
ensure_created() {
    local lib=$1
    [[ " ${CREATED[*]:-} " == *" $lib "* ]] && return 0
    echo "==> libs/$lib: conan create into the local cache (tests skipped; its own build runs them)"
    conan create "$ROOT/libs/$lib" --build=missing "${PROFILE_ARGS[@]}" \
        -s build_type="$BUILD_TYPE" -c tools.build:skip_test=True
    CREATED+=("$lib")
}

# The list of libraries to build: all of them in LIBS order, or just the one
# requested.
if [[ $TARGET == all ]]; then
    TARGETS=("${LIBS[@]}")
else
    TARGETS=("$TARGET")
fi

# Editable mode: register every library so Conan resolves any ndof
# dependency to its directory under libs/ rather than to the Conan cache. Each
# dependency is then built in place first (so its build tree exists for the
# dependent to link against), then the target. When building all, a library
# already in the target list is not built twice.
if [[ $EDITABLE == 1 ]]; then
    for l in "${LIBS[@]}"; do
        conan editable add "$ROOT/libs/$l" >/dev/null
    done
    echo "editable mode: ndof dependencies resolve to the trees under libs/"
    for t in "${TARGETS[@]}"; do
        for d in $(deps_of "$t"); do
            [[ " ${TARGETS[*]} " == *" $d "* && $TARGET == all ]] && continue
            lib_build "$d"
        done
        lib_build "$t"
    done
# Remote mode: nothing from libs/ may satisfy a dependency. Drop editable
# registrations and purge every ndof package from the local cache, so the
# target's conan install has to download its ndof dependencies from
# ndof-public, at the versions its requires names. Overrides are applied
# through a temporary Conan profile with a [replace_requires] section,
# which swaps the required reference for the requested published version
# across the whole dependency graph without touching any conanfile.
elif [[ $REMOTE == 1 ]]; then
    conan editable remove --refs "ndof-*" >/dev/null 2>&1 || true
    conan remove "ndof-*" -c >/dev/null 2>&1 || true
    export NDOF_CONAN_INSTALL_ARGS=""
    if [[ ${#OVERRIDES[@]} -gt 0 ]]; then
        override_profile=$(mktemp)
        {
            echo "[replace_requires]"
            for o in "${OVERRIDES[@]}"; do
                echo "ndof-${o%%=*}/*: ndof-${o%%=*}/${o#*=}"
            done
        } > "$override_profile"
        export NDOF_CONAN_INSTALL_ARGS="-pr:a $override_profile"
        echo "remote mode: ndof dependencies come from ndof-public; overrides: ${OVERRIDES[*]}"
    else
        echo "remote mode: ndof dependencies come from ndof-public at the versions the requires name"
    fi
    for t in "${TARGETS[@]}"; do
        lib_build "$t"
    done
# Default mode: drop any ndof editable registrations left by an earlier
# --editable run (a stale one would silently shadow the cache), package each
# dependency into the cache, then build the target against them. This is
# the same sequence CI runs.
else
    conan editable remove --refs "ndof-*" >/dev/null 2>&1 || true
    for t in "${TARGETS[@]}"; do
        for d in $(deps_of "$t"); do
            ensure_created "$d"
        done
        lib_build "$t"
    done
fi
