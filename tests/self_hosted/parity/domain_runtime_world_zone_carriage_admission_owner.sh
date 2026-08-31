#!/usr/bin/env bash

# Sourced after domain_runtime_zone_sync_execution_owner.sh has built the
# current codegen executable. A world owns embedded zone resources; exact
# semantic paths drive their C lock lifecycle and an admitted `inout` aliases
# the same identity without copy-in/copy-out.

: "${ROOT_DIR:?world-zone carriage gate requires ROOT_DIR}"
: "${BUILD_DIR:?world-zone carriage gate requires BUILD_DIR}"
: "${CODEGEN_BIN:?world-zone carriage gate requires CODEGEN_BIN}"
: "${PGY_BIN:?world-zone carriage gate requires PGY_BIN}"
: "${CC_BIN:?world-zone carriage gate requires CC_BIN}"

WORLD_BUILD_DIR="$BUILD_DIR/world-zone-carriage"
mkdir -p "$WORLD_BUILD_DIR"

WORLD_SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_world_zone_lifecycle.pgy"
WORLD_C="$WORLD_BUILD_DIR/world-zone.c"
WORLD_SINGLE_BIN="$WORLD_BUILD_DIR/world-zone-single.exe"
WORLD_THREADSAFE_BIN="$WORLD_BUILD_DIR/world-zone-threadsafe.exe"
WORLD_SINGLE_OUT="$WORLD_BUILD_DIR/world-zone-single.out"
WORLD_THREADSAFE_OUT="$WORLD_BUILD_DIR/world-zone-threadsafe.out"
WORLD_EXPECTED="$WORLD_BUILD_DIR/world-zone.expected"
WORLD_NATIVE_C="$WORLD_BUILD_DIR/world-zone-native.c"
WORLD_NATIVE_SINGLE_BIN="$WORLD_BUILD_DIR/world-zone-native-single.exe"
WORLD_NATIVE_THREADSAFE_BIN="$WORLD_BUILD_DIR/world-zone-native-threadsafe.exe"
WORLD_NATIVE_SINGLE_OUT="$WORLD_BUILD_DIR/world-zone-native-single.out"
WORLD_NATIVE_THREADSAFE_OUT="$WORLD_BUILD_DIR/world-zone-native-threadsafe.out"

"$CODEGEN_BIN" --source "${WORLD_SOURCE#"$ROOT_DIR/"}" >"$WORLD_C"

if grep -Fq 'Pergyra embedded zone requires an admitted transfer plan' \
    "$WORLD_C"; then
    echo "world-zone lifecycle retained the backend compile guard" >&2
    exit 1
fi
[[ "$(grep -Fc 'PGY_ZONE_LOCK_INIT(&compiler_world.cart);' "$WORLD_C")" == 1 ]]
[[ "$(grep -Fc 'PGY_ZONE_LOCK_DESTROY(&compiler_world.cart);' "$WORLD_C")" -ge 1 ]]
grep -Fq 'int32_t ReadWorld(CartWorld *_pgy_inout_compiler_world)' "$WORLD_C"
grep -Fq 'int32_t ForwardWorld(CartWorld *_pgy_inout_compiler_world)' "$WORLD_C"
if grep -Fq 'CartWorld compiler_world = *_pgy_inout_compiler_world;' \
    "$WORLD_C" || \
   grep -Fq '*_pgy_inout_compiler_world = compiler_world;' "$WORLD_C"; then
    echo "world-zone mutable borrow regressed to lock-bearing copy-in/out" >&2
    exit 1
fi

printf '7\n' >"$WORLD_EXPECTED"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    "$WORLD_C" -o "$WORLD_SINGLE_BIN"
"$WORLD_SINGLE_BIN" | tr -d '\r' >"$WORLD_SINGLE_OUT"
cmp -s "$WORLD_EXPECTED" "$WORLD_SINGLE_OUT"

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    -DPGY_ZONE_THREADSAFE "$WORLD_C" -o "$WORLD_THREADSAFE_BIN"
"$WORLD_THREADSAFE_BIN" | tr -d '\r' >"$WORLD_THREADSAFE_OUT"
cmp -s "$WORLD_EXPECTED" "$WORLD_THREADSAFE_OUT"

# The native C bootstrap oracle consumes the same MIR declaration-header zone
# paths as the Pergyra codegen. It must preserve the caller-owned world identity
# and bracket each embedded lock exactly once even in the thread-safe runtime.
"$PGY_BIN" "$WORLD_SOURCE" --native-pipeline --emit-c -o "$WORLD_NATIVE_C"
grep -Fq 'int32_t ReadWorld(CartWorld *compiler_world)' "$WORLD_NATIVE_C"
grep -Fq 'int32_t ForwardWorld(CartWorld *compiler_world)' "$WORLD_NATIVE_C"
grep -Fq 'return ReadWorld(compiler_world);' "$WORLD_NATIVE_C"
grep -Fq 'ForwardWorld(&_pgy_ssa_compiler_world_1)' "$WORLD_NATIVE_C"
[[ "$(grep -Fc 'PGY_ZONE_LOCK_INIT(&_pgy_ssa_compiler_world_1.cart);' \
    "$WORLD_NATIVE_C")" == 1 ]]
[[ "$(grep -Fc 'PGY_ZONE_LOCK_DESTROY(&_pgy_ssa_compiler_world_1.cart);' \
    "$WORLD_NATIVE_C")" == 1 ]]
if grep -Fq 'compiler_world__mutref' "$WORLD_NATIVE_C" || \
   grep -Fq 'CartWorld compiler_world = *' "$WORLD_NATIVE_C"; then
    echo "native world-zone inout regressed to lock-bearing copy-in/out" >&2
    exit 1
fi

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    "$WORLD_NATIVE_C" -o "$WORLD_NATIVE_SINGLE_BIN"
"$WORLD_NATIVE_SINGLE_BIN" | tr -d '\r' >"$WORLD_NATIVE_SINGLE_OUT"
cmp -s "$WORLD_EXPECTED" "$WORLD_NATIVE_SINGLE_OUT"

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    -DPGY_ZONE_THREADSAFE "$WORLD_NATIVE_C" \
    -o "$WORLD_NATIVE_THREADSAFE_BIN"
"$WORLD_NATIVE_THREADSAFE_BIN" | tr -d '\r' \
    >"$WORLD_NATIVE_THREADSAFE_OUT"
cmp -s "$WORLD_EXPECTED" "$WORLD_NATIVE_THREADSAFE_OUT"

for negative_case in copy reassign default_parameter return; do
    negative_source="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_world_zone_${negative_case}_rejected.pgy"
    negative_out="$WORLD_BUILD_DIR/$negative_case.out"
    negative_err="$WORLD_BUILD_DIR/$negative_case.err"
    negative_code='zone_value_copy_requires_transfer'
    if [[ "$negative_case" == reassign ]]; then
        negative_code='zone_value_reassignment_requires_transfer'
    elif [[ "$negative_case" == default_parameter ]]; then
        negative_code='zone_value_parameter_requires_transfer'
    fi
    if "$CODEGEN_BIN" --source "${negative_source#"$ROOT_DIR/"}" \
        >"$negative_out" 2>"$negative_err"; then
        echo "$negative_case world-zone carriage escaped semantic admission" >&2
        exit 1
    fi
    grep -Fq "Code: $negative_code" "$negative_out" "$negative_err"
    if grep -Eq '^#include |^typedef struct|^static void .*_sync\(' \
        "$negative_out"; then
        echo "$negative_case world-zone rejection leaked partial C" >&2
        exit 1
    fi
done

ZONE_CARRIAGE_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_zone_value_carriage_verdict_owner.pgy"
ZONE_PARAMETER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_zone_parameter_boundary_verdict_owner.pgy"
BODY_VIEW_OWNER="$ROOT_DIR/src/self_hosted/codegen/input/semantic_body_type_codegen_view_owner.pgy"
FUNCTION_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/function_emit.pgy"
STATEMENT_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/stmt_emit.pgy"
NOMINAL_EMIT_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/nominal_struct_emit_owner.pgy"

grep -Fq 'NominalFieldKindWorldZone()' "$ZONE_CARRIAGE_OWNER"
grep -Fq 'mutable_borrow_parameter_node_ids' "$ZONE_CARRIAGE_OWNER"
grep -Fq 'resource_field_paths' "$BODY_VIEW_OWNER"
grep -Fq 'CodegenSemanticMutableResourceParameter(' "$FUNCTION_OWNER"
grep -Fq 'CodegenSemanticResourceLocalPathOrDie(' "$STATEMENT_OWNER"
if grep -Fq 'Pergyra embedded zone requires an admitted transfer plan' \
    "$NOMINAL_EMIT_OWNER"; then
    echo "embedded-zone policy returned to nominal C emission" >&2
    exit 1
fi

echo "[domain-runtime-world-zone-carriage] semantic paths + mutable borrow + Pergyra/native single/thread-safe lifecycle: PASS"
