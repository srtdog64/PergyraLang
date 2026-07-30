#!/usr/bin/env bash
# Insere-derived latest-only document publication through the real self-host
# LSP Main --document-store-probe route.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
LSP_SOURCE="$ROOT_DIR/src/self_hosted/lsp/main.pgy"
STORE_OWNER="$ROOT_DIR/src/self_hosted/lsp/document_store_owner.pgy"
REVISION_OWNER="$ROOT_DIR/src/self_hosted/lsp/document_revision_owner.pgy"
EXPECTED="$ROOT_DIR/src/self_hosted/lsp/expected/document_store_latest.json"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/lsp_document_latest}"

fail() { echo "[self-host-parity:lsp-document-latest] FAIL: $*" >&2; exit 1; }

[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
pgy_require_runnable_binary_here "self-host-parity:lsp-document-latest" "$PGY"
for path in "$LSP_SOURCE" "$STORE_OWNER" "$REVISION_OWNER" "$EXPECTED"; do
    [[ -f "$path" ]] || fail "missing input: $path"
done

grep -Fq 'import "../../../stdlib/host_task_slot.pgy";' "$STORE_OWNER" ||
    fail "production document-store graph does not import HostTaskSlot"
grep -Fq 'import "document_revision_owner.pgy";' "$STORE_OWNER" ||
    fail "production document-store graph bypasses typed revision owner"
grep -Fq 'LspDocumentRevisionChange(' "$STORE_OWNER" ||
    fail "didChange bypasses typed revision admission"
grep -Fq 'LspDocumentPublicationAdmissionFor(' "$STORE_OWNER" ||
    fail "publication bypasses current-ticket admission"
grep -Fq 'HostTasks_ApplyPolicy(' "$REVISION_OWNER" ||
    fail "revision change does not consume HostTask policy owner"
grep -Fq 'HostTasks_IsCurrent(' "$REVISION_OWNER" ||
    fail "publication does not consume current generation"
if grep -Eq 'let (uris|versions|texts): Array<String>|ArraySet\((versions|texts),' "$STORE_OWNER"; then
    fail "parallel string-array document mutation bypass reappeared"
fi

mkdir -p "$BUILD_DIR"

body_a10='{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///a.pgy","version":10,"text":"A10"}}}'
body_b3='{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///b.pgy","version":3,"text":"B3"}}}'
body_a12='{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///a.pgy","version":12},"contentChanges":[{"text":"A12"}]}}'
body_a11='{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///a.pgy","version":11},"contentChanges":[{"text":"A11"}]}}'
body_a12_conflict='{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///a.pgy","version":12},"contentChanges":[{"text":"A12-conflict"}]}}'

frame_for_body() {
    local body="$1"
    printf 'Content-Length: %d\r\n\r\n%s' "${#body}" "$body"
}

input_stream="$(frame_for_body "$body_a10")$(frame_for_body "$body_b3")$(frame_for_body "$body_a12")$(frame_for_body "$body_a11")$(frame_for_body "$body_a12_conflict")"

BACKENDS="${PGY_SELFHOST_LSP_BACKENDS:-c llvm}"
RAN_BACKENDS=()
SKIPPED_BACKENDS=()
first_actual=""

for backend in $BACKENDS; do
    out_bin="$BUILD_DIR/main_${backend}.exe"
    compile_log="$BUILD_DIR/main_${backend}.compile.log"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$LSP_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$out_bin")" \
        >"$compile_log" 2>&1)
    compile_rc=$?
    set -e
    if [[ "$compile_rc" -ne 0 ]]; then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$compile_log"; then
            echo "[self-host-parity:lsp-document-latest] LLVM unavailable; skipping"
            SKIPPED_BACKENDS+=("$backend")
            continue
        fi
        cat "$compile_log" >&2
        fail "backend=$backend compile failed"
    fi

    actual="$BUILD_DIR/latest_${backend}.json"
    error_log="$BUILD_DIR/latest_${backend}.err"
    printf '%s' "$input_stream" | (cd "$ROOT_DIR" && "$out_bin" --document-store-probe 8192) \
        >"$actual.raw" 2>"$error_log" || {
            cat "$actual.raw" "$error_log" >&2
            fail "backend=$backend execution failed"
        }
    tr -d '\r' <"$actual.raw" >"$actual"
    rm -f "$actual.raw"

    cmp -s "$EXPECTED" "$actual" || {
        diff -u "$EXPECTED" "$actual" >&2 || true
        fail "backend=$backend latest-publication artifact mismatch"
    }
    grep -Fq '"reason":"stale_version"' "$actual" ||
        fail "backend=$backend stale version was not rejected"
    grep -Fq '"reason":"version_payload_conflict"' "$actual" ||
        fail "backend=$backend same-version payload conflict was not rejected"
    grep -Fq '"version":"10","generation":1,"ok":false,"reason":"stale_generation"' "$actual" ||
        fail "backend=$backend stale diagnostics candidate was published"
    grep -Fq '"version":"12","generation":2,"ok":true,"reason":"applied"' "$actual" ||
        fail "backend=$backend current diagnostics candidate was not admitted"

    if [[ -z "$first_actual" ]]; then
        first_actual="$actual"
    elif ! cmp -s "$first_actual" "$actual"; then
        diff -u "$first_actual" "$actual" >&2 || true
        fail "backend=$backend output differs from first backend"
    fi
    RAN_BACKENDS+=("$backend")
    echo "[self-host-parity:lsp-document-latest] backend=$backend latest-only publication locked"
done

[[ "${#RAN_BACKENDS[@]}" -gt 0 ]] || fail "no requested backend ran"
echo "[self-host-parity:lsp-document-latest] parity ok (backends=${RAN_BACKENDS[*]}; skipped=${SKIPPED_BACKENDS[*]:-none})"
