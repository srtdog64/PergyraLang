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
slot_proof_path = root / "docs" / "semantics" / "08_slot_capability_calculus.md"
checklist_path = root / "docs" / "100_beta_readiness_checklist.md"
todo_path = root / "TODO.md"
makefile_path = root / "Makefile"
board_path = root / "docs" / "70_beta_closure_master_board.md"
report_path = root / "docs" / "98_beta_closure_readiness_report.md"
flow_path = root / "src" / "semantic" / "type_checker_flow.c"
flow_effects_path = root / "src" / "semantic" / "type_checker_flow_effects.c"
flow_match_path = root / "src" / "semantic" / "type_checker_flow_match.c"
flow_resources_path = root / "src" / "semantic" / "type_checker_flow_resources.h"
flow_loops_path = root / "src" / "semantic" / "type_checker_flow_loops.h"
flow_parallel_path = root / "src" / "semantic" / "type_checker_flow_parallel.h"
mir_cleanup_path = root / "src" / "compiler" / "mir_cleanup.c"
mir_cfg_contract_pin_path = root / "src" / "compiler" / "mir_cfg_contract_pin.h"
mir_cfg_contract_control_path = root / "src" / "compiler" / "mir_cfg_contract_control.h"
mir_cfg_contract_validate_path = root / "src" / "compiler" / "mir_cfg_contract_validate.h"
mir_path = root / "src" / "compiler" / "mir.c"
mir_ssa_rename_path = root / "src" / "compiler" / "mir_ssa_rename.h"
mir_liveness_dce_path = root / "src" / "compiler" / "mir_liveness_dce.h"
mir_dce_path = root / "src" / "compiler" / "mir_dce.h"
mir_stmt_population_path = root / "src" / "compiler" / "mir_stmt_population.h"
hir_lower_cfg_path = root / "src" / "compiler" / "hir_lower_cfg.c"
hir_lower_intent_cfg_path = root / "src" / "compiler" / "hir_lower_intent_cfg.c"
mir_c_control_emit_path = root / "src" / "codegen" / "transpiler_mir_cfg_control_emit.h"
mir_llvm_control_emit_path = root / "src" / "codegen" / "llvm_mir_cfg_control.c"
mir_llvm_for_in_control_path = root / "src" / "codegen" / "llvm_mir_for_in_control.c"
mir_llvm_internal_api_path = root / "src" / "codegen" / "llvm_internal_api.h"
mir_tests_path = root / "src" / "test_mir.c"
async_channel_path = root / "src" / "semantic" / "type_checker_async_channel.h"
helpers_effects_path = root / "src" / "semantic" / "type_checker_helpers_effects.c"
builtins_query_channel_path = root / "src" / "semantic" / "type_checker_builtins_query_channel.c"
builtins_cancel_path = root / "src" / "semantic" / "type_checker_builtins_cancel.c"
type_system_path = root / "src" / "semantic" / "type_system.h"
type_system_impl_path = root / "src" / "semantic" / "type_system.c"
expr_path = root / "src" / "semantic" / "type_checker_expr.c"
program_path = root / "src" / "semantic" / "type_checker_func_decl.c"
diag_path = root / "src" / "semantic" / "diag_codes.h"
diag_doc_path = root / "docs" / "72_diagnostic_codes.md"
parser_path = root / "src" / "parser" / "parser.c"
let_path = root / "src" / "semantic" / "type_checker_ownership_let.c"
semantic_tests_part_a_path = root / "src" / "tests" / "semantic" / "test_semantic_misc_a_part_a.cases.h"
semantic_tests_part_b_path = root / "src" / "tests" / "semantic" / "test_semantic_misc_a_part_b.cases.h"
semantic_async_tests_part_a_path = root / "src" / "tests" / "semantic" / "test_semantic_async_part_a.cases.h"
semantic_async_tests_part_b_path = root / "src" / "tests" / "semantic" / "test_semantic_async_part_b.cases.h"
semantic_effect_tests_part_a_path = root / "src" / "tests" / "semantic" / "test_semantic_effects_part_a.cases.h"
semantic_effect_tests_part_b_path = root / "src" / "tests" / "semantic" / "test_semantic_effects_part_b.cases.h"
semantic_parallel_context_tests_path = root / "src" / "tests" / "semantic" / "test_semantic_parallel_context.cases.h"

for path in (
    doc_path,
    slot_proof_path,
    checklist_path,
    todo_path,
    makefile_path,
    board_path,
    report_path,
    flow_path,
    flow_effects_path,
    flow_match_path,
    flow_resources_path,
    flow_loops_path,
    flow_parallel_path,
    mir_cleanup_path,
    mir_cfg_contract_pin_path,
    mir_cfg_contract_control_path,
    mir_cfg_contract_validate_path,
    mir_path,
    mir_ssa_rename_path,
    mir_liveness_dce_path,
    mir_dce_path,
    mir_stmt_population_path,
    hir_lower_cfg_path,
    hir_lower_intent_cfg_path,
    mir_c_control_emit_path,
    mir_llvm_control_emit_path,
    mir_llvm_for_in_control_path,
    mir_llvm_internal_api_path,
    mir_tests_path,
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
    semantic_tests_part_a_path,
    semantic_tests_part_b_path,
    semantic_async_tests_part_a_path,
    semantic_async_tests_part_b_path,
    semantic_effect_tests_part_a_path,
    semantic_effect_tests_part_b_path,
    semantic_parallel_context_tests_path,
):
    if not path.exists():
        raise SystemExit(f"missing required cfg/body dataflow document: {path.relative_to(root)}")

doc = doc_path.read_text(encoding="utf-8")
slot_proof = slot_proof_path.read_text(encoding="utf-8")
checklist = checklist_path.read_text(encoding="utf-8")
todo = todo_path.read_text(encoding="utf-8")
makefile = makefile_path.read_text(encoding="utf-8")
board = board_path.read_text(encoding="utf-8")
report = report_path.read_text(encoding="utf-8")
flow = (
    flow_path.read_text(encoding="utf-8")
    + "\n"
    + flow_effects_path.read_text(encoding="utf-8")
    + "\n"
    + flow_match_path.read_text(encoding="utf-8")
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
mir_cleanup = mir_cleanup_path.read_text(encoding="utf-8")
mir_cfg_contract_pin = mir_cfg_contract_pin_path.read_text(encoding="utf-8")
mir_cfg_contract_control = mir_cfg_contract_control_path.read_text(encoding="utf-8")
mir_cfg_contract_validate = mir_cfg_contract_validate_path.read_text(encoding="utf-8")
mir_cfg_contract_validator = mir_cfg_contract_pin + "\n" + mir_cfg_contract_validate
mir = mir_path.read_text(encoding="utf-8")
mir_ssa_rename = mir_ssa_rename_path.read_text(encoding="utf-8")
mir_liveness_dce = mir_liveness_dce_path.read_text(encoding="utf-8")
mir_dce = mir_dce_path.read_text(encoding="utf-8")
mir_stmt_population = mir_stmt_population_path.read_text(encoding="utf-8")
mir_codegen_control = (
    mir_c_control_emit_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_control_emit_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_for_in_control_path.read_text(encoding="utf-8")
    + "\n"
    + mir_llvm_internal_api_path.read_text(encoding="utf-8")
)
mir_tests = mir_tests_path.read_text(encoding="utf-8")
program = program_path.read_text(encoding="utf-8")
diag = diag_path.read_text(encoding="utf-8")
diag_doc = diag_doc_path.read_text(encoding="utf-8")
parser = parser_path.read_text(encoding="utf-8")
let_checker = let_path.read_text(encoding="utf-8")
semantic_tests = (
    semantic_tests_part_a_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_tests_part_b_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_async_tests_part_a_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_async_tests_part_b_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_effect_tests_part_a_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_effect_tests_part_b_path.read_text(encoding="utf-8")
    + "\n"
    + semantic_parallel_context_tests_path.read_text(encoding="utf-8")
)

required_doc_terms = [
    "HIR has function CFG v0",
    "RIR carries flow-block summaries",
    "MIR has routine/block/instruction/cleanup blocks",
    "MIR cleanup consumes RIR flow/fact/semantic summaries",
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
    "Slot Borrow-Safety Bridge Facts",
    "NoEscape(view, region)",
    "NoSuspend(view, region)",
    "WriteExclusive(slot, region)",
    "DropOnce(owner, all_cfg_exits)",
    "ReleaseAfterUnpin(slot, all_cfg_exits)",
    "NoUnsupportedTokenTransport(token, boundary)",
    "PGY_SEM_PIN_ESCAPE",
    "PGY_SEM_PIN_AWAIT_BOUNDARY",
    "PGY_SEM_PIN_PARALLEL_CONFLICT",
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
if "Bridge Obligation: Borrow-Checker-Equivalent Safety" not in slot_proof:
    raise SystemExit("slot proof pack must name the borrow-checker-equivalent bridge")
for term in [
    "NoEscape(view, region)",
    "NoSuspend(view, region)",
    "WriteExclusive(slot, region)",
    "DropOnce(owner, all_cfg_exits)",
    "ReleaseAfterUnpin(slot, all_cfg_exits)",
    "NoUnsupportedTokenTransport(token, boundary)",
]:
    if term not in slot_proof:
        raise SystemExit(f"slot proof pack missing CFG bridge fact {term}")

required_mir_cleanup_terms = [
    "mir_rir_scope_requires_rollback",
    "mir_rir_scope_requires_invalidation",
    "RIR_FLOW_INVALIDATION",
    "RIR_FLOW_WORLD_HANDOFF",
    "RIR_FLOW_PROJECTION_INVALIDATION",
    "rir_scope->flow_blocks",
    "RIR_STATE_HANDOFF_PENDING",
    "RIR_STATE_AUTHORITY_LOST",
    "RIR_RESOURCE_WORLD_HANDLE",
]
missing_mir_cleanup = [term for term in required_mir_cleanup_terms if term not in mir_cleanup]
if missing_mir_cleanup:
    raise SystemExit(
        "MIR cleanup must consume RIR flow/fact summaries: "
        + ", ".join(missing_mir_cleanup)
    )
for forbidden in [
    "mir_intent_ast_needs_invalidation",
    "data.intent_step.using_expr",
    "data.intent_step.transfer_from_alias",
    "data.intent_step.transfer_to_alias",
]:
    if forbidden in mir_cleanup:
        raise SystemExit(
            "MIR cleanup reintroduced AST-carried intent invalidation fallback: "
            + forbidden
        )

required_mir_cleanup_validator_terms = [
    "mir_block_has_pin_cleanup_edge",
    "mir_stmt_ast_is_cfg_owned_control",
    "pin-unpin-cleanup-edge",
    "incomplete loop-init fact",
    "incomplete loop-branch fact",
    "pin-region block[%zu] missing pin-unpin cleanup fact",
    "cleanup block[%zu] must not have normal CFG successors",
    "cleanup block[%zu] must not be a pin region",
    "CFG-owned control statement as fallback STMT",
]
missing_mir_cleanup_validator = [
    term for term in required_mir_cleanup_validator_terms
    if term not in mir_cfg_contract_validator
]
if missing_mir_cleanup_validator:
    raise SystemExit(
        "MIR cleanup validator must reject pin regions without unpin facts: "
        + ", ".join(missing_mir_cleanup_validator)
    )

if "AST_PARALLEL_BLOCK" in mir_cfg_contract_control:
    raise SystemExit(
        "parallel blocks are not CFG-owned until HIR/MIR has real parallel CFG lowering"
    )
if "AST_PARALLEL_BLOCK" not in mir_dce:
    raise SystemExit(
        "MIR DCE must preserve parallel blocks as side-effecting statements"
    )

required_mir_codegen_control_terms = [
    "transpiler_mir_emit_for_loop_init_inst",
    "transpiler_mir_render_for_loop_condition_inst",
    "transpiler_mir_emit_for_in_body_binding",
    "_pgy_idx_%s",
    "transpiler_mir_find_loop_branch_inst",
    "llvm_mir_emit_for_loop_init(const MIRInstruction *inst",
    "llvm_mir_emit_for_loop_condition(const MIRInstruction *inst",
    "llvm_mir_emit_for_in_body_binding",
    "__pgy_idx_%s",
    "pgy_list_size_raw_export",
    "pgy_list_get_raw_export",
    "llvm_mir_find_loop_branch_inst",
]
missing_mir_codegen_control = [
    term for term in required_mir_codegen_control_terms
    if term not in mir_codegen_control
]
if missing_mir_codegen_control:
    raise SystemExit(
        "MIR C/LLVM control emitters must consume MIR loop facts: "
        + ", ".join(missing_mir_codegen_control)
    )
for forbidden in [
    "llvm_mir_emit_for_loop_init_from_ast",
    "target->source_ast",
]:
    if forbidden in mir_codegen_control:
        raise SystemExit(
            "MIR C/LLVM control emitter reintroduced AST fallback: "
            + forbidden
        )

mir_owner_limits = {
    mir_path: 600,
    mir_ssa_rename_path: 600,
    mir_liveness_dce_path: 600,
    mir_dce_path: 600,
    mir_stmt_population_path: 600,
}
for path, limit in mir_owner_limits.items():
    loc = len(path.read_text(encoding="utf-8").splitlines())
    if loc > limit:
        raise SystemExit(
            f"MIR CFG/body owner {path.relative_to(root)} is {loc} LOC; "
            f"split-review limit is {limit}"
        )

required_mir_owner_terms = {
    "src/compiler/mir.c": [
        "#include \"mir_ssa_rename.h\"",
        "#include \"mir_liveness_dce.h\"",
        "#include \"mir_stmt_population.h\"",
        "mir_build_blocks_from_hir",
        "mir_populate_instructions",
    ],
    "src/compiler/mir_ssa_rename.h": [
        "mir_apply_ssa_rename",
        "mir_populate_use_edges",
        "mir_collect_ssa_names",
    ],
    "src/compiler/mir_liveness_dce.h": [
        "mir_compute_liveness",
        "mir_build_value_summaries",
        "#include \"mir_dce.h\"",
    ],
    "src/compiler/mir_dce.h": [
        "mir_run_dce_on_routine",
        "mir_remove_instruction",
        "mir_reset_routine_analysis",
        "#include \"mir_cfg_contract_control.h\"",
        "mir_stmt_ast_is_cfg_owned_control(stmt)",
        "AST_PARALLEL_BLOCK",
    ],
    "src/compiler/mir_stmt_population.h": [
        "mir_populate_stmt_instructions",
        "mir_stmt_def_name",
        "mir_let_decl_requires_stmt_preservation",
        "MIR_INST_LOOP_INIT",
        "mir_stmt_is_for_loop_init_payload",
        "mir_stmt_is_semantic_carrier(&old_insts[r])",
        "Intent metadata is MIR semantic inventory",
        "#include \"mir_cfg_contract_control.h\"",
        "mir_stmt_ast_is_cfg_owned_control(stmt)",
    ],
    "src/compiler/mir_cfg_contract_control.h": [
        "PERGYRA_MIR_CFG_CONTRACT_CONTROL_H",
        "mir_stmt_ast_is_cfg_owned_control",
        "AST_WITH_STMT",
        "AST_UNSAFE_BLOCK",
        "AST_DEFER_STMT",
        "AST_FOR_LOOP",
        "AST_SELECT_STMT",
        "AST_MATCH_STMT",
        "AST_BREAK",
        "AST_CONTINUE",
    ],
}
mir_owner_text = {
    "src/compiler/mir.c": mir,
    "src/compiler/mir_ssa_rename.h": mir_ssa_rename,
    "src/compiler/mir_liveness_dce.h": mir_liveness_dce,
    "src/compiler/mir_dce.h": mir_dce,
    "src/compiler/mir_stmt_population.h": mir_stmt_population,
    "src/compiler/mir_cfg_contract_control.h": mir_cfg_contract_control,
}
for owner, terms in required_mir_owner_terms.items():
    text = mir_owner_text[owner]
    missing_terms = [term for term in terms if term not in text]
    if missing_terms:
        raise SystemExit(
            "MIR CFG/body owner split lost required terms in "
            + owner
            + ": "
            + ", ".join(missing_terms)
        )

required_flow_terms = [
    "FLOW_FALLTHROUGH",
    "FLOW_RETURN",
    "type_check_if_stmt_flow",
    "type_check_match_stmt_flow",
    "semantic_check_body_flow",
    "flow_record_statement_result",
    "flow_has_fallthrough",
    "flow_terminating_flags",
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

hir_routines = program_path.parent.parent / "compiler" / "hir_routines.c"
hir_routines_text = hir_routines.read_text(encoding="utf-8")
for term in [
    "hir_validate_cfg_shape",
    "hir_validate_cfg_predecessors",
    "HIR_BLOCK_FALLTHROUGH",
    "hir_cfg_successor_in_range",
    "hir_cfg_block_targets",
    "hir_cfg_predecessors_contain",
    "return_block_count",
    "normal_exit_block_count",
]:
    if term not in hir_routines_text:
        hir_header = root / "src" / "compiler" / "hir.h"
        hir_cfg = root / "src" / "compiler" / "hir_cfg.c"
        hir_cfg_phi = root / "src" / "compiler" / "hir_cfg_phi.c"
        hir_public = root / "src" / "compiler" / "hir_public.c"
        joined = (
            hir_header.read_text(encoding="utf-8")
            + "\n"
            + hir_cfg.read_text(encoding="utf-8")
            + "\n"
            + hir_cfg_phi.read_text(encoding="utf-8")
            + "\n"
            + hir_public.read_text(encoding="utf-8")
        )
        if term not in joined:
            raise SystemExit(f"HIR CFG validation/summary gate missing {term}")

hir_cfg_path = root / "src" / "compiler" / "hir_cfg.c"
hir_cfg_phi_path = root / "src" / "compiler" / "hir_cfg_phi.c"
hir_cfg_internal_path = root / "src" / "compiler" / "hir_cfg_internal.h"
for path in (
    hir_cfg_path,
    hir_cfg_phi_path,
    hir_cfg_internal_path,
    hir_lower_cfg_path,
    hir_lower_intent_cfg_path,
):
    if not path.exists():
        raise SystemExit(f"missing HIR CFG owner file: {path.relative_to(root)}")

hir_cfg_text = hir_cfg_path.read_text(encoding="utf-8")
hir_cfg_phi_text = hir_cfg_phi_path.read_text(encoding="utf-8")
hir_lower_cfg_text = hir_lower_cfg_path.read_text(encoding="utf-8")
hir_lower_intent_cfg_text = hir_lower_intent_cfg_path.read_text(encoding="utf-8")
for term in [
    "hir_compute_cfg_dominance",
    "hir_compute_cfg_dominance_frontier",
    "hir_compute_cfg_dom_tree",
    "hir_compute_cfg_loops",
    "hir_finalize_cfg_summary",
]:
    if term not in hir_cfg_text:
        raise SystemExit(f"HIR CFG structural owner missing {term}")
for term in [
    "hir_collect_cfg_local_defs",
    "hir_compute_cfg_phi_candidates",
    "hir_materialize_phi_nodes",
    "hir_routine_collect_ssa_names",
]:
    if term not in hir_cfg_phi_text:
        raise SystemExit(f"HIR CFG phi owner missing {term}")
for term in [
    "hir_lower_func_body_cfg",
    "hir_lower_intent_cfg",
]:
    if term not in (hir_lower_cfg_text + "\n" + hir_lower_intent_cfg_text):
        raise SystemExit(f"HIR CFG lowerer split missing {term}")
for term in [
    "intent_cfg_append_step_statements",
    "step->data.intent_step.using_expr",
    "step->data.intent_step.compensate_exprs",
]:
    if term not in hir_lower_intent_cfg_text:
        raise SystemExit(f"HIR intent CFG owner missing {term}")
for term in [
    "$(COMPILER_DIR)/hir_lower_intent_cfg.c",
    "$(BUILD_DIR)/compiler/hir_lower_intent_cfg.o",
]:
    if term not in makefile:
        raise SystemExit(f"Makefile must wire HIR intent CFG owner: {term}")

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
    "CFG spawn rejects authority Token boundary crossing",
    "CFG spawn rejects anonymous async body until capture lifetime is closed",
    "CFG parallel channel send consumes resource after join",
    "CFG parallel channel sends reject double resource consume",
    "channel send with active ReadView uses pin boundary diagnostic",
    "channel send rejects authority Token payload",
    "TryRecv rejects movable resource channel payloads",
    "TryRecv with active ReadView uses pin boundary diagnostic",
    "RecvTimeout rejects movable resource channel payloads",
    "TryRecv rejects anchored slot-handle channel payloads",
    "RecvTimeout rejects boundary-value channel payloads",
    "TryRecv rejects authority Token channel payloads",
    "SendTimeout rejects movable resource channel payloads",
    "TrySendStatus rejects authority Token channel payloads",
    "SendTimeoutStatus rejects authority Token channel payloads",
    "Cancel rejects movable resource Future payloads",
    "Cancel with active ReadView uses pin boundary diagnostic",
    "Cancel rejects anchored slot-handle Future payloads",
    "Cancel rejects boundary-value Future payloads",
    "Cancel rejects authority Token Future payloads",
    "ChannelClose(Channel<Int>) returns Void",
    "ChannelClose with active ReadView uses pin boundary diagnostic",
    "ChannelClose rejects movable resource channel payloads",
    "ChannelClose rejects authority Token channel payloads",
    "TrySend with active ReadView uses pin boundary diagnostic",
    "function body summary records param boundary modes",
    "function call propagates callee body summary",
    "direct function call records callable declaration boundary summary",
    "method call records callable declaration body summary",
    "lambda body summary stays on lambda type",
    "lambda body summary does not leak to enclosing function",
    "lambda call propagates lambda body summary",
    "ReadView return escape uses pin escape diagnostic",
    "await with active ReadView uses pin await diagnostic",
    "spawn with active ReadView uses pin boundary diagnostic",
    "async block with active ReadView uses pin boundary diagnostic",
    "parallel with active ReadView uses pin conflict diagnostic",
    "ViewRead inside parallel task is rejected by pin conflict diagnostic",
    "ViewRead rejects QubitSlot with pin qubit diagnostic",
    "WriteView requires exclusive slot view access",
    "ReadView after WriteView is rejected by exclusive view gate",
    "multiple ReadView bindings are accepted",
]:
    if term not in semantic_tests:
        raise SystemExit(f"semantic regression must cover {term}")

for term in [
    "MIR lowers for-loop init as loop-init instead of fallback statement",
    "MIR lowers for-in init as loop-init instead of fallback statement",
    "MIR validator rejects CFG-owned control fallback statements",
    "MIR keeps pin cleanup fact across early return",
    "MIR keeps pin cleanup fact across branch returns",
    "MIR keeps pin cleanup fact across loop break and continue",
    "source_terminator_kind == HIR_BLOCK_RETURN",
    "source_terminator_kind != HIR_BLOCK_GOTO",
    "routine_has_complete_loop_init_for",
    "routine_has_complete_loop_branch_for",
]:
    if term not in mir_tests:
        raise SystemExit(f"MIR regression must cover {term}")

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
PARALLEL_SELECT="$WORK_DIR/parallel_select.pgy"
PARALLEL_SELECT_AST="$WORK_DIR/parallel_select_ast.txt"
PARALLEL_SELECT_MIR="$WORK_DIR/parallel_select_mir.txt"

"$PGY" "$EXAMPLE_FOR_PGY" --hir-cfg > "$HIR_CFG_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --hir-dom > "$HIR_DOM_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --rir > "$RIR_OUT"
"$PGY" "$EXAMPLE_FOR_PGY" --mir > "$MIR_OUT"

cat > "$PARALLEL_SELECT" <<'EOF'
func Main() -> Void {
    let ch: Channel<Int> = Channel(4);
    parallel {
        ch <- 7;
    }
    select {
        case v = <-ch:
            Log(v);
        default:
            Log(0);
    }
}
EOF
PARALLEL_SELECT_FOR_PGY="$(to_native_path_for_pgy "$PARALLEL_SELECT")"
"$PGY" "$PARALLEL_SELECT_FOR_PGY" --ast > "$PARALLEL_SELECT_AST"
"$PGY" "$PARALLEL_SELECT_FOR_PGY" --mir > "$PARALLEL_SELECT_MIR"

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

grep -Fq "Parallel:" "$PARALLEL_SELECT_AST"
grep -Fq "ChannelSend: ch <- 7" "$PARALLEL_SELECT_AST"
grep -Fq "inst[01] stmt" "$PARALLEL_SELECT_MIR"
grep -Fq "ast-type=9" "$PARALLEL_SELECT_MIR"

echo "cfg-body-dataflow smoke: PASS $EXAMPLE"
