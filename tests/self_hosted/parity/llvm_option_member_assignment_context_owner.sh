#!/usr/bin/env bash
# A registered member field owns the contextual Option layout for its RHS.
# Contextless Some remains fail-closed; no anonymous aggregate is permitted.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-llvm-option-member-assignment-context"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_REL=".tmp/self_hosted/llvm_option_member_assignment_context"
WORK_DIR="$ROOT_DIR/$WORK_REL"
POSITIVE="tests/self_hosted/fixtures/llvm_option_member_assignment_context.pgy"
NEGATIVE="tests/self_hosted/fixtures/llvm_option_contextless_some_negative.pgy"
ASSIGNMENT_OWNER="$ROOT_DIR/src/codegen/llvm_expr_assignment_member_projection.c"
OPTION_CONSUMER="$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
[[ -f "$ROOT_DIR/$POSITIVE" && -f "$ROOT_DIR/$NEGATIVE" ]] ||
    fail "fixture set is incomplete"

grep -Fq 'saved_current_ret_type = ctx->current_ret_type;' \
    "$ASSIGNMENT_OWNER" || fail "member assignment does not snapshot context"
grep -Fq 'ctx->current_ret_type = field_type;' "$ASSIGNMENT_OWNER" ||
    fail "registered field layout is not the RHS context"
grep -Fq 'ctx->current_ret_type = saved_current_ret_type;' \
    "$ASSIGNMENT_OWNER" || fail "member assignment context is not restored"
grep -Fq 'LLVM Some(value) requires contextual Option<T>;' \
    "$OPTION_CONSUMER" || fail "contextless Some no longer fails closed"

[[ "$WORK_DIR" == "$ROOT_DIR/.tmp/self_hosted/llvm_option_member_assignment_context" ]] ||
    fail "refusing to clean an unexpected work directory"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
for backend in c llvm; do
    output_rel="$WORK_REL/positive-$backend$suffix"
    if ! (cd "$ROOT_DIR" && "$PGY" "$POSITIVE" --native-pipeline \
        --backend="$backend" -o "$output_rel") \
        >"$WORK_DIR/positive-$backend.compile.out" \
        2>"$WORK_DIR/positive-$backend.compile.err"; then
        cat "$WORK_DIR/positive-$backend.compile.out" \
            "$WORK_DIR/positive-$backend.compile.err" >&2
        fail "$backend rejected a member-owned Option constructor"
    fi
    [[ -x "$WORK_DIR/positive-$backend$suffix" ]] ||
        fail "$backend published no executable"
    "$WORK_DIR/positive-$backend$suffix" | tr -d '\r' \
        >"$WORK_DIR/positive-$backend.run"
done

printf '41\ntrue\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/positive-c.run" ||
    fail "C behavior drifted from exact 41/true"
cmp -s "$WORK_DIR/positive-c.run" "$WORK_DIR/positive-llvm.run" ||
    fail "C/LLVM member-owned Option behavior differs"

negative_rel="$WORK_REL/contextless-negative$suffix"
if (cd "$ROOT_DIR" && "$PGY" "$NEGATIVE" --native-pipeline \
    --backend=llvm -o "$negative_rel") \
    >"$WORK_DIR/contextless-negative.out" \
    2>"$WORK_DIR/contextless-negative.err"; then
    fail "LLVM accepted a contextless Some expression"
fi
[[ ! -e "$WORK_DIR/contextless-negative$suffix" ]] ||
    fail "LLVM published an artifact for contextless Some"
grep -Fq 'LLVM Some(value) requires contextual Option<T>' \
    "$WORK_DIR/contextless-negative.err" ||
    fail "contextless Some diagnostic identity drifted"

echo "[$LABEL] member Option C/LLVM parity + contextless negative: PASS"
