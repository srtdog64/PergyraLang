#!/usr/bin/env bash

# Sourced by domain_runtime_zone_sync_execution_owner.sh after it has built the
# current codegen executable. A default-mode zone parameter carries the same
# identity through the language-wide automatic-reference ABI; target-specific
# C mode may not decide whether copying is legal.

: "${ROOT_DIR:?zone parameter gate requires ROOT_DIR}"
: "${BUILD_DIR:?zone parameter gate requires BUILD_DIR}"
: "${CODEGEN_BIN:?zone parameter gate requires CODEGEN_BIN}"
: "${PGY_BIN:?zone parameter gate requires PGY_BIN}"
: "${CC_BIN:?zone parameter gate requires CC_BIN}"

PARAMETER_BUILD_DIR="$BUILD_DIR/zone-parameter-admission"
mkdir -p "$PARAMETER_BUILD_DIR"

IDENTITY_SOURCE="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_parameter_identity.pgy"
IDENTITY_C="$PARAMETER_BUILD_DIR/identity-self.c"
IDENTITY_NATIVE_C="$PARAMETER_BUILD_DIR/identity-native.c"
IDENTITY_EXPECTED="$PARAMETER_BUILD_DIR/identity.expected"
IDENTITY_SELF_SINGLE_BIN="$PARAMETER_BUILD_DIR/identity-self-single.exe"
IDENTITY_SELF_THREADSAFE_BIN="$PARAMETER_BUILD_DIR/identity-self-threadsafe.exe"
IDENTITY_NATIVE_SINGLE_BIN="$PARAMETER_BUILD_DIR/identity-native-single.exe"
IDENTITY_NATIVE_THREADSAFE_BIN="$PARAMETER_BUILD_DIR/identity-native-threadsafe.exe"

"$CODEGEN_BIN" --source "${IDENTITY_SOURCE#"$ROOT_DIR/"}" >"$IDENTITY_C"
"$PGY_BIN" "$IDENTITY_SOURCE" --native-pipeline --emit-c \
    -o "$IDENTITY_NATIVE_C"
grep -Fq 'int32_t Observe(CounterZone *value)' "$IDENTITY_C"
grep -Fq 'int32_t pgy_u_Observe(CounterZone *value)' "$IDENTITY_NATIVE_C"
if grep -Eq 'CounterZone value[[:space:]]*=[[:space:]]*\*' \
    "$IDENTITY_C" "$IDENTITY_NATIVE_C"; then
    echo "default zone identity regressed to a lock-bearing value copy" >&2
    exit 1
fi

printf '7\n' >"$IDENTITY_EXPECTED"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -Werror=discarded-qualifiers \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    "$IDENTITY_C" -o "$IDENTITY_SELF_SINGLE_BIN"
"$IDENTITY_SELF_SINGLE_BIN" | tr -d '\r' \
    >"$PARAMETER_BUILD_DIR/identity-self-single.out"
cmp -s "$IDENTITY_EXPECTED" \
    "$PARAMETER_BUILD_DIR/identity-self-single.out"

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -Werror=discarded-qualifiers \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    -DPGY_ZONE_THREADSAFE "$IDENTITY_C" \
    -o "$IDENTITY_SELF_THREADSAFE_BIN"
"$IDENTITY_SELF_THREADSAFE_BIN" | tr -d '\r' \
    >"$PARAMETER_BUILD_DIR/identity-self-threadsafe.out"
cmp -s "$IDENTITY_EXPECTED" \
    "$PARAMETER_BUILD_DIR/identity-self-threadsafe.out"

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -Werror=discarded-qualifiers \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    "$IDENTITY_NATIVE_C" -o "$IDENTITY_NATIVE_SINGLE_BIN"
"$IDENTITY_NATIVE_SINGLE_BIN" | tr -d '\r' \
    >"$PARAMETER_BUILD_DIR/identity-native-single.out"
cmp -s "$IDENTITY_EXPECTED" \
    "$PARAMETER_BUILD_DIR/identity-native-single.out"

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -Werror=discarded-qualifiers \
    "${POSIX_FEATURE_FLAGS[@]}" \
    -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -pthread \
    -DPGY_ZONE_THREADSAFE "$IDENTITY_NATIVE_C" \
    -o "$IDENTITY_NATIVE_THREADSAFE_BIN"
"$IDENTITY_NATIVE_THREADSAFE_BIN" | tr -d '\r' \
    >"$PARAMETER_BUILD_DIR/identity-native-threadsafe.out"
cmp -s "$IDENTITY_EXPECTED" \
    "$PARAMETER_BUILD_DIR/identity-native-threadsafe.out"

for return_case in zone world; do
    if [[ "$return_case" == zone ]]; then
        return_source="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_zone_return_rejected.pgy"
    else
        return_source="$ROOT_DIR/tests/self_hosted/fixtures/domain_runtime_world_zone_return_rejected.pgy"
    fi
    return_out="$PARAMETER_BUILD_DIR/return-$return_case.out"
    return_err="$PARAMETER_BUILD_DIR/return-$return_case.err"
    if "$CODEGEN_BIN" --source "${return_source#"$ROOT_DIR/"}" \
        >"$return_out" 2>"$return_err"; then
        echo "$return_case return escaped semantic admission" >&2
        exit 1
    fi
    grep -Fq 'Code: zone_value_copy_requires_transfer' \
        "$return_out" "$return_err"
    if grep -Fq 'Code: unregistered_diagnostic_code' \
        "$return_out" "$return_err"; then
        echo "$return_case return used an unregistered diagnostic" >&2
        exit 1
    fi
    if grep -Eq '^#include |^typedef struct|^static void .*_sync\(' \
        "$return_out"; then
        echo "$return_case return leaked partial C" >&2
        exit 1
    fi
done

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

echo "[domain-runtime-zone-parameter-admission] default identity native/self single/thread-safe parity + explicit ref + zone/world return rejection: PASS"
