#!/usr/bin/env bash
# Wrapper so this library can be built from its own directory. The build
# logic is shared by every library and lives in scripts/lib-build.sh at the
# repository root.
cd "$(dirname "$0")/.." && exec ../../scripts/lib-build.sh "$@"
