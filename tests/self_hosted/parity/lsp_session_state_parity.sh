#!/usr/bin/env bash
#
# LSP-2h parity: a buffered JSON-RPC request stream is projected into one
# session-state artifact that consumes both response replay and document-store
# state. This is still not a live read-exact loop or semantic feature content.

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
        echo "[self-host-parity:lsp-session-state] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:lsp-session-state] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "self-host-parity:lsp-session-state" "$PGY"

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lsp_session_state}"
HARNESS_PATHS_FILE="$BUILD_DIR/lsp_session_state_harness_paths.txt"
mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "self-host-parity:lsp-session-state" \
    "$BUILD_DIR" \
    "lsp-session-state-paths" \
    "$HARNESS_PATHS_FILE"

harness_paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    harness_paths+=("$line")
done <"$HARNESS_PATHS_FILE"
if [[ "${#harness_paths[@]}" -ne 4 ]]; then
    echo "[self-host-parity:lsp-session-state] TestHarness manifest expected 4 rows, got ${#harness_paths[@]}" >&2
    exit 1
fi

LSP_SOURCE="$ROOT_DIR/${harness_paths[0]}"
EXPECTED_SESSION_STATE="$ROOT_DIR/${harness_paths[1]}"
body_open="${harness_paths[2]}"
body_hover="${harness_paths[3]}"
for path in "$LSP_SOURCE" "$EXPECTED_SESSION_STATE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:lsp-session-state] missing TestHarness input: $path" >&2
        exit 1
    fi
done

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
        echo "[self-host-parity:lsp-session-state] backend=$backend compile failed" >&2
        cat "$compile_log" >&2
        exit 1
    fi
}

frame_for_body() {
    local body="$1"
    printf 'Content-Length: %d\r\n\r\n%s' "${#body}" "$body"
}

capture_session_state_output() {
    local backend="$1"
    local bin="$2"
    local label="$3"
    local input="$4"
    local expected="$5"
    local out_file="$BUILD_DIR/${label}_${backend}.json"
    local err_file="$BUILD_DIR/${label}_${backend}.err"
    local rc

    set +e
    printf '%b' "$input" | (cd "$ROOT_DIR" && "$bin" --session-state-probe 4096 >"$out_file.raw" 2>"$err_file")
    rc=$?
    set -e
    tr -d '\r' < "$out_file.raw" > "$out_file"
    rm -f "$out_file.raw"

    if [[ "$rc" -ne 0 ]]; then
        echo "[self-host-parity:lsp-session-state] backend=$backend label=$label failed rc=$rc" >&2
        cat "$out_file" "$err_file" >&2
        exit 1
    fi
    grep -Fq '"schema":"pgy.selfhost.lsp-session-state.v1"' "$out_file" || {
        echo "[self-host-parity:lsp-session-state] backend=$backend label=$label schema missing" >&2
        cat "$out_file" >&2
        exit 1
    }

    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "lsp-session-state:$backend:$label" \
        "$BUILD_DIR" \
        "$expected" \
        "$out_file" \
        "lsp_session_state"
}

BACKENDS="${PGY_SELFHOST_LSP_BACKENDS:-c llvm}"
RAN_BACKENDS=()
SKIPPED_BACKENDS=()

input_session_state="$(frame_for_body "$body_open")$(frame_for_body "$body_hover")"

for backend in $BACKENDS; do
    lsp_bin="$BUILD_DIR/main_${backend}.exe"
    set +e
    compile_lsp_backend "$backend" "$lsp_bin"
    compile_rc=$?
    set -e
    if [[ "$compile_rc" -eq 2 ]]; then
        echo "[self-host-parity:lsp-session-state] LLVM backend unavailable; skipping llvm-built session state"
        SKIPPED_BACKENDS+=("$backend")
        continue
    fi
    if [[ "$compile_rc" -ne 0 ]]; then
        exit "$compile_rc"
    fi

    capture_session_state_output "$backend" "$lsp_bin" \
        "session_state" \
        "$input_session_state" \
        "$EXPECTED_SESSION_STATE"
    RAN_BACKENDS+=("$backend")
done

if [[ "${#RAN_BACKENDS[@]}" -eq 0 ]]; then
    echo "[self-host-parity:lsp-session-state] no requested backend ran" >&2
    exit 1
fi

echo "[self-host-parity:lsp-session-state] session-state parity ok (backends=${RAN_BACKENDS[*]}; skipped=${SKIPPED_BACKENDS[*]:-none})"
