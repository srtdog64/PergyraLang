#!/usr/bin/env bash
# Ordered intent binding kind owns participant-pointer versus value ABI.
# A nominal data-bearing enum value must not inherit participant addressing.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-llvm-intent-value-argument-abi"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_REL=".tmp/self_hosted/llvm_intent_value_argument_abi"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURE="tests/self_hosted/fixtures/llvm_intent_value_argument_abi.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
[[ -f "$ROOT_DIR/$FIXTURE" ]] || fail "fixture is missing"

[[ "$WORK_DIR" == "$ROOT_DIR/.tmp/self_hosted/llvm_intent_value_argument_abi" ]] ||
    fail "refusing to clean an unexpected work directory"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
for backend in c llvm; do
    output_rel="$WORK_REL/value-$backend$suffix"
    if ! (cd "$ROOT_DIR" && "$PGY" "$FIXTURE" --native-pipeline \
        --backend="$backend" -o "$output_rel") \
        >"$WORK_DIR/value-$backend.compile.out" \
        2>"$WORK_DIR/value-$backend.compile.err"; then
        cat "$WORK_DIR/value-$backend.compile.out" \
            "$WORK_DIR/value-$backend.compile.err" >&2
        fail "$backend rejected participant plus data-bearing enum value ABI"
    fi
    [[ -x "$WORK_DIR/value-$backend$suffix" ]] ||
        fail "$backend published no executable"
    "$WORK_DIR/value-$backend$suffix" | tr -d '\r' \
        >"$WORK_DIR/value-$backend.run"
done

printf 'true\n1\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/value-c.run" ||
    fail "C behavior drifted from exact true/1"
cmp -s "$WORK_DIR/value-c.run" "$WORK_DIR/value-llvm.run" ||
    fail "C/LLVM intent value-argument behavior differs"

echo "[$LABEL] participant pointer + enum value C/LLVM ABI parity: PASS"
