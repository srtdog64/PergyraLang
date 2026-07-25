#!/usr/bin/env bash
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v cmp >/dev/null 2>&1 \
    || ! command -v grep >/dev/null 2>&1 \
    || ! command -v mkdir >/dev/null 2>&1; then
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
[[ -x "$PGY" ]] || { echo "missing pgy: $PGY" >&2; exit 1; }

B="$ROOT_DIR/.tmp/self_hosted/mir_json_instruction_writer"
TOOL="$ROOT_DIR/src/self_hosted/tools/mir_json_instruction_writer_probe/main.pgy"
BACKENDS="${PGY_MIR_JSON_WRITER_BACKENDS:-c llvm}"
mkdir -p "$B"

fixtures=(
    "src/self_hosted/mir_lower/fixture/let_log.pgy"
    "src/self_hosted/codegen/fixture/ast_node_array_literal.pgy"
    "src/self_hosted/codegen/fixture/enum_multi_payload.pgy"
    "src/self_hosted/mir_lower/fixture/array_destructure.pgy"
    "src/self_hosted/codegen/fixture/option_string_core.pgy"
)

diagnose_raw_difference() {
    local expected="$1" actual="$2" label="$3"
    echo "[mir-json-writer-byte-parity] raw byte mismatch: $label" >&2
    wc -c "$expected" "$actual" >&2
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$expected" "$actual" >&2
    fi
    cmp -l "$expected" "$actual" 2>/dev/null | sed -n '1,8p' >&2 || true
}

built_backends=()
for backend in $BACKENDS; do
    bin="$B/writer_probe_${backend}.exe"
    log="$B/writer_probe_${backend}.compile.log"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$TOOL")" \
        "--backend=$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" \
        >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$log"; then
            echo "[mir-json-writer-byte-parity] LLVM backend unavailable; C raw-byte leg remains active"
            continue
        fi
        echo "[mir-json-writer-byte-parity] probe compile failed: $backend" >&2
        cat "$log" >&2
        exit 1
    fi
    built_backends+=("$backend")

    for fixture in "${fixtures[@]}"; do
        [[ -f "$ROOT_DIR/$fixture" ]] || {
            echo "[mir-json-writer-byte-parity] missing fixture: $fixture" >&2
            exit 1
        }
        base="$(basename "$fixture" .pgy)"
        string_json="$B/${base}_${backend}.string.json"
        stream_json="$B/${base}_${backend}.stream.json"
        invalid_json="$B/${base}_${backend}.invalid.json"
        run_log="$B/${base}_${backend}.run.log"
        if ! (cd "$ROOT_DIR" && "$bin" "$fixture" \
            "${string_json#"$ROOT_DIR"/}" \
            "${stream_json#"$ROOT_DIR"/}" \
            "${invalid_json#"$ROOT_DIR"/}" \
            >"$run_log" 2>&1); then
            echo "[mir-json-writer-byte-parity] probe run failed: $backend/$base" >&2
            cat "$run_log" >&2
            exit 1
        fi
        grep -Fq 'mir-json-instruction-writer-byte-probe-ok' "$run_log" || {
            echo "[mir-json-writer-byte-parity] probe completion marker missing: $backend/$base" >&2
            cat "$run_log" >&2
            exit 1
        }
        if ! cmp -s "$string_json" "$stream_json"; then
            diagnose_raw_difference "$string_json" "$stream_json" \
                "$backend/$base String-vs-stream"
            exit 1
        fi
        if [[ "$(<"$invalid_json")" != "writer-preopen-sentinel" ]]; then
            echo "[mir-json-writer-byte-parity] invalid facts changed artifact: $backend/$base" >&2
            exit 1
        fi
    done
done

[[ " ${built_backends[*]} " == *" c "* ]] || {
    echo "[mir-json-writer-byte-parity] C backend leg did not run" >&2
    exit 1
}

if [[ " ${built_backends[*]} " == *" llvm "* ]]; then
    for fixture in "${fixtures[@]}"; do
        base="$(basename "$fixture" .pgy)"
        c_json="$B/${base}_c.stream.json"
        llvm_json="$B/${base}_llvm.stream.json"
        if ! cmp -s "$c_json" "$llvm_json"; then
            diagnose_raw_difference "$c_json" "$llvm_json" \
                "$base C-vs-LLVM stream"
            exit 1
        fi
    done
fi

grep -Fq '"expr0_graph":{' "$B/ast_node_array_literal_c.stream.json"
grep -Fq '"match_patterns":[' "$B/enum_multi_payload_c.stream.json"
grep -Fq '"destructure_element_type":' "$B/array_destructure_c.stream.json"
grep -Fq '"abi_layout_required":true' "$B/option_string_core_c.stream.json"
grep -Fq '"abi_type_name":null,"abi_layout_id":0,"abi_layout_required":false,"abi_layout":null' \
    "$B/option_string_core_c.stream.json"

echo "[mir-json-writer-byte-parity] raw String/file bytes and invalid pre-open rejection ok (backends=${built_backends[*]} fixtures=${#fixtures[@]})"
