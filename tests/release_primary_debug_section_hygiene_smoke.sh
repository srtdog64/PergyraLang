#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"

LABEL="release-primary-debug-section-hygiene"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"

fail() {
    echo "[$LABEL] $*" >&2
    exit 1
}

pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v objdump >/dev/null 2>&1 || fail "objdump is required"
command -v nm >/dev/null 2>&1 || fail "nm is required"

TMP_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/pgy-release-hygiene.XXXXXX")"
trap 'rm -rf "$TMP_ROOT"' EXIT

# Keep the source spelling relative to the repository root. The installed
# source-LLVM driver currently owns relative source requests; absolute Windows
# path admission is a separate path-protocol concern, not this release gate.
SOURCE_ARG="examples/hello.pgy"
C_BIN="$TMP_ROOT/hello-c"
LLVM_BIN="$TMP_ROOT/hello-llvm"
NATIVE_C_BIN="$TMP_ROOT/hello-native-c"
NATIVE_LLVM_BIN="$TMP_ROOT/hello-native-llvm"
if pgy_binary_expects_windows_paths "$PGY"; then
    C_BIN="${C_BIN}.exe"
    LLVM_BIN="${LLVM_BIN}.exe"
    NATIVE_C_BIN="${NATIVE_C_BIN}.exe"
    NATIVE_LLVM_BIN="${NATIVE_LLVM_BIN}.exe"
fi
C_BIN_ARG="$(pgy_path_for_compiler "$PGY" "$C_BIN")"
LLVM_BIN_ARG="$(pgy_path_for_compiler "$PGY" "$LLVM_BIN")"
NATIVE_C_BIN_ARG="$(pgy_path_for_compiler "$PGY" "$NATIVE_C_BIN")"
NATIVE_LLVM_BIN_ARG="$(pgy_path_for_compiler "$PGY" "$NATIVE_LLVM_BIN")"

debug_sections() {
    local listing=""
    listing="$(objdump -h "$1" 2>/dev/null)" || return 2
    printf '%s\n' "$listing" \
        | awk '$2 ~ /^(\.debug|\.zdebug|__debug)/ { print $2 }'
    return 0
}

assert_release_primary() {
    local artifact="$1"
    local backend="$2"
    local sections=""
    local symbols=""
    local output=""

    [[ -s "$artifact" ]] || fail "$backend produced no primary executable"
    sections="$(debug_sections "$artifact")"
    if [[ -n "$sections" ]]; then
        printf '%s\n' "$sections" >&2
        fail "$backend release retained debug sections"
    fi
    symbols="$(nm "$artifact" 2>/dev/null || true)"
    if [[ -n "$symbols" ]]; then
        printf '%s\n' "$symbols" | head -n 20 >&2
        fail "$backend release retained an ordinary symbol table"
    fi
    output="$("$artifact")"
    [[ "$output" == "Hello, Pergyra!" ]] \
        || fail "$backend release behavior changed: $output"
}

"$PGY" "$SOURCE_ARG" --backend=c --opt=release -o "$C_BIN_ARG" >/dev/null
"$PGY" "$SOURCE_ARG" --backend=llvm --opt=release -o "$LLVM_BIN_ARG" >/dev/null
"$PGY" "$SOURCE_ARG" --backend=c --opt=release --native-pipeline \
    -o "$NATIVE_C_BIN_ARG" >/dev/null
"$PGY" "$SOURCE_ARG" --backend=llvm --opt=release --native-pipeline \
    -o "$NATIVE_LLVM_BIN_ARG" >/dev/null
assert_release_primary "$C_BIN" C
assert_release_primary "$LLVM_BIN" LLVM
assert_release_primary "$NATIVE_C_BIN" NATIVE_C
assert_release_primary "$NATIVE_LLVM_BIN" NATIVE_LLVM

for consumer in compiler.c compiler_llvm.c compiler_self_host_artifact.c; do
    grep -Fq '#include "compiler_release_artifact_policy.h"' \
        "$ROOT_DIR/src/compiler/$consumer" \
        || fail "$consumer does not consume the shared release policy"
    if grep -Eq '"-s"|"-Wl,-S,-x"' "$ROOT_DIR/src/compiler/$consumer"; then
        fail "$consumer reconstructs a backend-local strip policy"
    fi
done
[[ "$(grep -Fc 'compiler_release_artifact_link_flag(opt_profile)' \
    "$ROOT_DIR/src/compiler/compiler_self_host_artifact.c")" -eq 2 ]] \
    || fail "self-host C/LLVM final links do not both consume the policy"

# The typed policy must be release-only. The explicit native developer lane is
# the currently supported owner for line metadata; it must remain inspectable.
DEV_BIN="$TMP_ROOT/hello-dev"
if pgy_binary_expects_windows_paths "$PGY"; then
    DEV_BIN="${DEV_BIN}.exe"
fi
DEV_BIN_ARG="$(pgy_path_for_compiler "$PGY" "$DEV_BIN")"
"$PGY" "$SOURCE_ARG" --backend=llvm --opt=dev --debug-lines \
    --native-pipeline -o "$DEV_BIN_ARG" >/dev/null
[[ -n "$(debug_sections "$DEV_BIN")" ]] \
    || fail "developer debug profile was stripped by release policy"
[[ "$("$DEV_BIN")" == "Hello, Pergyra!" ]] \
    || fail "developer debug profile behavior changed"

# The inspector must reject a real debug-bearing artifact; a gate that only
# passes clean outputs cannot prove that its forbidden-path check is live.
CC_BIN="${PGY_CC:-${CC:-cc}}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"
printf '%s\n' '#include <stdio.h>' 'int main(void) { puts("Hello, Pergyra!"); return 0; }' \
    > "$TMP_ROOT/debug-negative.c"
NEGATIVE_BIN="$TMP_ROOT/debug-negative"
if pgy_binary_expects_windows_paths "$PGY"; then
    NEGATIVE_BIN="${NEGATIVE_BIN}.exe"
fi
"$CC_BIN" -O0 -g "$TMP_ROOT/debug-negative.c" -o "$NEGATIVE_BIN"
[[ -n "$(debug_sections "$NEGATIVE_BIN")" ]] \
    || fail "negative debug fixture did not contain a detectable debug section"

# A same-toolchain C++ baseline is useful comparison evidence. The focused gate
# remains load-bearing without it on minimal C-only hosts; full acceptance still
# requires the cross-platform baseline matrix owned by the release contract.
CXX_BIN="${CXX:-}"
if [[ -z "$CXX_BIN" ]]; then
    for candidate in c++ g++ clang++; do
        if command -v "$candidate" >/dev/null 2>&1; then
            CXX_BIN="$candidate"
            break
        fi
    done
fi
if [[ -n "$CXX_BIN" ]] && command -v "$CXX_BIN" >/dev/null 2>&1; then
    CC_TARGET="$($CC_BIN -dumpmachine 2>/dev/null || true)"
    CXX_TARGET="$($CXX_BIN -dumpmachine 2>/dev/null || true)"
    [[ -n "$CC_TARGET" && "$CC_TARGET" == "$CXX_TARGET" ]] \
        || fail "C++ baseline target does not match Pergyra host toolchain"
    printf '%s\n' '#include <iostream>' 'int main() { std::cout << "Hello, Pergyra!\n"; }' \
        > "$TMP_ROOT/baseline.cpp"
    BASELINE_BIN="$TMP_ROOT/baseline-cpp"
    if pgy_binary_expects_windows_paths "$PGY"; then
        BASELINE_BIN="${BASELINE_BIN}.exe"
    fi
    CXX_STRIP_FLAG="-s"
    if [[ "$(uname -s 2>/dev/null || echo unknown)" == Darwin* ]]; then
        CXX_STRIP_FLAG="-Wl,-S,-x"
    fi
    "$CXX_BIN" -O3 "$CXX_STRIP_FLAG" "$TMP_ROOT/baseline.cpp" \
        -o "$BASELINE_BIN"
    assert_release_primary "$BASELINE_BIN" CXX_BASELINE
    echo "[$LABEL] sizes self-c=$(wc -c < "$C_BIN") self-llvm=$(wc -c < "$LLVM_BIN") native-c=$(wc -c < "$NATIVE_C_BIN") native-llvm=$(wc -c < "$NATIVE_LLVM_BIN") cpp=$(wc -c < "$BASELINE_BIN")"
else
    echo "[$LABEL] C++ baseline SKIP: no C++ compiler"
fi

echo "[$LABEL] self/native C/LLVM release primaries are stripped and behavior-preserving"
