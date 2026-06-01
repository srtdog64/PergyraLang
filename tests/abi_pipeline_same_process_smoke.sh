#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix)"
ABI_BIN="${1:-${PGY_ABI_PIPELINE_TEST_BIN:-./bin/test_abi_pipeline.exe}}"

cd "$ROOT_DIR"

if [[ ! -x "$ABI_BIN" && -x "./$ABI_BIN" ]]; then
    ABI_BIN="./$ABI_BIN"
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        ABI_WIN_BIN="$(pgy_path_for_windows_tool "$ABI_BIN")"
        PS_SCRIPT="$(mktemp "${TMPDIR:-/tmp}/pgy_abi_same_process.XXXXXX.ps1")"
        trap 'rm -f "$PS_SCRIPT"' EXIT
        {
            printf "%s\n" "\$env:PATH = '${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH"
            printf "%s\n" "\$env:PGY_ABI_PIPELINE_SAME_PROCESS = '1'"
            printf "%s\n" "\$env:PGY_ABI_PIPELINE_BACKEND = 'llvm'"
            printf "& '%s'\n" "$ABI_WIN_BIN"
            printf "%s\n" "exit \$LASTEXITCODE"
        } > "$PS_SCRIPT"
        powershell.exe -NoProfile -ExecutionPolicy Bypass \
            -File "$(pgy_path_for_windows_tool "$PS_SCRIPT")"
        ;;
    *)
        PGY_ABI_PIPELINE_SAME_PROCESS=1 \
        PGY_ABI_PIPELINE_BACKEND=llvm \
            "$ABI_BIN"
        ;;
esac
