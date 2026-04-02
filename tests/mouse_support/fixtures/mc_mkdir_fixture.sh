#!/usr/bin/env bash
set -euo pipefail

ARTIFACTS_DIR="${1:?usage: $0 ARTIFACTS_DIR}"
FIXTURE_ROOT="$ARTIFACTS_DIR/mc-root"

mkdir -p "$ARTIFACTS_DIR"
rm -rf "$FIXTURE_ROOT"
mkdir -p "$FIXTURE_ROOT"

export TERM="${TERM:-xterm-256color}"

capture_terminal_size() {
    local size
    for _ in $(seq 1 100); do
        size="$(stty size)"
        if [[ "$size" != "0 0" ]]; then
            printf '%s\n' "$size" > "$ARTIFACTS_DIR/terminal-size.txt"
            return 0
        fi
        sleep 0.1
    done
    printf '%s\n' "$size" > "$ARTIFACTS_DIR/terminal-size.txt"
    return 1
}

capture_terminal_size
printf '%s\n' "$FIXTURE_ROOT" > "$ARTIFACTS_DIR/mc-root-path.txt"

exec mc -P "$ARTIFACTS_DIR/mc-last-dir.txt" "$FIXTURE_ROOT"
