#!/usr/bin/env bash
# One linked runtime TU, seven shared-profile consumers, no installed-driver run.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
for consumer in direct_mir_array_int_readonly_ref_owner \
    direct_mir_cfg_identity_digest_owner direct_mir_scalar_array_string_readonly_ref_owner \
    direct_mir_scalar_entrypoint_early_return_owner \
    direct_mir_scalar_logical_record_collection_fields_owner \
    direct_mir_scalar_set_string_value_parameter_owner effect_builtin_execution_owner; do
    path="$ROOT_DIR/tests/self_hosted/parity/$consumer.sh"
    grep -Fq 'source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"' "$path"
    grep -Fq 'pgy_selfhost_select_linked_runtime_compile_profile' "$path"
    grep -Fq '"${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}"' "$path"
    if grep -Fq -- '-std=c11 -DPGY_LLVM_ENABLED' "$path"; then
        echo "[runtime-c-profile] $consumer reopened a bare runtime TU" >&2; exit 1
    fi
done
grep -Fq -- '-std=c11 -pthread \' "$ROOT_DIR/tests/lane_scheduler_smoke.sh"
grep -Fq -- '-std=c11 -pthread -O2 -g' "$ROOT_DIR/tests/source_test_harness_compile_smoke.sh"
"${CC:-cc}" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$work/runtime.o"
if (uname() { printf 'UnsupportedPergyraTestOS\n'; }; \
    pgy_selfhost_select_linked_runtime_compile_profile) >"$work/invalid.out" 2>&1; then
    echo '[runtime-c-profile] unsupported platform was admitted' >&2; exit 1
else
    rc=$?
    [[ "$rc" == 2 ]]
    grep -Fq 'self-host-runtime-compile-platform-invalid' "$work/invalid.out"
fi
if CC=false PGY_TEST_HARNESS_BUILD_DIR="$work/unusable" \
    bash "$ROOT_DIR/tests/source_test_harness_compile_smoke.sh" >"$work/unusable.out" 2>&1; then
    echo '[runtime-c-profile] unusable harness compiler appeared green' >&2; exit 1
else
    grep -Fq 'C compiler is not usable' "$work/unusable.out"
fi
echo '[runtime-c-profile] unique runtime TUs=1; profile consumers=7; unsupported platform/compiler refused; PASS'
