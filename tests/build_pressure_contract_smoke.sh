#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBE="$ROOT_DIR/scripts/measure_build_pressure.ps1"
DRIVER_PARITY="$ROOT_DIR/tests/self_hosted/parity/driver_rung2_body_parity.sh"

require() {
    grep -Fq "$1" "$PROBE" || {
        echo "[build-pressure-contract] missing: $1" >&2
        exit 1
    }
}

require 'schema = "pgy.build-pressure.v2"'
require 'compile_proc_count'
require 'link_proc_count'
require '$isCompiler -and $commandLine'
require '$phaseStats'
require 'peak_working_set_mb'
require 'peak_private_mb'
require 'ConvertTo-Json -Depth 4'
require '$summaryPath'
require '$env:MAKEFLAGS = $null'
require '$env:MAKELEVEL = $null'
require 'New-Object System.Diagnostics.ProcessStartInfo'
require 'StartedAt $started'
require 'cc1|cc1plus|lto1|lto-wrapper|collect2|ld'
require 'pgy|pgy-self-driver|parser_ast_producer|gen[0-9]+'
require 'detached_compiler_worker_tracking = $true'
require '$exitCode = [int]$process.ExitCode'
require 'New-Object System.Text.UTF8Encoding($false)'
require '[switch]$StopOnLimit'
require 'limit_exceeded = $limitExceeded'
require 'if ($StopOnLimit)'

grep -Fq 'PGY_BUILD_PRESSURE_LIMIT_MB ?= 3072' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing 3 GiB build ceiling" >&2; exit 1; }
grep -Fq 'build-pressure-dev-compiler:' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing dev compiler probe" >&2; exit 1; }
grep -Fq 'build-pressure-compiler:' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing full compiler probe" >&2; exit 1; }
grep -Fq 'build-pressure-self-host-compiler:' "$ROOT_DIR/Makefile" \
    || { echo "[build-pressure-contract] missing self-host compiler probe" >&2; exit 1; }
if [[ "$(grep -c -- '-StopOnLimit' "$ROOT_DIR/Makefile")" -lt 3 ]]; then
    echo "[build-pressure-contract] compiler probes do not enforce the hard memory ceiling" >&2
    exit 1
fi
grep -Fq '*/Git/usr/bin/bash.exe)' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] full DRV-2 matrix lacks the Git Bash orphan guard" >&2; exit 1; }
grep -Fq 'full matrix requires MSYS2 bash' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] full DRV-2 matrix shell diagnostic drifted" >&2; exit 1; }
grep -Fq 'PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER for a focused Git Bash gate' "$DRIVER_PARITY" \
    || { echo "[build-pressure-contract] focused Git Bash escape hatch is undocumented" >&2; exit 1; }

echo "[build-pressure-contract] native/self-host hard ceiling and full-matrix shell guard wired"
