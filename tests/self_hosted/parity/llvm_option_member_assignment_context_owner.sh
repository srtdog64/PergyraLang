#!/usr/bin/env bash
# A registered member field owns the contextual Option layout for its RHS.
# A Some(value) with no declared Option consumer takes the type the checker
# sealed on the call. The Some layout owner never reads the active return
# layout, so it cannot borrow the enclosing function's Option type, and a
# Some without either fact still fails closed.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-llvm-option-member-assignment-context"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_REL=".tmp/self_hosted/llvm_option_member_assignment_context"
WORK_DIR="$ROOT_DIR/$WORK_REL"
POSITIVE="tests/self_hosted/fixtures/llvm_option_member_assignment_context.pgy"
STATEMENT="tests/self_hosted/fixtures/llvm_option_statement_some.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
[[ -f "$ROOT_DIR/$POSITIVE" && -f "$ROOT_DIR/$STATEMENT" ]] ||
    fail "fixture set is incomplete"

[[ "$WORK_DIR" == "$ROOT_DIR/.tmp/self_hosted/llvm_option_member_assignment_context" ]] ||
    fail "refusing to clean an unexpected work directory"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
for fixture in positive statement; do
    source_rel="$POSITIVE"
    [[ "$fixture" == statement ]] && source_rel="$STATEMENT"
    for backend in c llvm; do
        output_rel="$WORK_REL/$fixture-$backend$suffix"
        if ! (cd "$ROOT_DIR" && "$PGY" "$source_rel" --native-pipeline \
            --backend="$backend" -o "$output_rel") \
            >"$WORK_DIR/$fixture-$backend.compile.out" \
            2>"$WORK_DIR/$fixture-$backend.compile.err"; then
            cat "$WORK_DIR/$fixture-$backend.compile.out" \
                "$WORK_DIR/$fixture-$backend.compile.err" >&2
            fail "$backend rejected the $fixture Option constructor"
        fi
        [[ -x "$WORK_DIR/$fixture-$backend$suffix" ]] ||
            fail "$backend published no $fixture executable"
        "$WORK_DIR/$fixture-$backend$suffix" | tr -d '\r' \
            >"$WORK_DIR/$fixture-$backend.run"
    done
done

printf '41\ntrue\n' >"$WORK_DIR/expected-positive.run"
cmp -s "$WORK_DIR/expected-positive.run" "$WORK_DIR/positive-c.run" ||
    fail "C behavior drifted from exact 41/true"
cmp -s "$WORK_DIR/positive-c.run" "$WORK_DIR/positive-llvm.run" ||
    fail "C/LLVM member-owned Option behavior differs"
printf 'statement some\n' >"$WORK_DIR/expected-statement.run"
cmp -s "$WORK_DIR/expected-statement.run" "$WORK_DIR/statement-c.run" ||
    fail "C statement Some behavior drifted"
cmp -s "$WORK_DIR/statement-c.run" "$WORK_DIR/statement-llvm.run" ||
    fail "C/LLVM statement Some behavior differs"

echo "[$LABEL] member Option + checker-typed statement Some C/LLVM parity: PASS"
