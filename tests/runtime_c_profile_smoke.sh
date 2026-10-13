#!/usr/bin/env bash
# One linked runtime TU, no installed-driver run.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
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
echo '[runtime-c-profile] unique runtime TUs=1; unsupported platform/compiler refused; PASS'
