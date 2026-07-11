#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBE="$ROOT_DIR/scripts/measure_build_pressure.ps1"

require() {
    grep -Fq "$1" "$PROBE" || {
        echo "[build-pressure-contract] missing: $1" >&2
        exit 1
    }
}

require 'schema = "pgy.build-pressure.v2"'
require 'compile_proc_count'
require 'link_proc_count'
require '$phaseStats'
require 'peak_working_set_mb'
require 'peak_private_mb'
require 'ConvertTo-Json -Depth 4'
require '$summaryPath'

grep -Fq 'PGY_BUILD_PRESSURE_LIMIT_MB ?= 3072' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing 3 GiB build ceiling" >&2; exit 1; }
grep -Fq 'build-pressure-dev-compiler:' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing dev compiler probe" >&2; exit 1; }
grep -Fq 'build-pressure-compiler:' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing full compiler probe" >&2; exit 1; }

echo "[build-pressure-contract] phase samples, JSON summary, and 3 GiB ceiling wired"
