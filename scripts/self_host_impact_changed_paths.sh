#!/usr/bin/env bash
# Changed-path entrypoint for self-host impact validation.
#
# This script owns changed-path discovery only. Impact classification still
# belongs to the Pergyra completeness impact planner consumed by
# self-host-preparation-impact-test-smoke.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PGY_SELFHOST_IMPACT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/impact_changed_paths}"
CHANGED_PATHS_FILE="$BUILD_DIR/changed_paths.txt"

find_make() {
    if [[ -n "${PGY_SELFHOST_IMPACT_MAKE:-}" ]]; then
        printf '%s' "$PGY_SELFHOST_IMPACT_MAKE"
    elif [[ -n "${MAKE:-}" ]]; then
        printf '%s' "$MAKE"
    elif command -v make >/dev/null 2>&1; then
        command -v make
    elif command -v mingw32-make >/dev/null 2>&1; then
        command -v mingw32-make
    else
        echo "[self-host-impact-changed-paths] missing make" >&2
        exit 1
    fi
}

normalize_changed_paths_file() {
    local input="$1"
    local output="$2"
    awk '{
        sub(/\r$/, "")
        if ($0 != "") {
            print $0
        }
    }' "$input" > "$output"
}

collect_git_changed_paths() {
    local raw_file="$1"
    : > "$raw_file"

    if ! git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        return 0
    fi

    if [[ -n "${PGY_SELFHOST_IMPACT_GIT_BASE:-}" ]]; then
        git -C "$ROOT_DIR" diff --name-only "${PGY_SELFHOST_IMPACT_GIT_BASE}...HEAD" > "$raw_file"
    elif [[ -n "${GITHUB_BASE_REF:-}" ]] \
        && git -C "$ROOT_DIR" rev-parse --verify "origin/${GITHUB_BASE_REF}" >/dev/null 2>&1; then
        git -C "$ROOT_DIR" diff --name-only "origin/${GITHUB_BASE_REF}...HEAD" > "$raw_file"
    elif git -C "$ROOT_DIR" rev-parse --verify HEAD^ >/dev/null 2>&1; then
        git -C "$ROOT_DIR" diff --name-only HEAD^ HEAD > "$raw_file"
    else
        git -C "$ROOT_DIR" diff --name-only HEAD > "$raw_file"
    fi
}

MAKE_BIN="$(find_make)"
mkdir -p "$BUILD_DIR"

if [[ -n "${PGY_SELFHOST_IMPACT_CHANGED_PATHS:-}" || -n "${PGY_SELFHOST_IMPACT_CHANGED_PATHS_FILE:-}" ]]; then
    exec "$MAKE_BIN" -C "$ROOT_DIR" self-host-preparation-impact-test-smoke
fi

RAW_CHANGED_PATHS_FILE="$BUILD_DIR/changed_paths.raw.txt"
collect_git_changed_paths "$RAW_CHANGED_PATHS_FILE"
normalize_changed_paths_file "$RAW_CHANGED_PATHS_FILE" "$CHANGED_PATHS_FILE"

if [[ ! -s "$CHANGED_PATHS_FILE" ]]; then
    echo "[self-host-impact-changed-paths] no changed paths; skipping impact validation"
    exit 0
fi

PGY_SELFHOST_IMPACT_CHANGED_PATHS_FILE="$CHANGED_PATHS_FILE" \
    exec "$MAKE_BIN" -C "$ROOT_DIR" self-host-preparation-impact-test-smoke
