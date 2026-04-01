#!/usr/bin/env bash
set -euo pipefail
set -x

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build/local}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

if command -v qmake6 >/dev/null; then
    QMAKE_BIN="$(command -v qmake6)"
elif command -v qmake >/dev/null; then
    QMAKE_BIN="$(command -v qmake)"
else
    echo "qmake6/qmake not found in PATH." >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR"
"$QMAKE_BIN" "$REPO_ROOT/realagi-retro-term.pro"
make -j"$JOBS"
popd

echo "Built $BUILD_DIR/realagi-retro-term"
