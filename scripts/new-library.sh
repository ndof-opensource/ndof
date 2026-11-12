#!/usr/bin/env bash
# Create a new library under libs/ from scripts/library-skeleton.
#
# usage: scripts/new-library.sh <name> "<one-line description>"
#   e.g. scripts/new-library.sh units "Compile-time unit checking"
#
# <name> is lowercase letters only. It becomes the directory libs/<name>,
# the Conan package ndof-<name>, the CMake target ndof::<name>, the C++
# namespace ndof::<name>, and the include path <ndof/<name>/...>.
#
# The script copies the skeleton, substitutes the name and description, and
# adds the library's fragment to .clangd. The library starts at version
# 0.1.0 with a version header, one source file, and one test, and passes
# every CI gate as stamped. Nothing else needs registering: the build
# order, CI, and publishing discover libraries from libs/*/conanfile.py.
# One manual step remains, printed at the end.
#
# The skeleton's placeholders are __NAME__, __NAME_UPPER__, and
# __DESCRIPTION__. CI's new-library job stamps a scratch library on every
# change and runs the gates on it, so the skeleton cannot drift from what
# the gates require.
set -euo pipefail

if [[ $# -ne 2 ]]; then
    sed -n '2,9p' "$0" >&2
    exit 2
fi

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export NL_NAME=$1
export NL_DESC=$2
NL_UPPER=$(printf '%s' "$NL_NAME" | tr 'a-z' 'A-Z')
export NL_UPPER
DEST="$ROOT/libs/$NL_NAME"

if [[ ! $NL_NAME =~ ^[a-z]+$ ]]; then
    echo "error: the name must be lowercase letters only, got '$NL_NAME'" >&2
    exit 2
fi
if [[ -e $DEST ]]; then
    echo "error: libs/$NL_NAME already exists" >&2
    exit 2
fi

cp -R "$ROOT/scripts/library-skeleton" "$DEST"
mv "$DEST/include/ndof/__NAME__" "$DEST/include/ndof/$NL_NAME"
find "$DEST" -type f -print0 | xargs -0 perl -pi -e '
    s/__NAME_UPPER__/$ENV{NL_UPPER}/g;
    s/__NAME__/$ENV{NL_NAME}/g;
    s/__DESCRIPTION__/$ENV{NL_DESC}/g;
'

# .clangd holds one fragment per library, followed by a last fragment that
# applies to every file. Insert the new library's fragment before that last
# one, which is the text after the final "---" line.
perl -0pi -e '
    my $n = $ENV{NL_NAME};
    my $frag = "If:\n  PathMatch: libs/$n/.*\nCompileFlags:\n  CompilationDatabase: libs/$n/build/Debug\n---\n";
    s/(.*\n---\n)/$1$frag/s or die "no --- separator found in .clangd\n";
' "$ROOT/.clangd"

echo "created libs/$NL_NAME and added its fragment to .clangd"
echo "still to do by hand: add a row for it to the library table in README.md"
echo "then: scripts/build.sh $NL_NAME debug"
