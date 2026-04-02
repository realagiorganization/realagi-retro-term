#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd -P)"
IMAGE_NAME="${IMAGE_NAME:-realagi-retro-term-mouse-bdd}"
ARTIFACTS_DIR="$REPO_ROOT/build/mouse-bdd-artifacts"
USER_ID="${USER_ID:-$(id -u)}"
GROUP_ID="${GROUP_ID:-$(id -g)}"

mkdir -p "$ARTIFACTS_DIR"

docker build \
    -t "$IMAGE_NAME" \
    -f "$REPO_ROOT/docker/mouse-bdd.Dockerfile" \
    "$REPO_ROOT"

docker run --rm \
    --user "$USER_ID:$GROUP_ID" \
    -e HOME=/tmp/realagi-home \
    -e ARTIFACTS_DIR=/workspace/build/mouse-bdd-artifacts \
    -e JOBS="${JOBS:-}" \
    -v "$REPO_ROOT:/workspace" \
    -w /workspace \
    "$IMAGE_NAME" \
    ./scripts/run-mouse-bdd.sh
