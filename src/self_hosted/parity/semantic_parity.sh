#!/usr/bin/env bash
# Rung 2 parity for the Pergyra-origin semantic substitution slice.
# The Pergyra tool emits a bounded deterministic verdict while the C compiler
# remains the accept/reject oracle for the same fixtures.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:semantic] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:semantic] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/semantic/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/semantic}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
FIXTURE_DIR="$ROOT_DIR/src/self_hosted/semantic/fixture"
EXPECTED_DIR="$ROOT_DIR/src/self_hosted/semantic/expected"

SOURCE_PAIRS=(
    "valid_int_return:ok"
    "valid_string_return:ok"
    "valid_arith_int:ok"
    "valid_compare_bool:ok"
    "valid_call_int:ok"
    "bad_let_type:error"
    "bad_return_type:error"
    "bad_arith_assign:error"
    "bad_compare_return:error"
    "bad_call_assign:error"
    "bad_builtin_arg:error"
    "bad_user_arg:error"
    "valid_user_call:ok"
    "valid_escaped_quote:ok"
    "bad_arity_too_few:error"
    "bad_arity_too_many:error"
    "bad_arity_builtin:error"
    "bad_undefined_return:error"
    "bad_undefined_let:error"
    "bad_undefined_arg:error"
    "bad_undefined_assign:error"
    "bad_undefined_assign_lhs:error"
    "bad_undefined_compound_return:error"
    "bad_undefined_compound_arg:error"
    "valid_compound_local:ok"
    "valid_toint_int:ok"
    "bad_toint_assign:error"
    "valid_fileexists_bool:ok"
    "bad_fileexists_assign:error"
    "valid_tostring_string:ok"
    "bad_tostring_assign:error"
    "valid_string_builtins:ok"
    "bad_tolower_assign:error"
    "bad_undefined_call:error"
    "valid_logical_bool:ok"
    "bad_logical_int:error"
    "valid_literal_compare:ok"
    "bad_arith_operand:error"
    "bad_compare_operand:error"
    "bad_logical_return:error"
    "bad_compare_condition:error"
    "bad_binop_assign:error"
    "valid_compare_condition:ok"
    "bad_assign_type:error"
    "bad_condition_not_bool:error"
    "bad_logical_right:error"
    "bad_logical_condition:error"
    "bad_logical_assign:error"
    "bad_binop_return:error"
    "bad_compare_assign:error"
    "bad_binop_condition:error"
    "bad_not_operand:error"
    "valid_array_builtins:ok"
    "bad_block_scope_leak:error"
    "bad_else_scope_leak:error"
    "valid_outer_block_assign:ok"
    "bad_inner_let_type:error"
    "bad_condition_undefined:error"
)

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"
LIB_BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/lib"
mkdir -p "$LIB_BUILD_DIR"
cp "$ROOT_DIR/src/self_hosted/lib/"*.pgy "$LIB_BUILD_DIR/"

check_c_oracle() {
    local base="$1"
    local expected_class="$2"
    local source="$FIXTURE_DIR/${base}.pgy"
    local out="$PERGYRA_TOOL_BUILD_DIR/${base}.c-oracle.out"
    local err="$PERGYRA_TOOL_BUILD_DIR/${base}.c-oracle.err"
    local exe="$PERGYRA_TOOL_BUILD_DIR/${base}.c-oracle.exe"
    local rc

    set +e
    (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$exe")" >"$out" 2>"$err")
    rc=$?
    set -e

    if [[ "$expected_class" == "ok" && "$rc" -ne 0 ]]; then
        echo "[self-host-parity:semantic] C oracle rejected valid fixture: $base" >&2
        sed -n '1,40p' "$err" >&2
        exit 1
    fi
    if [[ "$expected_class" == "error" && "$rc" -eq 0 ]]; then
        echo "[self-host-parity:semantic] C oracle accepted invalid fixture: $base" >&2
        exit 1
    fi
}

compile_semantic_backend() {
    local backend="$1"
    local tool_bin="$2"

    echo "[self-host-parity:semantic] compiling semantic backend=$backend..."
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$PERGYRA_TOOL")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$tool_bin")" >/dev/null)
}

run_semantic_backend() {
    local backend="$1"
    local tool_bin="$2"

    for pair in "${SOURCE_PAIRS[@]}"; do
        local base="${pair%%:*}"
        local source="$FIXTURE_DIR/${base}.pgy"
        local expected_file="$EXPECTED_DIR/${base}.diag"
        local pergyra_out
        local rc

        if [[ ! -f "$source" ]]; then
            echo "[self-host-parity:semantic] missing source: $source" >&2
            exit 1
        fi
        if [[ ! -f "$expected_file" ]]; then
            echo "[self-host-parity:semantic] missing expected: $expected_file" >&2
            exit 1
        fi

        set +e
        pergyra_out="$(cd "$ROOT_DIR" && "$tool_bin" \
            "src/self_hosted/semantic/fixture/${base}.pgy" 2>/dev/null \
            | tr -d '\r')"
        rc=$?
        set -e

        if [[ "$rc" -ne 0 ]]; then
            echo "[self-host-parity:semantic] backend=$backend $base: exit-code FAIL ($rc)" >&2
            printf '%s\n' "$pergyra_out" >&2
            exit 1
        fi
        if [[ "$pergyra_out" == SEMANTIC\ * ]]; then
            echo "[self-host-parity:semantic] backend=$backend $base: raw semantic text leaked" >&2
            printf '%s\n' "$pergyra_out" >&2
            exit 1
        fi
        if [[ "$pergyra_out" == \{* ]]; then
            echo "[self-host-parity:semantic] backend=$backend $base: JSON semantic output leaked" >&2
            printf '%s\n' "$pergyra_out" >&2
            exit 1
        fi
        if ! grep -Fq 'Diagnostic: pgy.selfhost.semantic.v1' <<<"$pergyra_out"; then
            echo "[self-host-parity:semantic] backend=$backend $base: semantic diagnostic header missing" >&2
            printf '%s\n' "$pergyra_out" >&2
            exit 1
        fi
        if [[ "${pair##*:}" == "ok" ]]; then
            if ! grep -Fq 'Status: ok' <<<"$pergyra_out"; then
                echo "[self-host-parity:semantic] backend=$backend $base: ok status missing" >&2
                printf '%s\n' "$pergyra_out" >&2
                exit 1
            fi
        else
            if ! grep -Fq 'Status: error' <<<"$pergyra_out" || ! grep -Fq 'Code: ' <<<"$pergyra_out"; then
                echo "[self-host-parity:semantic] backend=$backend $base: error diagnostic shape missing" >&2
                printf '%s\n' "$pergyra_out" >&2
                exit 1
            fi
        fi

        local expected
        expected="$(tr -d '\r' < "$expected_file")"
        if [[ "$pergyra_out" != "$expected" ]]; then
            echo "[self-host-parity:semantic] backend=$backend $base: verdict drift" >&2
            diff <(printf '%s\n' "$expected") <(printf '%s\n' "$pergyra_out") | head -30 >&2
            exit 1
        fi
    done

    echo "[self-host-parity:semantic] backend=$backend verdicts ok (${#SOURCE_PAIRS[@]} fixtures)"
}

for pair in "${SOURCE_PAIRS[@]}"; do
    check_c_oracle "${pair%%:*}" "${pair##*:}"
done

BACKENDS="${PGY_SELFHOST_SEMANTIC_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    tool_bin="$PERGYRA_TOOL_BUILD_DIR/main_${backend}.exe"
    compile_semantic_backend "$backend" "$tool_bin"
    run_semantic_backend "$backend" "$tool_bin"
done

echo "[self-host-parity:semantic] rung-2 parity ok (${#SOURCE_PAIRS[@]} fixtures; backends=$BACKENDS)"
