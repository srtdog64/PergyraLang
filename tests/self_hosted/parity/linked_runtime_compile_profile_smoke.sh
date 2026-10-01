#!/usr/bin/env bash
# Profile selection contract only; this does not prove platform compilation.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
fail() { echo "[linked-runtime-compile-profile] $*" >&2; exit 1; }
check_profile() (
    local test_platform="$1" expected="$2"
    uname() { printf '%s\n' "$test_platform"; }
    pgy_selfhost_select_linked_runtime_compile_profile || fail "$test_platform was refused"
    [[ "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[*]}" == "$expected" ]] ||
        fail "$test_platform compile options drifted"
)
base='-std=c11 -O0 -DPGY_LLVM_ENABLED -pthread'
check_profile Linux "$base -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE"
check_profile Darwin "$base -D_DARWIN_C_SOURCE -D_XOPEN_SOURCE=700"
check_profile MINGW64_NT "$base"
check_profile MSYS_NT "$base"
check_profile CYGWIN_NT "$base"
(
    uname() { printf 'Plan9\n'; }
    status=0
    pgy_selfhost_select_linked_runtime_compile_profile || status=$?
    [[ "$status" == 2 ]] || fail "unknown platform did not fail closed"
)
(
    uname() { return 1; }
    status=0
    pgy_selfhost_select_linked_runtime_compile_profile || status=$?
    [[ "$status" == 2 ]] || fail "unreadable platform did not fail closed"
)
echo '[linked-runtime-compile-profile] 5 profile mappings + 2 refusals PASS (not platform build evidence)'
