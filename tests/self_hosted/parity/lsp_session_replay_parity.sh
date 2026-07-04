#!/usr/bin/env bash
#
# LSP-2e parity: a buffered JSON-RPC request stream is replayed into the
# response frames that the self-host response owner can already prove. This is
# still not a live read-exact loop, document-store owner, or feature handler.

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
        echo "[self-host-parity:lsp-session-replay] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:lsp-session-replay] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:lsp-session-replay" "$PGY"

LSP_SOURCE="$ROOT_DIR/src/self_hosted/lsp/main.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lsp_session_replay}"
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
        echo "[self-host-parity:lsp-session-replay] backend=$backend compile failed" >&2
        cat "$compile_log" >&2
        exit 1
    fi
}

frame_for_body() {
    local body="$1"
    printf 'Content-Length: %d\r\n\r\n%s' "${#body}" "$body"
}

capture_session_output() {
    local backend="$1"
    local bin="$2"
    local label="$3"
    local input="$4"
    local expected="$5"
    local out_file="$BUILD_DIR/${label}_${backend}.json"
    local err_file="$BUILD_DIR/${label}_${backend}.err"
    local rc

    set +e
    printf '%b' "$input" | (cd "$ROOT_DIR" && "$bin" --session-replay-probe 2048 >"$out_file.raw" 2>"$err_file")
    rc=$?
    set -e
    tr -d '\r' < "$out_file.raw" > "$out_file"
    rm -f "$out_file.raw"

    if [[ "$rc" -ne 0 ]]; then
        echo "[self-host-parity:lsp-session-replay] backend=$backend label=$label failed rc=$rc" >&2
        cat "$out_file" "$err_file" >&2
        exit 1
    fi
    grep -Fq '"schema":"pgy.selfhost.lsp-session-replay.v1"' "$out_file" || {
        echo "[self-host-parity:lsp-session-replay] backend=$backend label=$label schema missing" >&2
        cat "$out_file" >&2
        exit 1
    }

    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "lsp-session-replay:$backend:$label" \
        "$BUILD_DIR" \
        "$expected" \
        "$out_file" \
        "lsp_session_replay"
}

BACKENDS="${PGY_SELFHOST_LSP_BACKENDS:-c llvm}"
RAN_BACKENDS=()
SKIPPED_BACKENDS=()

body_initialize='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
body_initialized='{"jsonrpc":"2.0","method":"initialized"}'
body_shutdown='{"jsonrpc":"2.0","id":2,"method":"shutdown"}'
body_hover='{"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{}}'
input_session="$(frame_for_body "$body_initialize")$(frame_for_body "$body_initialized")$(frame_for_body "$body_shutdown")"
input_unsupported="$(frame_for_body "$body_hover")"

for backend in $BACKENDS; do
    lsp_bin="$BUILD_DIR/main_${backend}.exe"
    set +e
    compile_lsp_backend "$backend" "$lsp_bin"
    compile_rc=$?
    set -e
    if [[ "$compile_rc" -eq 2 ]]; then
        echo "[self-host-parity:lsp-session-replay] LLVM backend unavailable; skipping llvm-built session replay"
        SKIPPED_BACKENDS+=("$backend")
        continue
    fi
    if [[ "$compile_rc" -ne 0 ]]; then
        exit "$compile_rc"
    fi

    capture_session_output "$backend" "$lsp_bin" \
        "session_replay" \
        "$input_session" \
        "$ROOT_DIR/src/self_hosted/lsp/expected/session_replay.json"
    capture_session_output "$backend" "$lsp_bin" \
        "session_replay_unsupported" \
        "$input_unsupported" \
        "$ROOT_DIR/src/self_hosted/lsp/expected/session_replay_unsupported.json"
    RAN_BACKENDS+=("$backend")
done

if [[ "${#RAN_BACKENDS[@]}" -eq 0 ]]; then
    echo "[self-host-parity:lsp-session-replay] no requested backend ran" >&2
    exit 1
fi

echo "[self-host-parity:lsp-session-replay] session replay parity ok (backends=${RAN_BACKENDS[*]}; skipped=${SKIPPED_BACKENDS[*]:-none})"
