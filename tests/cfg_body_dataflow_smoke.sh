#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "missing python for cfg body dataflow smoke" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
doc_path = root / "docs" / "103_cfg_body_dataflow_need.md"
checklist_path = root / "docs" / "100_beta_readiness_checklist.md"
todo_path = root / "TODO.md"
board_path = root / "docs" / "70_beta_closure_master_board.md"
report_path = root / "docs" / "98_beta_closure_readiness_report.md"
flow_path = root / "src" / "semantic" / "type_checker_flow.c"
flow_resources_path = root / "src" / "semantic" / "type_checker_flow_resources.inc"
flow_loops_path = root / "src" / "semantic" / "type_checker_flow_loops.inc"
flow_parallel_path = root / "src" / "semantic" / "type_checker_flow_parallel.inc"
async_channel_path = root / "src" / "semantic" / "type_checker_async_channel.inc"
helpers_effects_path = root / "src" / "semantic" / "type_checker_helpers_effects.inc"
builtins_query_channel_path = root / "src" / "semantic" / "type_checker_builtins_query_channel.inc"
builtins_cancel_path = root / "src" / "semantic" / "type_checker_builtins_cancel.c"
type_system_path = root / "src" / "semantic" / "type_system.h"
type_system_impl_path = root / "src" / "semantic" / "type_system.c"
expr_path = root / "src" / "semantic" / "type_checker_expr.inc"
program_path = root / "src" / "semantic" / "type_checker_program.inc"
diag_path = root / "src" / "semantic" / "diag_codes.h"
diag_doc_path = root / "docs" / "72_diagnostic_codes.md"
parser_path = root / "src" / "parser" / "parser.c"
let_path = root / "src" / "semantic" / "type_checker_ownership_let.c"
semantic_tests_path = root / "src" / "tests" / "semantic" / "test_semantic_misc_a.inc"
semantic_async_tests_path = root / "src" / "tests" / "semantic" / "test_semantic_async.inc"
semantic_effect_tests_path = root / "src" / "tests" / "semantic" / "test_semantic_effects.inc"

for path in (
    doc_path,
    checklist_path,
    todo_path,
    board_path,
    report_path,
    flow_path,
    flow_resources_path,
    flow_loops_path,
    flow_parallel_path,
    async_channel_path,
    helpers_effects_path,
    builtins_query_channel_path,
    builtins_cancel_path,
    type_system_path,
    type_system_impl_path,
    expr_path,
    program_path,
    diag_path,
    diag_doc_path,
    parser_path,
    let_path,
    semantic_tests_path,
    semantic_async_tests_path,
    semantic_effect_tests_path,
):
    if not path.exists():
        raise SystemExit(f"missing required cfg/body dataflow document: {path.relative_to(root)}")

doc = doc_path.read_text(encoding="utf-8")
checklist = checklist_path.read_text(encoding="utf-8")
todo = todo_path.read_text(encoding="utf-8")
board = board_path.read_text(encoding="utf-8")
report = report_path.read_text(encoding="utf-8")
flow = (
    flow_path.read_text(encoding="utf-8")
    + "\n"
    + flow_resources_path.read_text(encoding="utf-8")
    + "\n"
    + flow_loops_path.read_text(encoding="utf-8")
    + "\n"
    + flow_parallel_path.read_text(encoding="utf-8")
    + "\n"
    + async_channel_path.read_text(encoding="utf-8")
    + "\n"
    + helpers_effects_path.read_text(encoding="utf-8")
    + "\n"
    + builtins_query_channel_path.read_text(encoding="utf-8")
    + "\n"
    + builtins_cancel_path.read_text(encoding="utf-8")
    + "\n"
    + type_system_path.read_text(encoding="utf-8")
    + "\n"
    + type_system_impl_path.read_text(encoding="utf-8")
    + "\n"
    + expr_path.read_text(encoding="utf-8")
)
program = program_path.read_text(encoding="utf-8")
diag = diag_path.read_text(encoding="utf-8")
diag_doc = diag_doc_path.read_text(encoding="utf-8")
parser = parser_path.read_text(encoding="utf-8")
let_checker = let_path.read_text(encoding="utf-8")
semantic_tests = (
    semantic_tests_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_async_tests_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_effect_tests_path.read_text(encoding="utf-8")
)

required_doc_terms = [
    "HIR has function CFG v0",
    "RIR carries flow-block summaries",
    "MIR has routine/block/instruction/cleanup blocks",
    "All-path return",
    "Definite assignment",
    "Move/use-after-move",
    "Borrow lifetime",
    "Drop/cleanup",
    "Zone/effect transition",
    "Parallel/channel boundary",
    "Interprocedural summaries",
    "Diagnostics Contract",
    "Implementation Skeleton",
    "Completion Criteria",
    "PGY_SEM_UNINIT_LOCAL",
]

missing = [term for term in required_doc_terms if term not in doc]
if missing:
    raise SystemExit("cfg body dataflow doc missing terms: " + ", ".join(missing))

if "docs/103_cfg_body_dataflow_need.md" not in checklist:
    raise SystemExit("beta readiness checklist must reference CFG body dataflow source doc")
if "docs/103_cfg_body_dataflow_need.md" not in todo:
    raise SystemExit("TODO must reference CFG body dataflow source doc")
if "Function CFG / body dataflow" not in board:
    raise SystemExit("master board must track Function CFG / body dataflow")
if "Function CFG And Body Dataflow Source Of Truth" not in report:
    raise SystemExit("readiness report must track CFG/body source-of-truth blocker")
if "make cfg-body-dataflow-test-smoke" not in checklist:
    raise SystemExit("checklist must include cfg-body-dataflow-test-smoke")

required_flow_terms = [
    "FLOW_FALLTHROUGH",
    "FLOW_RETURN",
    "type_check_if_stmt_flow",
    "type_check_match_stmt_flow",
    "semantic_check_body_flow",
    "match_stmt_has_total_case_coverage",
    "flow_record_unreachable_statement",
    "loop_flow_record",
    "type_check_while_loop",
    "type_check_for_loop",
    "merge_resource_snapshots_or",
    "type_check_defer_body_flow",
    "type_check_parallel_block_flow",
    "flow_snapshot_tracks_symbol",
    "semantic_classify_ownership_type",
    "used_states",
    "PGY_CAUSE_PARALLEL_RESOURCE_CONFLICT",
    "semantic_validate_spawn_ref_boundary",
    "semantic_reject_anonymous_async_spawn",
    "cannot cross spawn boundary",
    "Anonymous async spawn bodies are beta-out-of-scope",
    "type_check_channel_close_builtin",
    "type_check_cancel_rejects_payload",
    "body_summary_mask",
    "type_function_body_summary",
    "semantic_record_callee_body_summary",
    "semantic_record_callable_decl_summary",
    "lambda_body_summary",
    "BODY_SUMMARY_SPAWNS_TASK",
    "BODY_SUMMARY_SENDS_CHANNEL",
    "BODY_SUMMARY_MAY_ESCAPE_REF",
    "ChannelClose does not support",
    "Cancel does not support",
    "cannot yield Token values yet",
    "PGY_CODE_SEM_BORROW_ESCAPE",
    "SlotState",
    "ResourceConsumeSnapshot before_defer",
    "type_check_defer_body_flow(node->data.defer_stmt.body, ctx)",
    "type_check_block_flow(body, ctx, NULL)",
    "restore_resource_states(&before_defer)",
]
missing_flow = [term for term in required_flow_terms if term not in flow]
if missing_flow:
    raise SystemExit(
        "semantic CFG body flow is missing implementation terms: "
        + ", ".join(missing_flow)
    )

for term in [
    "semantic_check_body_flow",
    "PGY_CODE_SEM_MISSING_RETURN",
    "PGY_CAUSE_CFG_MISSING_RETURN",
]:
    if term not in program:
        raise SystemExit(f"function declaration checker is not wired to {term}")

for term in [
    "PGY_CODE_SEM_MISSING_RETURN",
    "PGY_CAUSE_CFG_MISSING_RETURN",
    "PGY_FIX_ADD_RETURN_ON_ALL_PATHS",
    "PGY_CODE_SEM_UNREACHABLE_CODE",
    "PGY_CAUSE_CFG_UNREACHABLE_STATEMENT",
    "PGY_FIX_REMOVE_OR_MOVE_BEFORE_TERMINATOR",
]:
    if term not in diag:
        raise SystemExit(f"diagnostic registry is missing {term}")

for term in ["PGY_SEM_MISSING_RETURN", "PGY_SEM_UNREACHABLE_CODE"]:
    if term not in diag_doc:
        raise SystemExit(f"diagnostic docs must document {term}")

for term in [
    "CFG body flow warns on unreachable statement after return",
    "CFG body flow warns after all if branches terminate",
    "CFG body flow warns after exhaustive match terminates",
    "CFG body flow warns after loop break terminates path",
    "CFG body flow warns after loop continue terminates path",
    "CFG loop move join consumes QubitSlot on break path",
    "CFG loop move join rejects consumed QubitSlot on continue backedge",
    "CFG defer return does not make following statement unreachable",
    "CFG defer return does not satisfy non-Void all-path return",
    "CFG defer QubitSlot release does not consume current path",
    "CFG defer loop break does not consume current path resource state",
    "defer statement fallback restores QubitSlot resource state",
    "CFG slot release in terminating branch does not poison fallthrough path",
    "CFG slot release in fallthrough branch poisons joined path",
    "CFG own subject move in terminating branch does not poison fallthrough path",
    "CFG own subject move in fallthrough branch poisons joined path",
    "CFG parallel task return does not terminate outer path",
    "CFG parallel task move consumes resource after join",
    "CFG parallel task own subject move consumes boundary after join",
    "CFG parallel tasks reject double resource consume",
    "CFG parallel tasks reject double own subject consume",
    "CFG parallel tasks reject ref and own subject boundary conflict",
    "CFG parallel tasks allow shared ref subject boundary reads",
    "CFG spawn rejects borrowed subject boundary crossing",
    "CFG spawn allows copy ref boundary crossing",
    "CFG spawn rejects anonymous async body until capture lifetime is closed",
    "CFG parallel channel send consumes resource after join",
    "CFG parallel channel sends reject double resource consume",
    "TryRecv rejects movable resource channel payloads",
    "RecvTimeout rejects movable resource channel payloads",
    "TryRecv rejects anchored slot-handle channel payloads",
    "RecvTimeout rejects boundary-value channel payloads",
    "TryRecv rejects authority Token channel payloads",
    "SendTimeout rejects movable resource channel payloads",
    "TrySendStatus rejects authority Token channel payloads",
    "SendTimeoutStatus rejects authority Token channel payloads",
    "Cancel rejects movable resource Future payloads",
    "Cancel rejects anchored slot-handle Future payloads",
    "Cancel rejects boundary-value Future payloads",
    "Cancel rejects authority Token Future payloads",
    "ChannelClose(Channel<Int>) returns Void",
    "ChannelClose rejects movable resource channel payloads",
    "ChannelClose rejects authority Token channel payloads",
    "function body summary records param boundary modes",
    "function call propagates callee body summary",
    "direct function call records callable declaration boundary summary",
    "method call records callable declaration body summary",
    "lambda body summary stays on lambda type",
    "lambda body summary does not leak to enclosing function",
    "lambda call propagates lambda body summary",
]:
    if term not in semantic_tests:
        raise SystemExit(f"semantic regression must cover {term}")

if 'parser_consume(parser, TOKEN_ASSIGN, "Expected \'=\' in let declaration")' not in parser:
    raise SystemExit("parser must keep local let declarations initialized")

for term in [
    "PGY_CODE_SEM_UNINIT_LOCAL",
    "PGY_CAUSE_UNINIT_LOCAL",
    "PGY_FIX_INITIALIZE_AT_BINDING",
    "function-body lets must be initialized at the binding site",
]:
    if term not in let_checker:
        raise SystemExit(f"semantic let checker is missing uninit-local guard {term}")

print("cfg body dataflow docs: ok")
PY

DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
TMP_PGY="${TMP_BASE%/}/pgy-PergyraLang-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
elif [[ -x "$DEFAULT_PGY" ]]; then
    PGY="$DEFAULT_PGY"
elif [[ -x "$TMP_PGY" ]]; then
    PGY="$TMP_PGY"
else
    PGY="$DEFAULT_PGY"
fi

EXAMPLE="${1:-$ROOT_DIR/examples/logistics_intent_probe/main.pgy}"
WORK_BASE="$ROOT_DIR/.tmp/cfg-body-dataflow"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

to_native_path_for_pgy() {
    local path="$1"
    if [[ "$PGY" != *.exe ]]; then
        printf '%s\n' "$path"
        return 0
    fi
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$path"
        return 0
    fi
    if [[ "$path" =~ ^/mnt/([A-Za-z])/(.*)$ ]]; then
        local drive="${BASH_REMATCH[1]}"
        local rest="${BASH_REMATCH[2]//\//\\}"
        printf '%s:\\%s\n' "${drive^^}" "$rest"
        return 0
    fi
    if [[ "$path" =~ ^/([A-Za-z])/(.*)$ ]]; then
        local drive="${BASH_REMATCH[1]}"
        local rest="${BASH_REMATCH[2]//\//\\}"
        printf '%s:\\%s\n' "${drive^^}" "$rest"
        return 0
    fi
    printf '%s\n' "$path"
}

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

if [[ ! -f "$EXAMPLE" ]]; then
    echo "missing example source: $EXAMPLE" >&2
    exit 1
fi
EXAMPLE_FOR_PGY="$(to_native_path_for_pgy "$EXAMPLE")"

HIR_CFG_OUT="$WORK_DIR/hir_cfg.txt"
HIR_DOM_OUT="$WORK_DIR/hir_dom.txt"
RIR_OUT="$WORK_DIR/rir.txt"
MIR_OUT="$WORK_DIR/mir.txt"

"$PGY" "$EXAMPLE_FOR_PGY" --hir-cfg > "$HIR_CFG_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --hir-dom > "$HIR_DOM_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --rir > "$RIR_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --mir > "$MIR_OUT"

grep -Fq "HIR cfg view" "$HIR_CFG_OUT"
grep -Fq "function MergeRouteScore" "$HIR_CFG_OUT"
grep -Fq "blocks=6" "$HIR_CFG_OUT"
grep -Fq "blocks-with-phi=2" "$HIR_CFG_OUT"
grep -Fq "succ=TF" "$HIR_CFG_OUT"

grep -Fq "HIR dom view" "$HIR_DOM_OUT"
grep -Fq "idom=" "$HIR_DOM_OUT"
grep -Fq "df=" "$HIR_DOM_OUT"
grep -Fq "loop=" "$HIR_DOM_OUT"
grep -Fq "rpo=" "$HIR_DOM_OUT"

grep -Fq "flow-block[" "$RIR_OUT"
grep -Fq "join=yes" "$RIR_OUT"
grep -Fq "semantics=authority|world-handoff|invalidation|authority-loss" "$RIR_OUT"
grep -Fq "kind=ProjectionTObject state=Published" "$RIR_OUT"

grep -Fq "routine[02] function MergeRouteScore blocks=6" "$MIR_OUT"
grep -Fq "phi=2" "$MIR_OUT"
grep -Fq "value[00] score.1 slot=score" "$MIR_OUT"
grep -Fq "cleanup-block=yes rollback-block=yes invalidation-block=yes" "$MIR_OUT"
grep -Fq "cleanup-edge" "$MIR_OUT"
grep -Fq "DetachInvalidation" "$MIR_OUT"

echo "cfg-body-dataflow smoke: PASS $EXAMPLE"
