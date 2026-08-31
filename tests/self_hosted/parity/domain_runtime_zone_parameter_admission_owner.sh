#!/usr/bin/env bash

# Sourced by domain_runtime_zone_sync_execution_owner.sh after it has built the
# current codegen executable. A zone parameter must use an explicit read-only
# ref boundary; target-specific C mode may not decide whether copying is legal.

: "${ROOT_DIR:?zone parameter gate requires ROOT_DIR}"
: "${BUILD_DIR:?zone parameter gate requires BUILD_DIR}"
: "${CODEGEN_BIN:?zone parameter gate requires CODEGEN_BIN}"
: "${CC_BIN:?zone parameter gate requires CC_BIN}"

PARAMETER_BUILD_DIR="$BUILD_DIR/zone-parameter-admission"
mkdir -p "$PARAMETER_BUILD_DIR"

NEGATIVE_SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_parameter_rejected.pgy"
NEGATIVE_OUT="$PARAMETER_BUILD_DIR/default.out"
NEGATIVE_ERR="$PARAMETER_BUILD_DIR/default.err"
if "$CODEGEN_BIN" --source "${NEGATIVE_SOURCE#"$ROOT_DIR/"}" \
    >"$NEGATIVE_OUT" 2>"$NEGATIVE_ERR"; then
    echo "default zone parameter escaped semantic admission" >&2
    exit 1
fi
grep -Fq 'Code: zone_value_parameter_requires_transfer' \
    "$NEGATIVE_OUT" "$NEGATIVE_ERR"
if grep -Fq 'Code: unregistered_diagnostic_code' \
    "$NEGATIVE_OUT" "$NEGATIVE_ERR"; then
    echo "default zone parameter used an unregistered diagnostic" >&2
    exit 1
fi
if grep -Eq '^#include |^typedef struct|^static void .*_sync\(' \
    "$NEGATIVE_OUT"; then
    echo "default zone parameter leaked partial C" >&2
    exit 1
fi

REF_SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_parameter_ref.pgy"
REF_C="$PARAMETER_BUILD_DIR/ref.c"
REF_EXPECTED="$PARAMETER_BUILD_DIR/ref.expected"
REF_SINGLE_BIN="$PARAMETER_BUILD_DIR/ref-single.exe"
REF_THREADSAFE_BIN="$PARAMETER_BUILD_DIR/ref-threadsafe.exe"
REF_SINGLE_OUT="$PARAMETER_BUILD_DIR/ref-single.out"
REF_THREADSAFE_OUT="$PARAMETER_BUILD_DIR/ref-threadsafe.out"

"$CODEGEN_BIN" --source "${REF_SOURCE#"$ROOT_DIR/"}" >"$REF_C"
printf '7\n' >"$REF_EXPECTED"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -Werror=discarded-qualifiers \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    "$REF_C" -o "$REF_SINGLE_BIN"
"$REF_SINGLE_BIN" | tr -d '\r' >"$REF_SINGLE_OUT"
cmp -s "$REF_EXPECTED" "$REF_SINGLE_OUT"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -Werror=discarded-qualifiers \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    -DPGY_ZONE_THREADSAFE "$REF_C" -o "$REF_THREADSAFE_BIN"
"$REF_THREADSAFE_BIN" | tr -d '\r' >"$REF_THREADSAFE_OUT"
cmp -s "$REF_EXPECTED" "$REF_THREADSAFE_OUT"

ZONE_PARAMETER_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_zone_parameter_boundary_verdict_owner.pgy"
FUNCTION_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/function_emit.pgy"
NOMINAL_OWNER="$ROOT_DIR/src/self_hosted/codegen/emission/nominal_struct_emit_owner.pgy"

grep -Fq 'zone_value_parameter_requires_transfer' "$ZONE_PARAMETER_OWNER"
grep -Fq 'UnwrapOption(parameter_mode) != 3' "$ZONE_PARAMETER_OWNER"
if grep -Fq 'Pergyra zone by-value parameter requires an admitted transfer plan' \
    "$FUNCTION_OWNER"; then
    echo "zone parameter policy returned to C function emission" >&2
    exit 1
fi
grep -Fq 'Pergyra zone return requires an admitted transfer plan' \
    "$FUNCTION_OWNER"
grep -Fq 'Pergyra embedded zone requires an admitted transfer plan' \
    "$NOMINAL_OWNER"

echo "[domain-runtime-zone-parameter-admission] default semantic rejection plus ref single/thread-safe execution: PASS"
