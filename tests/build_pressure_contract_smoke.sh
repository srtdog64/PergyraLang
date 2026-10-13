#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${OS:-}" == "Windows_NT" ]]; then
    source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
    pressure_powershell="$(command -v powershell.exe || command -v powershell)" || {
        echo "[build-pressure-contract] Windows execution receipt requires PowerShell" >&2
        exit 1
    }
    pressure_selftest="$(pgy_path_for_windows_tool "$ROOT_DIR/tests/build_pressure_execution_receipt_selftest.ps1")"
    # Codex/PowerShell 7 may export its own module path. Windows PowerShell 5
    # cannot load those modules; use its built-ins in this child process only.
    env -u PSModulePath -u PSModuleAnalysisCachePath \
        "$pressure_powershell" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$pressure_selftest"
else
    echo "[build-pressure-contract] Windows execution receipt self-test NOT RUN on this host"
fi
