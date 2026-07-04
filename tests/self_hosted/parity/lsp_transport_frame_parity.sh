#!/usr/bin/env bash
#
# LSP-2a parity: the self-host LSP transport owner consumes the byte-count
# ReadStdin substrate and parses one JSON-RPC Content-Length frame. This is not
# the full LSP session loop; LSP-2 remains planned until the loop/dispatch
# owner lands.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:lsp-transport-frame] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:lsp-transport-frame] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:lsp-transport-frame" "$PGY"

LSP_SOURCE="$ROOT_DIR/src/self_hosted/lsp/main.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lsp_transport_frame}"
mkdir -p "$BUILD_DIR"

compile_lsp_backend() {
    local backend="$1"
    local out_bin="$2"
    local compile_log="$BUILD_DIR/main_${backend}.compile.log"

    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$LSP_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$out_bin")" \
        >"$compile_log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$compile_log"; then
            return 2
        fi
        echo "[self-host-parity:lsp-transport-frame] backend=$backend compile failed" >&2
        cat "$compile_log" >&2
        exit 1
    fi
}

capture_frame_output() {
    local backend="$1"
    local bin="$2"
    local label="$3"
    local input="$4"
    local expected="$5"
    local out_file="$BUILD_DIR/${label}_${backend}.json"
    local err_file="$BUILD_DIR/${label}_${backend}.err"
    local rc

    set +e
    printf '%b' "$input" | (cd "$ROOT_DIR" && "$bin" --transport-frame-probe 64 >"$out_file.raw" 2>"$err_file")
    rc=$?
    set -e
    tr -d '\r' < "$out_file.raw" > "$out_file"
    rm -f "$out_file.raw"

    if [[ "$rc" -ne 0 ]]; then
        echo "[self-host-parity:lsp-transport-frame] backend=$backend label=$label failed rc=$rc" >&2
        cat "$out_file" "$err_file" >&2
        exit 1
    fi
    grep -Fq '"schema":"pgy.selfhost.lsp-transport-frame.v1"' "$out_file" || {
        echo "[self-host-parity:lsp-transport-frame] backend=$backend label=$label schema missing" >&2
        cat "$out_file" >&2
        exit 1
    }

    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "lsp-transport-frame:$backend:$label" \
        "$BUILD_DIR" \
        "$expected" \
        "$out_file" \
        "lsp_transport_frame"
}

BACKENDS="${PGY_SELFHOST_LSP_BACKENDS:-c llvm}"
RAN_BACKENDS=()
SKIPPED_BACKENDS=()

for backend in $BACKENDS; do
    lsp_bin="$BUILD_DIR/main_${backend}.exe"
    set +e
    compile_lsp_backend "$backend" "$lsp_bin"
    compile_rc=$?
    set -e
    if [[ "$compile_rc" -eq 2 ]]; then
        echo "[self-host-parity:lsp-transport-frame] LLVM backend unavailable; skipping llvm-built transport probe"
        SKIPPED_BACKENDS+=("$backend")
        continue
    fi
    if [[ "$compile_rc" -ne 0 ]]; then
        exit "$compile_rc"
    fi

    capture_frame_output "$backend" "$lsp_bin" \
        "transport_frame" \
        'Content-Length: 2\r\n\r\n{}' \
        "$ROOT_DIR/src/self_hosted/lsp/expected/transport_frame.json"
    capture_frame_output "$backend" "$lsp_bin" \
        "transport_frame_incomplete" \
        'Content-Length: 5\r\n\r\n{}' \
        "$ROOT_DIR/src/self_hosted/lsp/expected/transport_frame_incomplete.json"
    RAN_BACKENDS+=("$backend")
done

if [[ "${#RAN_BACKENDS[@]}" -eq 0 ]]; then
    echo "[self-host-parity:lsp-transport-frame] no requested backend ran" >&2
    exit 1
fi

echo "[self-host-parity:lsp-transport-frame] single-frame transport parity ok (backends=${RAN_BACKENDS[*]}; skipped=${SKIPPED_BACKENDS[*]:-none})"
