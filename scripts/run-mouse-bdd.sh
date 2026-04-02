#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
ARTIFACTS_DIR="${ARTIFACTS_DIR:-$REPO_ROOT/build/mouse-bdd-artifacts}"
DISPLAY_VALUE="${DISPLAY_VALUE:-:99}"
PYTHON_TAG="$(python3 -c 'import platform, sys; print(f"py{sys.version_info.major}.{sys.version_info.minor}-{platform.system().lower()}-{platform.machine().lower()}")')"
DEFAULT_VENV_DIR="$REPO_ROOT/build/mouse-bdd-venv-$PYTHON_TAG"
VENV_DIR="${MOUSE_BDD_VENV_DIR:-$DEFAULT_VENV_DIR}"
REQUIREMENTS_FILE="$REPO_ROOT/tests/mouse_support/requirements.txt"
if command -v qmake6 >/dev/null 2>&1; then
    QMAKE_QUERY_BIN="$(command -v qmake6)"
elif command -v qmake >/dev/null 2>&1; then
    QMAKE_QUERY_BIN="$(command -v qmake)"
else
    QMAKE_QUERY_BIN=""
fi

if [[ -n "$QMAKE_QUERY_BIN" ]]; then
    QT_VERSION="$("$QMAKE_QUERY_BIN" -query QT_VERSION | tr '.' '-')"
else
    QT_VERSION="unknown"
fi

BUILD_TAG="qt${QT_VERSION}-$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m | tr '[:upper:]' '[:lower:]')"
BUILD_DIR="${MOUSE_BDD_BUILD_DIR:-$REPO_ROOT/build/mouse-bdd-build-$BUILD_TAG}"

rm -rf "$ARTIFACTS_DIR"
mkdir -p "$ARTIFACTS_DIR"
cleanup() {
    if [[ -n "${XVFB_PID:-}" ]]; then
        kill "$XVFB_PID" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

export DISPLAY="$DISPLAY_VALUE"
export TERM="${TERM:-xterm-256color}"
export PIP_DISABLE_PIP_VERSION_CHECK=1
export BUILD_DIR

if [[ ! -x "$VENV_DIR/bin/python" || ! -x "$VENV_DIR/bin/pip" ]]; then
    rm -rf "$VENV_DIR"
    python3 -m venv "$VENV_DIR"
fi

"$VENV_DIR/bin/python" -m pip install --no-cache-dir -r "$REQUIREMENTS_FILE" >/dev/null

Xvfb "$DISPLAY" -screen 0 1440x900x24 -ac >"$ARTIFACTS_DIR/xvfb.log" 2>&1 &
XVFB_PID=$!

for _ in $(seq 1 100); do
    if xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

if ! xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
    echo "Xvfb did not become ready on $DISPLAY" >&2
    exit 1
fi

"$REPO_ROOT/scripts/build-local.sh"

"$VENV_DIR/bin/behave" "$REPO_ROOT/tests/mouse_support/features" \
    -D repo_root="$REPO_ROOT" \
    -D artifacts_dir="$ARTIFACTS_DIR" \
    -D app_binary="$BUILD_DIR/realagi-retro-term"
