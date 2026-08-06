#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD_DIR="${PGY_SELFHOST_ROLE_RECEIVER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/codegen-role-receiver}"
DRIVER="${PGY_SELFHOST_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver.exe}"
CC_BIN="${PGY_SELFHOST_CC:-gcc}"
CODEGEN_BIN="${PGY_SELFHOST_PREBUILT_CODEGEN:-$BUILD_DIR/codegen.exe}"
CODEGEN_SOURCE_REL="src/self_hosted/codegen/main.pgy"
ROLE_SOURCE_REL="src/self_hosted/codegen/role_fixture/operator_add.pgy"

fail() {
    echo "[self-host-codegen-role-receiver] $*" >&2
    exit 1
}

mkdir -p "$BUILD_DIR"
[[ -x "$DRIVER" ]] || fail "installed sibling driver is missing: $DRIVER"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is missing: $CC_BIN"

if [[ -z "${PGY_SELFHOST_PREBUILT_CODEGEN:-}" ]]; then
    codegen_c="$BUILD_DIR/codegen.c"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-c-artifact-verified \
        "$CODEGEN_SOURCE_REL" \
        "${codegen_c#"$ROOT_DIR/"}") \
        >"$BUILD_DIR/codegen.emit.out" 2>"$BUILD_DIR/codegen.emit.err" \
        || { cat "$BUILD_DIR/codegen.emit.err" >&2; fail "current codegen C emission failed"; }
    [[ -s "$codegen_c" ]] || fail "current codegen C artifact is empty"
    "$CC_BIN" -x c -std=c11 -O3 -fwrapv -fno-strict-aliasing \
        -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" "$codegen_c" \
        -o "$CODEGEN_BIN" 2>"$BUILD_DIR/codegen.cc.err" \
        || { cat "$BUILD_DIR/codegen.cc.err" >&2; fail "current codegen C compile failed"; }
fi
[[ -x "$CODEGEN_BIN" ]] || fail "codegen executable is missing: $CODEGEN_BIN"

run_role_case() {
    local label="$1" source_rel="$2" expected="$3"
    local c_file="$BUILD_DIR/$label.c"
    local exe="$BUILD_DIR/$label.exe"
    local raw="$BUILD_DIR/$label.out.raw"
    local out="$BUILD_DIR/$label.out"
    (cd "$ROOT_DIR" && "$CODEGEN_BIN" --source "$source_rel" \
        >"$c_file" 2>"$BUILD_DIR/$label.emit.err") \
        || { cat "$BUILD_DIR/$label.emit.err" >&2; fail "$label source codegen failed"; }
    grep -Fq 'long long IntMath_Add(void *_pgy_raw_self, long long rhs)' "$c_file" \
        || fail "$label lost erased role method ABI"
    grep -Fq 'long long self = _pgy_raw_self ? *(long long *)_pgy_raw_self' "$c_file" \
        || fail "$label lost scalar value receiver binding"
    grep -Fq 'long long lhs_copy = lhs;' "$c_file" \
        || fail "$label lost stable erased-role argument address"
    grep -Fq 'operator_add_Int' "$c_file" \
        || fail "$label lost admitted role operator adapter"
    ! grep -Fq 'operator_sub_Int' "$c_file" \
        || fail "$label invented a foreign role operator adapter"
    ! grep -Fq 'long long *self = ' "$c_file" \
        || fail "$label treated scalar target as mutable nominal identity"
    "$CC_BIN" -std=c11 -I"$ROOT_DIR/src/runtime" "$c_file" -o "$exe" \
        2>"$BUILD_DIR/$label.cc.err" \
        || { cat "$BUILD_DIR/$label.cc.err" >&2; fail "$label emitted C compile failed"; }
    "$exe" >"$raw" 2>"$BUILD_DIR/$label.run.err" \
        || { cat "$BUILD_DIR/$label.run.err" >&2; fail "$label executable failed"; }
    tr -d '\r' <"$raw" >"$out"
    [[ "$(cat "$out")" == "$expected" ]] \
        || { cat "$out" >&2; fail "$label stdout drifted"; }
}

run_role_case base "$ROLE_SOURCE_REL" $'123\n123\n3'

mutated_source="$BUILD_DIR/operator_add_321.pgy"
sed '0,/return 123;/s//return 321;/' "$ROOT_DIR/$ROLE_SOURCE_REL" >"$mutated_source"
run_role_case mutated "${mutated_source#"$ROOT_DIR/"}" $'321\n321\n3'

rejected_source="$BUILD_DIR/operator_add_noncopyable_target.pgy"
sed '0,/role IntMath for Int/s//role IntMath for String/' \
    "$ROOT_DIR/$ROLE_SOURCE_REL" >"$rejected_source"
rejected_c="$BUILD_DIR/rejected.c"
if (cd "$ROOT_DIR" && "$CODEGEN_BIN" --source \
    "${rejected_source#"$ROOT_DIR/"}" >"$rejected_c" \
    2>"$BUILD_DIR/rejected.err"); then
    fail "non-copyable builtin role target was accepted"
fi
grep -Fq 'role receiver builtin target is not an admitted plain value' \
    "$rejected_c" "$BUILD_DIR/rejected.err" \
    || fail "non-copyable builtin target lost its owned diagnostic"
! grep -Fq '#include <stdio.h>' "$rejected_c" \
    || fail "partial C escaped before role target rejection"

receiver_owner="$ROOT_DIR/src/self_hosted/codegen/input/callable_receiver_codegen_view_owner.pgy"
binding_owner="$ROOT_DIR/src/self_hosted/codegen/emission/role_receiver_binding_owner.pgy"
function_owner="$ROOT_DIR/src/self_hosted/codegen/emission/function_emit.pgy"
grep -Fq 'role_target_types: Array<String>' "$receiver_owner" \
    || fail "callable receiver target-type carriage disappeared"
grep -Fq 'role_target_carriages: Array<String>' "$receiver_owner" \
    || fail "callable receiver target representation carriage disappeared"
! grep -Fq 'LookupKindTypeRowPresent(env, target_type, "nk")' "$binding_owner" \
    || fail "role binding resumed late nominal-kind reconstruction"
! grep -Fq 'CodegenSemanticRoleReceiverType(' "$function_owner" \
    || fail "function emission resumed role-target rescanning"

echo "[self-host-codegen-role-receiver] PASS exact base/metamorphic execution and negative admission"
