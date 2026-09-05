#!/usr/bin/env bash
set -euo pipefail

# A source rewrite experiment, not an implementation of Intent without facts:
# the three positive programs have equal values/call counts, while a typed
# intent requires exact terminal attribution that an ordinary func need not.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-$ROOT_DIR/bin/pgy-self-driver}")"
BUILD_DIR="${PGY_CONCEPT_INTENT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/concept_semantics_20260905/intent/terminal_substitution}"
mkdir -p "$BUILD_DIR"
pgy_require_runnable_binary_here intent-terminal-substitution "$PGY"
pgy_require_runnable_binary_here intent-terminal-substitution "$DRIVER"
printf '%s\n' 'delivered=7' 'calls=1' 'rejected=9' 'calls=2' >"$BUILD_DIR/expected.run"

fail() { echo "[intent-terminal-substitution] $*" >&2; exit 1; }

for name in single_step_exact function_exact function_rebuilt; do
    source_rel="tests/concept_semantics/intent/$name.pgy"
    for backend in c llvm; do
        exe="$BUILD_DIR/$name.native.$backend.exe"
        (cd "$ROOT_DIR" && "$PGY" --native-pipeline "$source_rel" \
            --backend="$backend" -o "$(pgy_path_for_compiler "$PGY" "$exe")") \
            >"$BUILD_DIR/$name.native.$backend.log" 2>&1 \
            || { cat "$BUILD_DIR/$name.native.$backend.log" >&2; fail "$name/$backend compile"; }
        "$exe" | tr -d '\r' >"$BUILD_DIR/$name.native.$backend.run"
        cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/$name.native.$backend.run" \
            || fail "$name/$backend output or call count differs"
    done
done

for name in single_step_rebuilt_reject single_step_missing_terminal_reject; do
    source_rel="tests/concept_semantics/intent/$name.pgy"
    if (cd "$ROOT_DIR" && "$PGY" --native-pipeline --dir "$source_rel") \
        >"$BUILD_DIR/$name.native.out" 2>"$BUILD_DIR/$name.native.err"; then
        fail "$name native accepted an invalid typed terminal"
    fi
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source_rel") \
        >"$BUILD_DIR/$name.self.out" 2>"$BUILD_DIR/$name.self.err"; then
        fail "$name self accepted an invalid typed terminal"
    fi
    if grep -Eq '^\{|^#include|^typedef' "$BUILD_DIR/$name.self.out"; then
        fail "$name published an artifact on rejection"
    fi
done
grep -Fq "must carry the exact admitted payload binding 'receipt'" \
    "$BUILD_DIR/single_step_rebuilt_reject.native.err"
grep -Fq 'requires one labeled failure terminal for every step' \
    "$BUILD_DIR/single_step_missing_terminal_reject.native.err"
grep -Fq 'typed intent terminal payload identity is invalid' \
    "$BUILD_DIR/single_step_rebuilt_reject.self.out" \
    "$BUILD_DIR/single_step_rebuilt_reject.self.err"
echo '[intent-terminal-substitution] one-step exact payload + func value parity + 2 terminal negatives: PASS'
