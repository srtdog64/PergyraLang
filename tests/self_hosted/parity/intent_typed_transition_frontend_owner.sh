#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/parser_tool_build_leg.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
PGY_EXEC="$(pgy_path_for_bash_tool "$PGY")"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_typed_transition_frontend}"
PARSER="$BUILD_DIR/parser.exe"
SOURCE="$ROOT_DIR/src/self_hosted/parser/main.pgy"
FIXTURE_REL="tests/self_hosted/parity/fixture/intent_typed_outcome_compensation.pgy"
mkdir -p "$BUILD_DIR"

if ! pgy_selfhost_compile_parser_tool \
    "self-host-intent-typed-frontend" "$SOURCE" c "$PARSER" \
    "$BUILD_DIR/parser.compile.log"; then
    cat "$BUILD_DIR/parser.compile.log" >&2
    exit 1
fi

(cd "$ROOT_DIR" && "$PARSER" "$FIXTURE_REL") >"$BUILD_DIR/positive.ast"
grep -Fq 'IntentReturns: WorkflowOutcome' "$BUILD_DIR/positive.ast"
grep -Fq 'IntentStep: B after A using work' "$BUILD_DIR/positive.ast"
grep -Fq 'IntentStepSuccess: WorkflowASucceeded(receipt_a)' "$BUILD_DIR/positive.ast"
grep -Fq 'IntentStepFailure: WorkflowBFailed(problem_b)' "$BUILD_DIR/positive.ast"
grep -Fq 'IntentTerminalSuccess: B => WorkflowCommitted(receipt_b)' "$BUILD_DIR/positive.ast"
grep -Fq 'IntentTerminalFailure: A => WorkflowFailedA(problem_a)' "$BUILD_DIR/positive.ast"

reject_source() {
    local label="$1"
    local source="$2"
    if (cd "$ROOT_DIR" && "$PARSER" "$source") \
        >"$BUILD_DIR/$label.out" 2>"$BUILD_DIR/$label.err"; then
        echo "[self-host-intent-typed-frontend] accepted $label" >&2
        exit 1
    fi
}

sed '/failure: WorkflowAFailed(problem_a);/d' \
    "$ROOT_DIR/$FIXTURE_REL" >"$BUILD_DIR/missing-step-failure.pgy"
reject_source missing-step-failure \
    "${BUILD_DIR#"$ROOT_DIR/"}/missing-step-failure.pgy"

sed 's/WorkflowASucceeded(receipt_a)/WorkflowASucceeded(receipt_a, extra)/' \
    "$ROOT_DIR/$FIXTURE_REL" >"$BUILD_DIR/multiple-pattern-payloads.pgy"
reject_source multiple-pattern-payloads \
    "${BUILD_DIR#"$ROOT_DIR/"}/multiple-pattern-payloads.pgy"

sed 's/success B: WorkflowCommitted/success: WorkflowCommitted/' \
    "$ROOT_DIR/$FIXTURE_REL" >"$BUILD_DIR/unlabelled-typed-terminal.pgy"
reject_source unlabelled-typed-terminal \
    "${BUILD_DIR#"$ROOT_DIR/"}/unlabelled-typed-terminal.pgy"

# Native semantic/DIR owns the exact terminal carrier.  A terminal may not
# rebuild a same-typed tobject and thereby sever the admitted step payload
# identity, even when ordinary expression type checking would accept it.
(cd "$ROOT_DIR" && "$PGY_EXEC" --dir "$FIXTURE_REL" \
    >"$BUILD_DIR/native-positive.dir" \
    2>"$BUILD_DIR/native-positive.err")
sed 's/WorkflowCommitted(receipt_b)/WorkflowCommitted(WorkflowReceiptB(999))/' \
    "$ROOT_DIR/$FIXTURE_REL" >"$BUILD_DIR/rebuilt-terminal-payload.pgy"
if (cd "$ROOT_DIR" && "$PGY_EXEC" --dir \
    "${BUILD_DIR#"$ROOT_DIR/"}/rebuilt-terminal-payload.pgy" \
    >"$BUILD_DIR/rebuilt-terminal-payload.dir" \
    2>"$BUILD_DIR/rebuilt-terminal-payload.err"); then
    echo "[self-host-intent-typed-frontend] native semantic accepted rebuilt terminal payload" >&2
    exit 1
fi
grep -Fq "must carry the exact admitted payload binding 'receipt_b'" \
    "$BUILD_DIR/rebuilt-terminal-payload.err"

grep -Fq 'enum_matches == 1 && variant_matches == 1' \
    "$ROOT_DIR/src/self_hosted/semantic/ast_intent_transition_row_owner.pgy"
grep -Fq 'typed intent predecessor evidence is invalid' \
    "$ROOT_DIR/src/self_hosted/semantic/ast_intent_transition_fact_owner.pgy"
grep -Fq 'typed intent terminal mapping coverage is incomplete' \
    "$ROOT_DIR/src/self_hosted/semantic/ast_intent_transition_fact_owner.pgy"

echo "[self-host-intent-typed-frontend] PASS"
