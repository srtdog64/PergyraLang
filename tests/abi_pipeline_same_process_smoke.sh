#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix)"
ABI_BIN="${1:-${PGY_ABI_PIPELINE_TEST_BIN:-./bin/test_abi_pipeline.exe}}"
ABI_WIN_BIN=""
ABI_WIN_BIN_PS=""
PATH_PREFIX_PS=""
ABI_BIN_DIR=""
ABI_BIN_DIR_WIN=""
PS_OUT=""
PS_ERR=""
PS_RC=0

cd "$ROOT_DIR"

ABI_BIN="$(pgy_path_for_bash_tool "$ABI_BIN")"
if [[ ! -x "$ABI_BIN" && -x "./$ABI_BIN" ]]; then
    ABI_BIN="./$ABI_BIN"
fi
if [[ ! -x "$ABI_BIN" && "$ABI_BIN" != *.exe && -x "${ABI_BIN}.exe" ]]; then
    ABI_BIN="${ABI_BIN}.exe"
fi
if [[ ! -x "$ABI_BIN" ]]; then
    echo "[abi-pipeline-same-process] missing ABI pipeline test binary: $ABI_BIN" >&2
    exit 1
fi
if ! pgy_binary_is_runnable_here "$ABI_BIN"; then
    echo "[abi-pipeline-same-process] ABI pipeline test binary is not runnable on this host: $ABI_BIN" >&2
    if command -v file >/dev/null 2>&1; then
        file "$ABI_BIN" >&2 || true
    fi
    exit 1
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        ABI_BIN_DIR="$(cd "$(dirname "$ABI_BIN")" && pwd)"
        ABI_BIN_DIR_WIN="$(pgy_path_for_windows_tool "$ABI_BIN_DIR")"
        ABI_WIN_BIN="$(pgy_path_for_windows_tool "${ABI_BIN_DIR}/$(basename "$ABI_BIN")")"
        ABI_WIN_BIN_PS="$(pgy_powershell_quote "$ABI_WIN_BIN")"
        PATH_PREFIX_PS="$(pgy_powershell_quote "${ABI_BIN_DIR_WIN};${PGY_WINDOWS_PS_PATH_PREFIX}")"
        PS_OUT="$(mktemp "${TMPDIR:-/tmp}/pgy_abi_same_process.XXXXXX.out")"
        PS_ERR="$(mktemp "${TMPDIR:-/tmp}/pgy_abi_same_process.XXXXXX.err")"
        trap 'rm -f "$PS_OUT" "$PS_ERR"' EXIT
        set +e
        powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
            "\$env:PATH=${PATH_PREFIX_PS} + \$env:PATH; \$env:PGY_ABI_PIPELINE_SAME_PROCESS='1'; \$env:PGY_ABI_PIPELINE_BACKEND='llvm'; & ${ABI_WIN_BIN_PS}; exit \$LASTEXITCODE" \
            >"$PS_OUT" 2>"$PS_ERR"
        PS_RC=$?
        set -e
        cat "$PS_OUT"
        if [[ "$PS_RC" -ne 0 ]]; then
            echo "[abi-pipeline-same-process] PowerShell launch failed: $PS_RC" >&2
            cat "$PS_ERR" >&2
            exit "$PS_RC"
        fi
        ;;
    *)
        PGY_ABI_PIPELINE_SAME_PROCESS=1 \
        PGY_ABI_PIPELINE_BACKEND=llvm \
            "$ABI_BIN"
        ;;
esac
