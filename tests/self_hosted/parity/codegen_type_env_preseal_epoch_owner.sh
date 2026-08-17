#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
NATIVE_LLVM_PGY="${PGY_NATIVE_LLVM_BIN:-$ROOT_DIR/bin-dev-llvm/pgy.exe}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/codegen_type_env_preseal_epoch_owner.pgy"
TYPE_ENV="$ROOT_DIR/src/self_hosted/codegen/type_facts/type_env.pgy"
LOCAL_SCAN_OWNER="$ROOT_DIR/src/self_hosted/codegen/type_facts/type_env_local_row_scan_owner.pgy"
STATE_OWNER="$ROOT_DIR/src/self_hosted/codegen/type_facts/type_env_state_lifetime_owner.pgy"
STMT_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/stmt_emit.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/codegen_type_env_preseal_epoch}"
mkdir -p "$BUILD_DIR"

function_body() {
    local path="$1"
    local signature="$2"
    awk -v signature="$signature" '
        index($0, signature) { active = 1 }
        active && seen && /^[[:space:]]*(export[[:space:]]+)?func[[:space:]]/ { exit }
        active { print; seen = 1 }
    ' "$path"
}

owned_rows_body="$(function_body "$STATE_OWNER" \
    'func CodegenTypeEnvStateAppendOwnedLocalRows(')"
typed_rows_body="$(function_body "$STATE_OWNER" \
    'func CodegenTypeEnvStateAppendTypedValueBinding(')"
emit_let_body="$(function_body "$STMT_OWNER" 'func EmitLet(')"
append_local_body="$(function_body "$TYPE_ENV" 'func TypeEnvAppendLocalRows(')"
csv_at_body="$(function_body "$TYPE_ENV" 'func CsvAt(')"
mode_count_body="$(function_body "$TYPE_ENV" 'func ParamModeCsvCount(')"
lookup_rows_body="$(function_body "$LOCAL_SCAN_OWNER" \
    'func LookupKindTypeRows(')"
present_rows_body="$(function_body "$LOCAL_SCAN_OWNER" \
    'func LookupKindTypeRowPresentRows(')"

grep -Fq 'CodegenCharCodeAt(csv, n, i) == 44' <<<"$csv_at_body" || {
    echo "[self-host-parity:codegen-type-env-preseal] CSV lookup does not consume the allocation-free delimiter fact" >&2
    exit 1
}
grep -Fq 'CodegenCharCodeAt(modes, n, i) == 44' <<<"$mode_count_body" || {
    echo "[self-host-parity:codegen-type-env-preseal] parameter-mode count does not consume the allocation-free delimiter fact" >&2
    exit 1
}
for scan_body in "$csv_at_body" "$mode_count_body"; do
    if grep -Fq 'CodegenCharAt(' <<<"$scan_body"; then
        echo "[self-host-parity:codegen-type-env-preseal] CSV scan rebuilt delimiter Strings" >&2
        exit 1
    fi
done

grep -Fq 'let next_local_rows: String = Concat(rows, local_rows);' \
    <<<"$append_local_body" || {
    echo "[self-host-parity:codegen-type-env-preseal] local rows do not use one newest-first append" >&2
    exit 1
}
for retired_path in 'Substring(' 'let combined:' 'Concat("|",'; do
    if grep -Fq "$retired_path" <<<"$append_local_body"; then
        echo "[self-host-parity:codegen-type-env-preseal] local rows rebuilt the retained prefix: $retired_path" >&2
        exit 1
    fi
done
grep -Fq 'CodegenTypeLocalRowValueStart(rows, name, kind)' \
    <<<"$lookup_rows_body" || {
    echo "[self-host-parity:codegen-type-env-preseal] local value lookup bypasses the row-start owner" >&2
    exit 1
}
grep -Fq 'CodegenTypeLocalRowValueStart(rows, name, kind) >= 0' \
    <<<"$present_rows_body" || {
    echo "[self-host-parity:codegen-type-env-preseal] local presence lookup bypasses the row-start owner" >&2
    exit 1
}
grep -Fq 'let row_start: Bool = name_start == 0;' "$LOCAL_SCAN_OWNER" || {
    echo "[self-host-parity:codegen-type-env-preseal] first local row is not admitted" >&2
    exit 1
}

for term in 'TypeEnvAppendLocalRows(' \
    'CodegenTypeEnvStateReplaceOwnedLocal(' \
    'ArrayDropOwnedStrings(retired);'; do
    grep -Fq "$term" <<<"$owned_rows_body" || {
        echo "[self-host-parity:codegen-type-env-preseal] owned local-row lifetime is missing: $term" >&2
        exit 1
    }
done
append_line="$(grep -Fn 'TypeEnvAppendLocalRows(' <<<"$owned_rows_body" | head -1 | cut -d: -f1)"
replace_line="$(grep -Fn 'CodegenTypeEnvStateReplaceOwnedLocal(' <<<"$owned_rows_body" | head -1 | cut -d: -f1)"
retire_line="$(grep -Fn 'ArrayDropOwnedStrings(retired);' <<<"$owned_rows_body" | head -1 | cut -d: -f1)"
if (( append_line >= replace_line || replace_line >= retire_line )); then
    echo "[self-host-parity:codegen-type-env-preseal] owned local rows are not copy/install/retire ordered" >&2
    exit 1
fi
grep -Fq 'CodegenTypeEnvStateAppendOwnedLocalRows(state, env, rows);' \
    <<<"$typed_rows_body" || {
    echo "[self-host-parity:codegen-type-env-preseal] typed row adapter bypasses the lifetime owner" >&2
    exit 1
}
if grep -Fq 'TypeEnvAppendLocalRows(' <<<"$typed_rows_body"; then
    echo "[self-host-parity:codegen-type-env-preseal] typed row adapter rebuilt the append path" >&2
    exit 1
fi
owned_call_count="$(grep -Fc 'CodegenTypeEnvStateAppendOwnedLocalRows(' <<<"$emit_let_body")"
binding_row_count="$(grep -Fc 'binding.env_rows' <<<"$emit_let_body")"
if [[ "$owned_call_count" != "7" || "$binding_row_count" != "7" ]]; then
    echo "[self-host-parity:codegen-type-env-preseal] EmitLet must consume seven admitted binding rows exactly once" >&2
    exit 1
fi
if grep -Fq 'CodegenTypeEnvStateAppendTypedValueBinding(' <<<"$emit_let_body"; then
    echo "[self-host-parity:codegen-type-env-preseal] EmitLet rebuilt admitted binding rows" >&2
    exit 1
fi

run_backend() {
    local label="$1"
    local compiler="$2"
    local backend="$3"
    shift 3
    local bin="$BUILD_DIR/preseal_${label}.exe"
    local compile_log="$BUILD_DIR/preseal_${label}.compile.log"
    (cd "$ROOT_DIR" && "$compiler" "$@" \
        "$(pgy_path_for_compiler "$compiler" "$SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$compiler" "$bin")" \
        >"$compile_log" 2>&1) || {
        cat "$compile_log" >&2
        return 1
    }
    pgy_require_runnable_binary_here \
        "self-host-parity:codegen-type-env-preseal:$label" "$bin"
    local output
    output="$(cd "$ROOT_DIR" && "$bin" | tr -d '\r')"
    [[ "$output" == "codegen-type-env-preseal-epoch-ok" ]] || {
        echo "[self-host-parity:codegen-type-env-preseal] $label output drifted: $output" >&2
        return 1
    }
    if (cd "$ROOT_DIR" && "$bin" --malformed \
        >"$BUILD_DIR/preseal_${label}.malformed.out" \
        2>"$BUILD_DIR/preseal_${label}.malformed.err"); then
        echo "[self-host-parity:codegen-type-env-preseal] $label malformed delta was accepted" >&2
        return 1
    fi
    grep -Fq 'codegen preseal type-row delta is malformed' \
        "$BUILD_DIR/preseal_${label}.malformed.out" \
        "$BUILD_DIR/preseal_${label}.malformed.err" || {
        echo "[self-host-parity:codegen-type-env-preseal] $label malformed diagnostic drifted" >&2
        return 1
    }
}

run_backend installed_c "$PGY" c
run_backend native_llvm "$NATIVE_LLVM_PGY" llvm --native-pipeline

echo "[self-host-parity:codegen-type-env-preseal] ordered delta C/native LLVM parity ok"
