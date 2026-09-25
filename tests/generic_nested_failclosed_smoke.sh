#!/usr/bin/env bash
#
# generic_nested_failclosed_smoke.sh — generic functions over constructed
# types on the native backends. A call either runs with the same output on
# C and LLVM or is refused with a diagnostic; it never reaches a native
# compiler as broken source unless a row below names that open fault.
#
# 2026-09-25 (registry row mir.generic_specialization): the checker binds a
# generic call's type arguments structurally (explicit type argument, the
# matching component of the first argument whose parameter type mentions
# the parameter, then the default) and seals the binding on the call. MIR
# specializes from that sealed binding only; its text matcher is gone, and a
# call with no sealed binding is refused in MIR with PGY_MIR_TOPOLOGY_INVALID.
#
#   - PARAM position (Option<T>, Array<T>, Result<T, E>) runs on both
#     backends; nested_param no longer fails closed on LLVM.
#   - A generic struct parameter (Crate<T>) binds T = Int and runs on both
#     backends. Native C used to declare the Open_Int prototype before the
#     Crate_Int typedef (the layout went to the late helper stream); the
#     layout now goes into the declaration stream of the prototype or type
#     that first names it.
#   - Conflicting bindings (nested or explicit) and a parameter no argument
#     fixes are refused at native semantic with a coded diagnostic; they used
#     to pass semantic and fail in MIR lowering, the C compiler or the LLVM
#     verifier.
#   - A shadowed local no longer borrows the outer local's type: the text
#     binder bound T = Int for a String and both backends failed.
#   - Class-generic constructed-over-T FIELD: C substitutes and runs; LLVM
#     still fails closed on aggregate lowering (G-5 owns it).

set -euo pipefail

# Subject of this gate:
#   the native generic call binding and MIR specialization.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[generic-nested] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/generic_nested_failclosed"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[generic-nested] FAIL: $*" >&2; exit 1; }

compile() {
    # compile <backend> <fixture> <out-name>; echoes rc, captures log.
    local backend="$1" fixture="$2" out_name="$3"
    local src out rc
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$fixture")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$out_name")"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$out_name.log" 2>&1
    rc=$?
    set -e
    return $rc
}

expect_reject() {
    local backend="$1" fixture="$2" needle="$3"
    if compile "$backend" "$fixture" "rej_${backend}_${fixture%.pgy}.exe"; then
        fail "$backend/$fixture compiled but must fail closed"
    fi
    grep -Fq "$needle" "$OUT_DIR/rej_${backend}_${fixture%.pgy}.exe.log" ||
        fail "$backend/$fixture failed without the expected diagnostic: $needle"
}

expect_runs() {
    local backend="$1" fixture="$2" want="$3"
    local exe="run_${backend}_${fixture%.pgy}.exe"
    compile "$backend" "$fixture" "$exe" ||
        fail "$backend/$fixture must compile: $(tail -2 "$OUT_DIR/$exe.log")"
    local got
    got="$("$OUT_DIR/$exe" | tr -d '\r')" || fail "$backend/$fixture crashed at runtime"
    [ "$got" = "$want" ] || fail "$backend/$fixture printed '$got', expected '$want'"
}

for backend in c llvm; do
    # Constructed parameter positions bind from the checked argument type.
    expect_runs "$backend" nested_param.pgy "1"
    expect_runs "$backend" nested_array_result.pgy $'3\n4\na\n3\n9'
    expect_runs "$backend" nested_where.pgy "1"
    expect_runs "$backend" class_member_argument.pgy "7"
    expect_runs "$backend" shadowed_argument.pgy $'inner\n1'

    # G-1 cell: return position + body-locals (run-equal parity).
    expect_runs "$backend" nested_return.pgy "7"
    expect_runs "$backend" body_local.pgy "9"

    # No false positives: bare-T generics stay green.
    expect_runs "$backend" bare_ok.pgy "42"

    # Refused at native semantic, with the checker's code.
    expect_reject "$backend" nested_conflict.pgy \
        "binds generic parameter 'T' to both 'Int' (argument 1) and 'String' (argument 2)"
    expect_reject "$backend" explicit_conflict.pgy \
        "binds generic parameter 'T' to 'Int' by its explicit type argument, but argument 1 gives it 'String'"
    expect_reject "$backend" nested_unbound.pgy \
        "cannot infer generic parameter 'T' from its arguments"
done
# The refusals are semantic diagnostics with a code and a position, not an
# uncoded mir_lower stage failure at location null.
expect_semantic_code() {
    local fixture="$1" code="$2"
    local log="$OUT_DIR/json_${fixture%.pgy}.log"
    local src out
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$fixture")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/json_${fixture%.pgy}.exe")"
    if (cd "$ROOT_DIR" && "$PGY" "$src" --backend=llvm --error-format=json \
            -o "$out") >"$log" 2>&1; then
        fail "$fixture compiled under --error-format=json"
    fi
    grep -Fq "\"stage\":\"semantic\"" "$log" && grep -Fq "\"code\":\"$code\"" "$log" \
        && grep -Fq '"location":{"line":' "$log" ||
        fail "$fixture is not a positioned semantic $code refusal: $(head -c 300 "$log")"
}
expect_semantic_code nested_conflict.pgy PGY_SEM_TYPE_MISMATCH
expect_semantic_code explicit_conflict.pgy PGY_SEM_TYPE_MISMATCH
expect_semantic_code nested_unbound.pgy PGY_SEM_INFER_GENERIC

# Generic struct parameter: runs on both backends. Native C declares the
# Crate_Int layout ahead of the Open_Int prototype that takes it by value.
expect_runs   llvm nested_struct_param.pgy "4"
expect_runs   c    nested_struct_param.pgy "4"

# Class-generic constructed-over-T FIELD: C substitutes and runs; LLVM fails
# closed on aggregate lowering. G-5 owns closing it.
expect_runs   c    class_field.pgy $'7\n7'
expect_reject llvm class_field.pgy "movable handle lowering"

echo "[generic-nested] checker-sealed generic bindings: constructed params run on c and llvm; conflicting and unbindable calls refused at semantic"
