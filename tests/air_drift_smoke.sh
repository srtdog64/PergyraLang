#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/beta_checklist_shards.sh"
PYTHON_BIN="${PYTHON_BIN:-}"
AIR_CHECK_DONE=0

require_literal() {
    local rel="$1"
    local term="$2"
    if [[ "$rel" == "docs/100_beta_readiness_checklist.md" ]]; then
        pgy_beta_checklist_contains "$term" || {
            echo "AIR drift literal fallback missing term in $rel shards: $term" >&2
            exit 1
        }
        return 0
    fi
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || {
        echo "AIR drift literal fallback missing term in $rel: $term" >&2
        exit 1
    }
}

run_literal_air_drift_smoke() {
    local required_files=(
        "docs/104_air_compiler_architecture.md"
        "docs/100_beta_readiness_checklist.md"
        "TODO.md"
        "Makefile"
        "docs/semantics/07_air_abstraction_safety.md"
        "src/compiler/air.h"
        "src/compiler/air.c"
        "src/compiler/air_drift.c"
        "src/compiler/air_boundary.c"
        "src/compiler/air_boundary_walk.c"
        "src/compiler/air_dump.c"
        "src/compiler/air_dump_json.c"
        "src/compiler/air_vocabulary.c"
        "src/compiler/air_boundary_evidence_policy.c"
        "src/compiler/air_evidence_node.c"
        "src/compiler/air_evidence_mir.c"
        "src/compiler/air_evidence_mir_facts.c"
        "src/compiler/air_evidence_mir_pin.c"
        "src/compiler/air_evidence_hir.c"
        "src/compiler/air_evidence_ast.c"
        "src/compiler/air_evidence_rir.c"
        "src/compiler/air_evidence_rir_match.c"
        "src/compiler/air_evidence_rir_propagation.c"
        "src/compiler/air_evidence_rir_boundary.c"
        "src/compiler/air_validate_global_evidence.c"
        "src/compiler/air_validate_summary_counters.c"
        "src/compiler/mir_cleanup_fact_names.h"
        "src/compiler/air_internal.h"
        "src/compiler/air_validate.c"
        "src/compiler/air_validate_boundary_evidence.c"
        "src/compiler/air_validate_evidence.c"
        "src/compiler/air_verify.c"
        "src/compiler/air_verify_provenance.c"
        "src/compiler/driver_app.c"
        "src/compiler/driver_diag.c"
        "src/test_air.c"
        "src/tests/air/test_air_core_part_h.cases.h"
        "src/tests/air/test_air_mir_terminator_part_h.cases.h"
        "src/tests/air/test_air_counter_strict_part_i.cases.h"
        "src/tests/air/test_air_rir_propagation_part_j.cases.h"
        "src/tests/rir/test_rir_lowering.cases.h"
        "docs/72_diagnostic_codes.md"
        "tests/air_backend_nonimpact_smoke.sh"
        "tests/diagnostics_json_smoke.sh"
    )

    for rel in "${required_files[@]}"; do
        [[ -f "$ROOT_DIR/$rel" ]] || {
            echo "missing AIR gate input: $rel" >&2
            exit 1
        }
    done

    require_literal "docs/104_air_compiler_architecture.md" "verification-only"
    require_literal "docs/104_air_compiler_architecture.md" "Strict evidence"
    require_literal "docs/100_beta_readiness_checklist.md" "## 0f. AIR Abstraction Safety Closure"
    require_literal "TODO.md" "AIR source of truth"
    require_literal "Makefile" "air-drift-test-smoke"
    require_literal "src/compiler/air.h" "AIREvidenceNode"
    require_literal "src/compiler/air_evidence_mir_pin.c" "AIR_EVIDENCE_MIR_PIN_CLEANUP"
    require_literal "src/compiler/air_evidence_mir_facts.c" "air_mir_cleanup_evidence_kind"
    require_literal "src/compiler/air_evidence_mir_facts.c" "AIR_EVIDENCE_MIR_TERMINATOR"
    require_literal "src/compiler/air_evidence_mir_facts.c" "mir_instruction_uses_select_receive_statement_emit"
    require_literal "src/semantic/semantic.c" "semantic_result_type_resolution_metadata_entries"
    require_literal "src/compiler/air_evidence_dag.c" "semantic_result_type_resolution_metadata_entries"
    require_literal "src/compiler/air_evidence_mir_facts.c" "mir_block_has_expected_cleanup_edge_fact(routine, i)"
    require_literal "src/compiler/mir_cleanup_fact_names.h" "cleanup-edge-from-rollback"
    require_literal "src/compiler/air_evidence_mir_pin.c" "slot_anchor"
    require_literal "src/compiler/air_evidence_mir_pin.c" "mir_block_find_pin_cleanup_edge_fact"
    require_literal "src/compiler/air_evidence_dag.c" "AIR_EVIDENCE_DAG_METADATA"
    require_literal "src/compiler/air_evidence_dag.c" "metadata-inventory"
    require_literal "src/compiler/air_evidence_dag.c" "AIR_EVIDENCE_DAG_GENERIC"
    require_literal "src/compiler/air_verify.c" "air_verify"
    require_literal "src/compiler/air_verify_provenance.c" "source_provenance="
    require_literal "src/compiler/air_verify_provenance.c" "who_provenance="
    require_literal "src/compiler/air_verify_provenance.c" "intent-default+transfer"
    require_literal "src/compiler/air_boundary.c" "air_boundary_declares_authority_name"
    require_literal "src/compiler/air_verify_global.c" "strict AIR requires graph-backed type evidence"
    require_literal "src/compiler/air_verify_global.c" "missing DAG evidence node"
    require_literal "src/compiler/air_verify.c" "strict AIR requires lowered boundary evidence"
    require_literal "src/compiler/air_boundary_evidence_policy.c" "kBoundaryEvidencePolicies"
    require_literal "src/compiler/air_boundary_evidence_policy.c" "air_boundary_requires_hir_routine_evidence"
    require_literal "src/compiler/air_boundary_evidence_policy.c" "air_boundary_requires_hir_evidence"
    require_literal "src/compiler/air_boundary_evidence_policy.c" "air_boundary_requires_rir_evidence"
    require_literal "src/compiler/air_boundary_evidence_policy.c" "air_boundary_requires_mir_pin_cleanup_evidence"
    require_literal "src/compiler/air_verify.c" "air_boundary_requires_hir_cfg_for_program"
    require_literal "src/compiler/air_validate_evidence.c" "air_boundary_has_evidence"
    require_literal "src/compiler/air_validate_evidence.c" "air_global_evidence_fact_count"
    require_literal "src/compiler/air_validate_evidence.c" "air_global_evidence_node_count"
    require_literal "src/compiler/air_verify_global.c" "air_global_has_evidence_kind"
    require_literal "src/compiler/air_validate.c" "AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING"
    require_literal "src/compiler/air_validate_global_evidence.c" "air_global_evidence_kind_has_validator"
    require_literal "src/compiler/air_validate_global_evidence.c" "has no global validator"
    require_literal "src/compiler/air_validate_global_evidence.c" "AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY"
    require_literal "src/compiler/air_evidence_runtime.c" "air_collect_singleton_global_evidence"
    require_literal "src/compiler/air_evidence_runtime.c" "AIR singleton global evidence has conflicting counts"
    require_literal "src/compiler/air_validate_summary_counters.c" "air_validate_summary_counters"
    require_literal "src/compiler/driver_diag.c" "air_boundary_has_evidence("
    require_literal "src/compiler/air_verify.c" "strict AIR requires body control-flow evidence"
    require_literal "src/compiler/air_verify_global.c" "strict AIR requires MIR branch/return terminator provenance"
    require_literal "src/compiler/air_verify_global.c" "strict AIR treats MIR summary counters as observability only"
    require_literal "src/compiler/air_verify.c" "strict AIR requires pin boundaries to prove all exits run unpin cleanup"
    require_literal "src/compiler/air_validate_summary_counters.c" "AIR MIR %s evidence counter does not match evidence nodes"
    require_literal "src/compiler/air_verify_global.c" "strict AIR requires observability ABI schema evidence"
    require_literal "src/compiler/air_verify_global.c" "strict AIR requires runtime frontier policy evidence"
    require_literal "src/compiler/air_boundary_walk.c" "case AST_LAMBDA_EXPR:"
    require_literal "src/compiler/air_evidence_ast.c" "case AST_LAMBDA_EXPR:"
    require_literal "src/compiler/air_internal.h" "air_next_capacity"
    require_literal "src/compiler/air_names.c" "air_next_capacity"
    require_literal "src/compiler/air_evidence_node.c" "air_next_capacity(&new_capacity"
    require_literal "src/compiler/air_evidence_node.c" "kEvidenceKindMeta"
    require_literal "src/compiler/air_evidence_node.c" "bool present"
    require_literal "src/compiler/air_evidence_node.c" "if (!kEvidenceKindMeta[kind].present)"
    require_literal "src/compiler/air_evidence_node.c" "air_evidence_kind_has_global_validator"
    require_literal "src/compiler/air_validate_global_evidence.c" "air_evidence_kind_has_global_validator"
    require_literal "src/compiler/air_evidence_node.c" "AIR evidence append requires a known evidence kind"
    require_literal "src/compiler/air_evidence_node.c" "AIR evidence append requires non-empty provider and subject provenance"
    require_literal "src/compiler/air_evidence_node.c" "AIR evidence append requires at least one fact or fallback fact"
    require_literal "src/compiler/air_evidence_node.c" "air_next_capacity(&new_capacity"
    require_literal "src/compiler/air_evidence_node.c" "node->fact_count += fact_count"
    require_literal "src/compiler/air_evidence_node.c" "air_evidence_node_kind"
    require_literal "src/compiler/air_evidence_node.c" "air_evidence_node_boundary_index_or"
    require_literal "src/compiler/air_evidence_node.c" "air_evidence_node_provider_name_or"
    require_literal "src/compiler/air_evidence_node.c" "air_evidence_node_subject_name_or"
    require_literal "src/compiler/air_evidence_node.c" "air_evidence_node_fact_count"
    require_literal "src/compiler/air_evidence_node.c" "air_evidence_node_fallback_count"
    for rel in src/compiler/air_dump.c src/compiler/air_dump_json.c; do
        for field in kind boundary_index provider_name subject_name fact_count fallback_count; do
            if grep -Fq -- "evidence->$field" "$ROOT_DIR/$rel"; then
                echo "AIR dumps must consume evidence node fields through the evidence-node owner: $rel evidence->$field" >&2
                exit 1
            fi
        done
    done
    for field in kind provider_name subject_name fact_count fallback_count; do
        if grep -Fq -- "evidence->$field" "$ROOT_DIR/src/compiler/air_validate_global_evidence.c"; then
            echo "AIR global evidence validator must consume evidence node fields through the evidence-node owner: evidence->$field" >&2
            exit 1
        fi
    done
    for field in kind boundary_index provider_name subject_name fact_count fallback_count; do
        if grep -Fq -- "evidence->$field" "$ROOT_DIR/src/compiler/air_validate_boundary_evidence.c"; then
            echo "AIR boundary evidence validator must consume evidence node fields through the evidence-node owner: evidence->$field" >&2
            exit 1
        fi
    done
    raw_evidence_hits="$(
        grep -RInE --include='air*.c' --include='air*.h' -- \
            'evidence->(kind|boundary_index|provider_name|subject_name|fact_count|fallback_count)' "$ROOT_DIR/src/compiler" \
            | grep -v 'src/compiler/air_evidence_node.c:' \
            || true
    )"
    if [[ -n "$raw_evidence_hits" ]]; then
        printf '%s\n' "$raw_evidence_hits" >&2
        echo "AIR consumers must read AIREvidenceNode fields through the evidence-node owner" >&2
        exit 1
    fi
    require_literal "src/compiler/air_validate_evidence.c" "duplicates evidence node"
    require_literal "src/compiler/driver_app.c" "air_synthesize"
    require_literal "docs/72_diagnostic_codes.md" "PGY_SEM_INTENT_BOUNDARY_DRIFT"
    require_literal "src/test_air.c" "AIR strict evidence requires MIR pin cleanup"
    require_literal "src/test_air.c" "AIR who inference does not imply authority"
    require_literal "src/tests/air/test_air_core_part_a.cases.h" "test_air_who_inference_does_not_imply_authority"
    require_literal "src/tests/air/test_air_core_part_a.cases.h" "!air->boundaries[0].authority_required"
    require_literal "src/test_air.c" "AIR strict evidence rejects observability counter only"
    require_literal "src/test_air.c" "AIR strict evidence rejects frontier counter only"
    require_literal "src/test_air.c" "AIR ignores orphan MIR cleanup root evidence"
    require_literal "src/test_air.c" "AIR verify rejects invalid drift inventory"
    require_literal "src/test_air.c" "AIR verify rejects duplicate evidence nodes"
    require_literal "src/test_air.c" "AIR append merges duplicate evidence nodes"
    require_literal "src/test_air.c" "AIR append rejects unknown evidence kind"
    require_literal "src/test_air.c" "AIR append rejects empty evidence provenance"
    require_literal "src/test_air.c" "AIR append rejects empty evidence counts"
    require_literal "src/test_air.c" "AIR collects MIR terminator evidence"
    require_literal "src/test_air.c" "AIR rejects MIR evidence without routine provider"
    require_literal "src/test_air.c" "AIR rejects malformed MIR routine inventory"
    require_literal "src/compiler/air_evidence_mir.c" "AIR MIR evidence requires routine name or owner provenance"
    require_literal "src/tests/air/test_air_mir_terminator_part_h.cases.h" "AIR MIR evidence counter has no matching evidence node"
    require_literal "src/tests/air/test_air_mir_terminator_part_h.cases.h" "MIR cleanup evidence counter does not match evidence nodes"
    require_literal "src/tests/air/test_air_mir_terminator_part_h.cases.h" "MIR terminator evidence node 0 has no terminator facts"
    require_literal "src/tests/air/test_air_mir_terminator_part_h.cases.h" "test_air_rejects_mir_evidence_without_routine_provider"
    require_literal "src/tests/air/test_air_mir_terminator_part_h.cases.h" "test_air_rejects_malformed_mir_routine_inventory"
    require_literal "src/tests/air/test_air_mir_terminator_part_h.cases.h" "AIR MIR evidence has invalid routine inventory row[0]"
    require_literal "src/tests/air/test_air_mir_terminator_part_h.cases.h" "AIR MIR input has no CFG terminator evidence"
    require_literal "src/test_air.c" "AIR parsed transfer emits zone and world boundaries"
    require_literal "src/tests/rir/test_rir_lowering.cases.h" "RIR_OP_SPAWN"
    require_literal "docs/72_diagnostic_codes.md" "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING"
    require_literal "tests/air_backend_nonimpact_smoke.sh" "PGY_AIR_STRICT_EVIDENCE=0"
    require_literal "docs/semantics/07_air_abstraction_safety.md" "## Theorem: AIR Synthesis Read-Only"
    require_literal "docs/semantics/07_air_abstraction_safety.md" "## Theorem: Codegen Non-Impact"

    echo "AIR drift smoke: ok (literal fallback)"
}

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        run_literal_air_drift_smoke
        AIR_CHECK_DONE=1
    fi
fi

if [[ "$AIR_CHECK_DONE" -eq 0 ]]; then
"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
air_path = root / "docs" / "104_air_compiler_architecture.md"
checklist_paths = [
    root / "docs" / "100_beta_readiness_checklist.md",
    root / "docs" / "100a_beta_active_status.md",
    root / "docs" / "100b_beta_p0_semantics_systems_air.md",
    root / "docs" / "100c_beta_dag_mir_abi_runtime.md",
    root / "docs" / "100d_beta_execution_log.md",
]
todo_path = root / "TODO.md"
makefile_path = root / "Makefile"
air_semantics_path = root / "docs" / "semantics" / "07_air_abstraction_safety.md"
io_boundary_builtin_path = root / "src" / "compiler" / "io_boundary_builtin.c"
compiler_header_path = root / "src" / "compiler" / "compiler.h"
driver_path = root / "src" / "compiler" / "driver_app.c"
driver_diag_path = root / "src" / "compiler" / "driver_diag.c"
pgy_driver_path = root / "src" / "pgy_driver.c"
parser_intent_path = root / "src" / "parser" / "parser_intent.c"
parser_intent_step_path = root / "src" / "parser" / "parser_intent_step.c"
dir_header_path = root / "src" / "compiler" / "dir.h"
dir_impl_path = root / "src" / "compiler" / "dir.c"
dir_collect_path = root / "src" / "compiler" / "dir_collect.c"
dir_collect_intent_path = root / "src" / "compiler" / "dir_collect_intent.c"
air_header_path = root / "src" / "compiler" / "air.h"
air_impl_path = root / "src" / "compiler" / "air.c"
air_drift_path = root / "src" / "compiler" / "air_drift.c"
air_boundary_path = root / "src" / "compiler" / "air_boundary.c"
air_boundary_walk_path = root / "src" / "compiler" / "air_boundary_walk.c"
air_dump_path = root / "src" / "compiler" / "air_dump.c"
air_dump_json_path = root / "src" / "compiler" / "air_dump_json.c"
air_vocabulary_path = root / "src" / "compiler" / "air_vocabulary.c"
air_boundary_evidence_policy_path = root / "src" / "compiler" / "air_boundary_evidence_policy.c"
air_evidence_node_path = root / "src" / "compiler" / "air_evidence_node.c"
air_evidence_path = root / "src" / "compiler" / "air_evidence_mir.c"
air_evidence_mir_facts_path = root / "src" / "compiler" / "air_evidence_mir_facts.c"
air_evidence_mir_pin_path = root / "src" / "compiler" / "air_evidence_mir_pin.c"
air_evidence_hir_path = root / "src" / "compiler" / "air_evidence_hir.c"
air_evidence_dag_path = root / "src" / "compiler" / "air_evidence_dag.c"
air_evidence_ast_path = root / "src" / "compiler" / "air_evidence_ast.c"
air_evidence_rir_path = root / "src" / "compiler" / "air_evidence_rir.c"
air_evidence_rir_match_path = root / "src" / "compiler" / "air_evidence_rir_match.c"
air_evidence_rir_propagation_path = root / "src" / "compiler" / "air_evidence_rir_propagation.c"
air_evidence_rir_boundary_path = root / "src" / "compiler" / "air_evidence_rir_boundary.c"
air_evidence_runtime_path = root / "src" / "compiler" / "air_evidence_runtime.c"
env_flags_path = root / "src" / "common" / "env_flags.c"
mir_cleanup_fact_names_path = root / "src" / "compiler" / "mir_cleanup_fact_names.h"
air_internal_path = root / "src" / "compiler" / "air_internal.h"
air_validate_path = root / "src" / "compiler" / "air_validate.c"
air_validate_boundary_evidence_path = root / "src" / "compiler" / "air_validate_boundary_evidence.c"
air_validate_evidence_path = root / "src" / "compiler" / "air_validate_evidence.c"
air_validate_boundary_summary_path = root / "src" / "compiler" / "air_validate_boundary_summary.c"
air_validate_global_evidence_path = root / "src" / "compiler" / "air_validate_global_evidence.c"
air_verify_provenance_path = root / "src" / "compiler" / "air_verify_provenance.c"
air_verify_global_path = root / "src" / "compiler" / "air_verify_global.c"
air_verify_path = root / "src" / "compiler" / "air_verify.c"
air_test_path = root / "src" / "test_air.c"
air_test_case_paths = [
    root / "src" / "tests" / "air" / "test_air_core_part_a.cases.h",
    root / "src" / "tests" / "air" / "test_air_evidence_part_b.cases.h",
    root / "src" / "tests" / "air" / "test_air_cleanup_transfer_part_c.cases.h",
    root / "src" / "tests" / "air" / "test_air_cleanup_transfer_part_d.cases.h",
    root / "src" / "tests" / "air" / "test_air_boundary_part_d.cases.h",
    root / "src" / "tests" / "air" / "test_air_boundary_part_e.cases.h",
    root / "src" / "tests" / "air" / "test_air_parsed_part_e.cases.h",
    root / "src" / "tests" / "air" / "test_air_strict_part_f.cases.h",
    root / "src" / "tests" / "air" / "test_air_observability_pin_part_g.cases.h",
    root / "src" / "tests" / "air" / "test_air_core_part_h.cases.h",
    root / "src" / "tests" / "air" / "test_air_mir_terminator_part_h.cases.h",
    root / "src" / "tests" / "air" / "test_air_counter_strict_part_i.cases.h",
    root / "src" / "tests" / "air" / "test_air_rir_propagation_part_j.cases.h",
    root / "src" / "tests" / "air" / "test_air_core_evidence_part_k.cases.h",
]
rir_test_path = root / "src" / "test_rir.c"
rir_test_case_paths = [
    root / "src" / "tests" / "rir" / "test_rir_lowering.cases.h",
]
diag_docs_path = root / "docs" / "72_diagnostic_codes.md"
air_backend_nonimpact_path = root / "tests" / "air_backend_nonimpact_smoke.sh"
diagnostics_json_path = root / "tests" / "diagnostics_json_smoke.sh"

for path in (air_path, *checklist_paths, todo_path, makefile_path, air_semantics_path, io_boundary_builtin_path, compiler_header_path, driver_path, driver_diag_path, pgy_driver_path, parser_intent_path, parser_intent_step_path, dir_header_path, dir_impl_path, dir_collect_path, dir_collect_intent_path, air_header_path, air_impl_path, air_drift_path, air_boundary_path, air_boundary_walk_path, air_dump_path, air_dump_json_path, air_vocabulary_path, air_boundary_evidence_policy_path, air_evidence_node_path, air_evidence_path, air_evidence_mir_facts_path, air_evidence_mir_pin_path, air_evidence_dag_path, air_evidence_ast_path, air_evidence_rir_path, air_evidence_rir_match_path, air_evidence_rir_propagation_path, air_evidence_rir_boundary_path, air_evidence_runtime_path, env_flags_path, mir_cleanup_fact_names_path, air_internal_path, air_validate_path, air_validate_boundary_evidence_path, air_validate_evidence_path, air_validate_boundary_summary_path, air_validate_global_evidence_path, air_verify_provenance_path, air_verify_global_path, air_verify_path, air_test_path, *air_test_case_paths, rir_test_path, *rir_test_case_paths, diag_docs_path, air_backend_nonimpact_path, diagnostics_json_path):
    if not path.exists():
        raise SystemExit(f"missing AIR gate input: {path.relative_to(root)}")

air = air_path.read_text(encoding="utf-8")
checklist = "\n".join(path.read_text(encoding="utf-8")
                      for path in checklist_paths)
todo = todo_path.read_text(encoding="utf-8")
makefile = makefile_path.read_text(encoding="utf-8")
air_semantics = air_semantics_path.read_text(encoding="utf-8")
compiler_header = compiler_header_path.read_text(encoding="utf-8")
driver = "\n".join([
    driver_path.read_text(encoding="utf-8"),
    driver_diag_path.read_text(encoding="utf-8"),
    pgy_driver_path.read_text(encoding="utf-8"),
])
parser_intent = "\n".join([
    parser_intent_path.read_text(encoding="utf-8"),
    parser_intent_step_path.read_text(encoding="utf-8"),
])
dir_header = dir_header_path.read_text(encoding="utf-8")
dir_impl = "\n".join([
    dir_impl_path.read_text(encoding="utf-8"),
    dir_collect_path.read_text(encoding="utf-8"),
    dir_collect_intent_path.read_text(encoding="utf-8"),
])
air_header = air_header_path.read_text(encoding="utf-8")
air_boundary_walk = air_boundary_walk_path.read_text(encoding="utf-8")
air_evidence = "\n".join([
    air_evidence_path.read_text(encoding="utf-8"),
    air_evidence_mir_facts_path.read_text(encoding="utf-8"),
    air_evidence_mir_pin_path.read_text(encoding="utf-8"),
    air_evidence_hir_path.read_text(encoding="utf-8"),
    air_evidence_dag_path.read_text(encoding="utf-8"),
    air_evidence_ast_path.read_text(encoding="utf-8"),
    air_evidence_rir_match_path.read_text(encoding="utf-8"),
    air_evidence_rir_propagation_path.read_text(encoding="utf-8"),
    air_evidence_rir_boundary_path.read_text(encoding="utf-8"),
    air_evidence_runtime_path.read_text(encoding="utf-8"),
    mir_cleanup_fact_names_path.read_text(encoding="utf-8"),
])
air_evidence_dag_text = air_evidence_dag_path.read_text(encoding="utf-8")
air_impl = "\n".join([
    air_impl_path.read_text(encoding="utf-8"),
    air_drift_path.read_text(encoding="utf-8"),
    io_boundary_builtin_path.read_text(encoding="utf-8"),
    air_boundary_path.read_text(encoding="utf-8"),
    air_boundary_walk_path.read_text(encoding="utf-8"),
    air_dump_path.read_text(encoding="utf-8"),
    air_dump_json_path.read_text(encoding="utf-8"),
    air_vocabulary_path.read_text(encoding="utf-8"),
    air_boundary_evidence_policy_path.read_text(encoding="utf-8"),
    air_evidence_node_path.read_text(encoding="utf-8"),
    air_evidence_path.read_text(encoding="utf-8"),
    air_evidence_mir_facts_path.read_text(encoding="utf-8"),
    air_evidence_mir_pin_path.read_text(encoding="utf-8"),
    air_evidence_hir_path.read_text(encoding="utf-8"),
    air_evidence_dag_path.read_text(encoding="utf-8"),
    air_evidence_ast_path.read_text(encoding="utf-8"),
    air_evidence_rir_path.read_text(encoding="utf-8"),
    air_evidence_rir_match_path.read_text(encoding="utf-8"),
    air_evidence_rir_propagation_path.read_text(encoding="utf-8"),
    air_evidence_rir_boundary_path.read_text(encoding="utf-8"),
    air_evidence_runtime_path.read_text(encoding="utf-8"),
    env_flags_path.read_text(encoding="utf-8"),
    mir_cleanup_fact_names_path.read_text(encoding="utf-8"),
    air_internal_path.read_text(encoding="utf-8"),
    air_validate_path.read_text(encoding="utf-8"),
    air_validate_boundary_evidence_path.read_text(encoding="utf-8"),
    air_validate_evidence_path.read_text(encoding="utf-8"),
    air_validate_boundary_summary_path.read_text(encoding="utf-8"),
    air_verify_provenance_path.read_text(encoding="utf-8"),
    air_verify_global_path.read_text(encoding="utf-8"),
    air_verify_path.read_text(encoding="utf-8"),
])
air_global = air_validate_global_evidence_path.read_text(encoding="utf-8")
air_verify_global_text = air_verify_global_path.read_text(encoding="utf-8", errors="ignore")
air_test = "\n".join(
    [air_test_path.read_text(encoding="utf-8")]
    + [path.read_text(encoding="utf-8") for path in air_test_case_paths]
)
rir_test = "\n".join(
    [rir_test_path.read_text(encoding="utf-8")]
    + [path.read_text(encoding="utf-8") for path in rir_test_case_paths]
)
diag_docs = diag_docs_path.read_text(encoding="utf-8")
air_backend_nonimpact = air_backend_nonimpact_path.read_text(encoding="utf-8")
diagnostics_json = diagnostics_json_path.read_text(encoding="utf-8")

air_boundary_text = air_boundary_path.read_text(encoding="utf-8")
if not re.search(
    r"case\s+AIR_BOUNDARY_WORLD\s*:\s*return\s+AIR_SYNC_ASYNC\s*;",
    air_boundary_text,
):
    raise SystemExit(
        "AIR world boundary sync classifier must match validator: "
        "AIR_BOUNDARY_WORLD => AIR_SYNC_ASYNC"
    )

required_air_terms = [
    "AIR (Abstraction Intent Representation)",
    "verification-only synthesis IR",
    "AST → HIR → DIR → RIR → MIR → C / LLVM",
    "AIR 는 이 codegen path 옆에 붙는 verification-only synthesis IR",
    "Intent Node",
    "Boundary Node",
    "Drift Detection",
    "PGY_SEM_INTENT_BOUNDARY_DRIFT",
    "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING",
    "PGY_AIR_STRICT_EVIDENCE",
    "AIR 는 codegen IR 이 아니다",
    "AIR 는 ownership / borrow 검사의 home 이 아니다",
    "AIR 는 type 검사의 home 이 아니다",
    "AIR 는 effect propagation 자체의 home 이 아니다",
    "AIR 는 새로운 keyword / syntax 를 추가하지 않는다",
    "Phase 1 (베타 closure 안)",
    "make air-drift-test-smoke",
    "make air-json-schema-test-smoke",
    "make air-backend-nonimpact-test-smoke",
    "pgy --air <source.pgy>",
    "AIRProgram intents=... boundaries=... evidence_nodes=... drifts=...",
    "AIREvidenceNode",
    "CompilerIRBundle",
]
missing_air = [term for term in required_air_terms if term not in air]
if missing_air:
    raise SystemExit("AIR architecture doc missing term(s): " + ", ".join(missing_air))

required_checklist_terms = [
    "strict beta readiness is now about 75%",
    "## 0f. AIR Abstraction Safety Closure",
    "Source of truth: `docs/104_air_compiler_architecture.md`",
    "Status: `BLOCKER`",
    "AIR 는 codegen path 위가 아니라 옆에 위치하는",
    "make air-drift-test-smoke",
    "make air-backend-nonimpact-test-smoke",
    "PGY_SEM_INTENT_BOUNDARY_DRIFT",
    "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING",
    "Strict evidence is now the default AIR validation mode",
    "PGY_AIR_STRICT_EVIDENCE=0",
]
missing_checklist = [term for term in required_checklist_terms if term not in checklist]
if missing_checklist:
    raise SystemExit("beta checklist missing AIR term(s): " + ", ".join(missing_checklist))

required_todo_terms = [
    "strict beta readiness is now about 75%",
    "AIR abstraction safety는 Phase 1 데이터 구조 / synthesis / drift checker baseline",
    "strict evidence는 기본값으로 승격됐다",
    "PGY_AIR_STRICT_EVIDENCE=0",
    "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING",
    "docs/104_air_compiler_architecture.md",
    "make air-drift-test-smoke",
    "make air-json-schema-test-smoke",
    "air-backend-nonimpact-test-smoke",
]
missing_todo = [term for term in required_todo_terms if term not in todo]
if missing_todo:
    raise SystemExit("TODO missing AIR beta gate term(s): " + ", ".join(missing_todo))

for term in [
    "TEST_AIR_SRC",
    "test-air:",
    "air-drift-test-smoke:",
    "air-json-schema-test-smoke:",
    "air-backend-nonimpact-test-smoke:",
    "air-backend-nonimpact-full-test-smoke:",
    "air-strict-backend-compare-test-smoke:",
    "$(COMPILER_DIR)/io_boundary_builtin.c",
    "$(BUILD_DIR)/compiler/io_boundary_builtin.o",
    "$(COMPILER_DIR)/air_verify.c",
    "$(COMPILER_DIR)/air_boundary_walk.c",
    "$(BUILD_DIR)/compiler/air_verify.o",
    "$(BUILD_DIR)/compiler/air_boundary_walk.o",
    "$(MAKE) test-air",
    "tests/air_drift_smoke.sh",
    "tests/air_json_schema_smoke.sh",
    "tests/air_backend_nonimpact_smoke.sh",
    "air-drift-test-smoke",
    "air-json-schema-test-smoke",
    "air-backend-nonimpact-test-smoke",
    "air-backend-nonimpact-full-test-smoke",
    "air-strict-backend-compare-test-smoke",
]:
    if term not in makefile:
        raise SystemExit(f"Makefile missing AIR smoke wiring: {term}")

required_header_terms = [
    "AIRProgram",
    "AIRIntentNode",
    "AIRBoundaryNode",
    "AIR_DRIFT_SYNC_ASYNC_CONFLICT",
    "AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING",
    "strict_evidence",
    "has_hir_routine_evidence",
    "has_hir_cfg_evidence",
    "has_rir_boundary_evidence",
    "has_rir_authority_evidence",
    "hir_routine_evidence_name",
    "rir_boundary_evidence_scope",
    "rir_authority_evidence_name",
    "authority_names",
    "authority_name_count",
    "has_hir_input",
    "has_rir_input",
    "hir_routine_evidence_count",
    "hir_cfg_evidence_count",
    "rir_boundary_evidence_count",
    "rir_authority_evidence_count",
    "air_synthesize",
    "air_verify",
    "air_check_drift",
    "air_boundary_requires_hir_evidence",
    "air_boundary_requires_rir_evidence",
    "air_boundary_evidence_node",
    "air_boundary_evidence_provider",
    "air_boundary_evidence_subject",
]
missing_header = [term for term in required_header_terms if term not in air_header]
if missing_header:
    raise SystemExit("AIR header missing term(s): " + ", ".join(missing_header))

required_impl_terms = [
    "air_synthesize",
    "air_validate",
    "air_verify",
    "air_check_drift",
    "PGY_CODE_AIR_INVARIANT_INVALID",
    "PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT",
    "PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING",
    "PGY_AIR_STRICT_EVIDENCE",
    "air_strict_evidence_enabled",
    "pgy_env_value_is_false(value)",
    "FALSE",
    "NO",
    "air_mark_hir_input(air)",
    "air_mark_rir_input(air)",
    "air_sync_conflicts",
    "air_collect_hir_evidence",
    "air_collect_rir_evidence",
    "air_collect_runtime_frontier_policy_evidence",
    "PGY_FRONTIER_POLICY_SCHEMA",
    "air_evidence_inventory_is_authoritative(air)",
    "air_boundary_has_evidence(air, i, AIR_EVIDENCE_RIR_BOUNDARY)",
    "air_assign_first_owned_name",
    "air_boundary_sync_shape_valid",
    "air_boundary_requires_hir_evidence",
    "AST_LET_DECL",
    "AST_LET_DESTRUCTURE",
    "AST_EVENT_SUBSCRIBE",
    "event-subscribe",
    "event-unsubscribe",
    "AST_PARTY_INSTANCE",
    "AST_WORLD_ZONE",
    "AST_DOMAIN_SLOT",
    "AIR implementation boundary has no matching HIR CFG evidence",
    "AIR boundary has no matching HIR routine evidence",
    "strict AIR requires lowered boundary evidence",
    "strict AIR requires body control-flow evidence",
    "strict AIR requires MIR branch/return terminator provenance",
    "strict AIR requires pin boundaries to prove all exits run unpin cleanup",
    "air_drift_kind_valid",
    "air_name_is_empty",
    "air_collect_hir_evidence(air, hir, error_message)",
    "air_collect_rir_evidence(air, rir, error_message)",
    "air_strdup_owned",
    "air_clear_drifts",
    "air_boundary_kind_from_ast",
    "air_count_step_expr_boundaries",
    "air_append_step_expr_boundaries",
    "air_boundary_sync_from_kind",
    "World handoff is an async abstraction boundary by AIR contract.",
    "AST_AWAIT_EXPR",
    "AST_TASK_GROUP",
    "task-group",
    "RIR_OP_AWAIT_REMOTE",
    "RIR_OP_SPAWN",
    "RIR_OP_ASYNC",
    "RIR_OP_PARALLEL",
    "RIR_OP_TASK_GROUP",
    "RIR_OP_IO",
    "RIR_OP_CHANNEL_SEND",
    "RIR_OP_CHANNEL_RECV",
    "RIR_OP_CHANNEL_SELECT",
    "air_call_is_io_boundary",
    "air_format_authority_names",
    "air_format_boundary_provenance",
    "source_provenance=",
    "who_provenance=",
    "intent-default+transfer",
    "air_boundary_declares_authority_name",
    "air_boundary_missing_authority_evidence",
    "air_boundary_has_evidence_kind_subject",
    "air_ast_contains_node",
    "air_rir_op_matches_boundary_ast",
    "air_rir_io_op_matches_boundary",
    "air_rir_channel_op_matches_boundary",
    "air_rir_parallel_op_matches_boundary",
    "air_hir_routine_matches_boundary",
    "air_hir_cfg_contains_boundary_ast",
    "air_rir_scope_matches_boundary",
    "air_mir_cleanup_root_is_valid",
    "air_mir_pin_block_has_cleanup_successor",
    "air_mir_routine_cleanup_fact_count",
    "cleanup-edge-from-rollback",
    "cleanup-edge-from-invalidation",
    "slot_anchor",
    "mir_block_find_pin_cleanup_edge_fact",
    "block->is_reachable",
    "block->cleanup_succ != routine->cleanup_block",
    "AIR synthesis count mismatch",
    "intent_index != intent_node_count",
    "boundary_index != boundary_node_count",
]
missing_impl = [term for term in required_impl_terms if term not in air_impl]
if missing_impl:
    raise SystemExit("AIR implementation missing term(s): " + ", ".join(missing_impl))

shared_walker_terms = [
    "AST_LET_DECL",
    "AST_LET_DESTRUCTURE",
    "AST_EVENT_SUBSCRIBE",
    "AST_EVENT_UNSUBSCRIBE",
    "AST_PARTY_SHARED",
    "AST_PARTY_INSTANCE",
    "AST_WORLD_SYSTEMIC",
    "AST_WORLD_ZONE",
    "AST_DOMAIN_SLOT",
]
missing_boundary_walk_terms = [
    term for term in shared_walker_terms
    if term not in air_boundary_walk
]
if missing_boundary_walk_terms:
    raise SystemExit(
        "AIR boundary walker missing single traversal payload term(s): "
        + ", ".join(missing_boundary_walk_terms)
    )
for forbidden in [
    "air_count_expr_boundaries",
    "air_append_expr_boundaries",
]:
    if forbidden in air_boundary_walk:
        raise SystemExit(
            "AIR boundary walker reintroduced split count/append traversal: "
            + forbidden
        )
for required in [
    "AIRBoundaryWalkCtx",
    "air_walk_step_expr_boundaries",
    "air_walk_expr_boundaries",
    "ctx->append",
]:
    if required not in air_boundary_walk:
        raise SystemExit(
            "AIR boundary walker no longer uses a single count/append traversal: "
            + required
        )
missing_evidence_terms = [term for term in shared_walker_terms if term not in air_evidence]
if missing_evidence_terms:
    raise SystemExit(
        "AIR evidence containment missing mirrored payload term(s): "
        + ", ".join(missing_evidence_terms)
    )

for required in [
    "AIRAstBoundaryRule",
    "kAstBoundaryRules",
    "air_ast_boundary_rule_for_node",
    "source_name",
]:
    if required not in air_impl:
        raise SystemExit(
            "AIR boundary taxonomy must stay table-backed: " + required
        )

required_driver_terms = [
    "air_synthesize(hir, dir, rir",
    "driver_emit_air_drift_fail",
    "driver_format_air_authority_names",
    "air_boundary_authority_name_count(boundary)",
    "air_boundary_authority_name_at(boundary, i)",
    "driver_format_air_evidence_summary",
    "PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT",
    "PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING",
    "PGY_CODE_AIR_INVARIANT_INVALID",
    "PGY_CAUSE_AIR_INVARIANT_INVALID",
    "PGY_FIX_REPORT_COMPILER_BUG",
    "PGY_CAUSE_INTENT_BOUNDARY_DRIFT",
    "PGY_CAUSE_INTENT_BOUNDARY_EVIDENCE",
    "PGY_FIX_ALIGN_INTENT_BOUNDARY_SYNC",
    "PGY_FIX_ALIGN_INTENT_BOUNDARY_EVIDENCE",
    "air_boundary_requires_hir_evidence(boundary)",
    "air_boundary_requires_rir_evidence(boundary)",
    "--air",
    "--air-json",
    "dump_air",
    "dump_air_json",
    "air_dump(air, stdout)",
    "air_dump_json(air, stdout)",
    "HIR CFG and RIR boundary evidence",
    "HIR CFG evidence",
    "RIR boundary evidence",
    "expected authority participant(s):",
    "evidence hir=",
    "hir_cfg=",
    "Reason:",
    "Fix:",
]
missing_driver = [term for term in required_driver_terms if term not in driver]
if missing_driver:
    raise SystemExit("driver AIR validation missing term(s): " + ", ".join(missing_driver))

driver_authority_guard = re.compile(
    r"!\s*air_boundary_has_evidence\s*\([^)]*AIR_EVIDENCE_RIR_AUTHORITY[^)]*\)\s*"
    r"&&\s*driver_format_air_authority_names",
    re.S,
)
if driver_authority_guard.search(driver):
    raise SystemExit(
        "driver AIR authority diagnostics must report expected participants even "
        "when partial RIR authority evidence exists"
    )

if "AIRProgram" in compiler_header:
    raise SystemExit("CompilerIRBundle must not carry AIRProgram; AIR is verification-only and non-codegen")

required_source_span_terms = [
    (parser_intent, "step->line = name_tok.line", "parser intent step line"),
    (parser_intent, "step->column = name_tok.column", "parser intent step column"),
    (dir_header, "ASTNode    *ast;", "DIR intent step AST field"),
    (dir_impl, "step.ast = step_node", "DIR intent step AST capture"),
    (air_impl, "air_step_provenance_ast", "AIR step AST provenance seam"),
    (air_impl, "air_count_add", "AIR synthesis count overflow guard"),
    (air_impl, "AIR boundary count overflow", "AIR synthesis boundary overflow diagnostic"),
    (air_impl, "? node", "AIR expression boundary span keeps node when available"),
    (air_impl, ": ctx->step->ast", "AIR expression boundary span falls back to step AST"),
    (air_test, "air->intents[0].ast->line > 0", "AIR parsed source intent span test"),
    (air_test, "air->boundaries[0].ast->line > 0", "AIR parsed source boundary span test"),
    (air_test, "found_io_drift", "AIR parsed IO boundary drift is tied to IO node"),
    (diagnostics_json, 'data[0].get("location", {}).get("line", 0) > 0', "AIR JSON line assertion"),
    (diagnostics_json, 'data[0].get("location", {}).get("column", 0) > 0', "AIR JSON column assertion"),
    (diagnostics_json, "air-io-evidence", "AIR JSON parsed IO boundary evidence case"),
    (diagnostics_json, "ReadFile", "AIR JSON parsed IO boundary source"),
]
missing_span_terms = [label for text, needle, label in required_source_span_terms if needle not in text]
if missing_span_terms:
    raise SystemExit("AIR source span gate missing term(s): " + ", ".join(missing_span_terms))

required_dag_drift_terms = [
    (air_impl, "dag_dead_end_present", "AIR DAG dead-end drift JSON name"),
    (air_impl, "AIR DAG evidence contains unresolved metadata dead-end", "AIR DAG unresolved dead-end drift message"),
    (air_impl, "strict AIR requires graph-backed type evidence", "AIR DAG dead-end drift reason"),
    (air_impl, "missing DAG evidence node", "AIR DAG dead-end drift fix"),
    (air_impl, "strict AIR requires authority checks to be backed by RIR authority evidence", "AIR authority drift reason"),
    (air_impl, "strict AIR requires every authorized participant to be backed by RIR authority evidence", "AIR authority participant-complete drift reason"),
    (air_impl, "strict AIR requires every effect propagation op to carry resource/state evidence", "AIR effect propagation drift reason"),
    (air_impl, "strict AIR requires every relation propagation op to carry resource/state evidence", "AIR relation propagation drift reason"),
    (air_impl, "strict AIR requires observability ABI schema evidence", "AIR observability schema drift reason"),
    (air_impl, "strict AIR requires runtime frontier policy evidence", "AIR runtime frontier policy drift reason"),
]
missing_dag_drift_terms = [label for text, needle, label in required_dag_drift_terms if needle not in text]
if missing_dag_drift_terms:
    raise SystemExit("AIR DAG drift gate missing term(s): " + ", ".join(missing_dag_drift_terms))

required_evidence_shape_terms = [
    (air_impl, "air_evidence_node_matches_boundary_shape", "AIR evidence boundary shape validator"),
    (air_impl, "AIR global evidence node", "AIR global evidence concrete-boundary rejection"),
    (air_impl, "AIR HIR CFG evidence node", "AIR HIR CFG evidence shape rejection"),
    (air_impl, "AIR RIR authority evidence node", "AIR RIR authority evidence shape rejection"),
    (air_impl, "AIR MIR pin cleanup evidence node", "AIR MIR pin cleanup boundary rejection"),
    (air_impl, "bool present", "AIR evidence kind explicit presence bit"),
    (air_impl, "if (!kEvidenceKindMeta[kind].present)", "AIR evidence kind missing-meta rejection"),
    (air_impl, "AIR evidence append requires a known evidence kind", "AIR unknown evidence append rejection"),
]
missing_evidence_shape_terms = [label for text, needle, label in required_evidence_shape_terms if needle not in text]
if missing_evidence_shape_terms:
    raise SystemExit("AIR evidence shape gate missing term(s): " + ", ".join(missing_evidence_shape_terms))

backend_non_consumers = [
    root / "src" / "compiler" / "compiler.c",
    root / "src" / "compiler" / "c_runner.c",
    root / "src" / "compiler" / "llvm_runner.c",
]
backend_non_consumers.extend((root / "src" / "codegen").glob("*.c"))
backend_non_consumers.extend((root / "src" / "codegen").glob("*.h"))
for path in backend_non_consumers:
    text = path.read_text(encoding="utf-8", errors="ignore")
    if "air.h" in text or "AIRProgram" in text or "air_synthesize" in text:
        raise SystemExit(
            f"backend/codegen file must not consume AIR directly: {path.relative_to(root)}"
        )

summary_flags = [
    "has_hir_routine_evidence",
    "has_hir_cfg_evidence",
    "has_rir_boundary_evidence",
    "has_rir_authority_evidence",
]
summary_allowed = {
    root / "src" / "compiler" / "air.h",
    root / "src" / "compiler" / "air_evidence_mir.c",
    root / "src" / "compiler" / "air_evidence_rir.c",
    root / "src" / "compiler" / "air_validate_evidence.c",
    root / "src" / "compiler" / "air_validate_boundary_summary.c",
}
for path in (root / "src" / "compiler").glob("*.c"):
    if path in summary_allowed:
        continue
    text = path.read_text(encoding="utf-8", errors="ignore")
    for flag in summary_flags:
        if flag in text:
            raise SystemExit(
                "AIR boundary summary flag escaped its evidence owner: "
                f"{path.relative_to(root)} uses {flag}"
            )
if "air_boundary_has_authoritative_evidence" in (root / "src" / "compiler" / "air_verify.c").read_text(encoding="utf-8", errors="ignore"):
    raise SystemExit(
        "AIR verify must consume boundary evidence through air_boundary_has_evidence"
    )
air_verify_text = (root / "src" / "compiler" / "air_verify.c").read_text(encoding="utf-8", errors="ignore")
if "air_evidence_inventory_is_authoritative" in air_verify_text:
    raise SystemExit(
        "AIR verify must not own authoritative evidence selection"
    )
if "air_boundary_has_evidence_kind_subject" in air_verify_text:
    raise SystemExit(
        "AIR verify must not scan participant evidence directly"
    )
if "air_validate_boundary_summary_shape" not in (
    root / "src" / "compiler" / "air_validate.c"
).read_text(encoding="utf-8", errors="ignore"):
    raise SystemExit("AIR validate must delegate boundary summary shape checks")
if "air_validate_boundary_summary_shape" in (
    root / "src" / "compiler" / "air_validate_evidence.c"
).read_text(encoding="utf-8", errors="ignore"):
    raise SystemExit("AIR evidence inventory owner must not own boundary summary shape checks")
if "kBoundaryEvidenceSummaryRules" in (
    root / "src" / "compiler" / "air_validate_evidence.c"
).read_text(encoding="utf-8", errors="ignore"):
    raise SystemExit("AIR evidence inventory owner must not own boundary summary inventory rules")
if "air_validate_boundary_summary_shape" not in (
    root / "src" / "compiler" / "air_validate_boundary_summary.c"
).read_text(encoding="utf-8", errors="ignore"):
    raise SystemExit("AIR boundary summary shape checks must live in their own owner")
if "air_validate_boundary_summary_inventory" not in (
    root / "src" / "compiler" / "air_validate_boundary_summary.c"
).read_text(encoding="utf-8", errors="ignore"):
    raise SystemExit("AIR boundary summary inventory checks must live in their own owner")
for flag in summary_flags:
    if flag in air_verify_text:
        raise SystemExit(
                "AIR verify must not read boundary summary flags directly: "
            f"{flag}"
        )
air_validate_evidence_text = (root / "src" / "compiler" / "air_validate_evidence.c").read_text(encoding="utf-8", errors="ignore")
if "air_boundary_has_evidence(const AIRProgram *air" not in air_validate_evidence_text:
    raise SystemExit("AIR evidence owner must expose air_boundary_has_evidence")
if "return air_boundary_has_summary_flag(boundary, kind);" not in air_validate_evidence_text:
    raise SystemExit("AIR boundary summary fallback must stay inside the evidence owner")
if "air_boundary_missing_authority_evidence(const AIRProgram *air" not in air_validate_evidence_text:
    raise SystemExit("AIR evidence owner must expose missing authority evidence lookup")
if "air_global_evidence_fact_count(const AIRProgram *air" not in air_validate_evidence_text:
    raise SystemExit("AIR evidence owner must expose global evidence fact counting")
if "air_global_evidence_fallback_count(const AIRProgram *air" not in air_validate_evidence_text:
    raise SystemExit("AIR evidence owner must expose global evidence fallback counting")
if "air_global_evidence_node_count(const AIRProgram *air" not in air_validate_evidence_text:
    raise SystemExit("AIR evidence owner must expose global evidence node counting")
if "air_global_has_evidence_kind(const AIRProgram *air" not in air_validate_evidence_text:
    raise SystemExit("AIR evidence owner must expose global evidence presence lookup")
if "air_boundary_evidence_node_count(const AIRProgram *air" not in air_validate_evidence_text:
    raise SystemExit("AIR evidence owner must expose boundary evidence node counting")
air_validate_summary_counters_text = (
    root / "src" / "compiler" / "air_validate_summary_counters.c"
).read_text(encoding="utf-8", errors="ignore")
if "air_evidence_summary_count(const AIRProgram *air" not in air_validate_summary_counters_text:
    raise SystemExit("AIR summary-counter owner must expose summary evidence counting")
if "air_increment_evidence_summary_count(AIRProgram *air" not in air_validate_summary_counters_text:
    raise SystemExit("AIR summary-counter owner must expose summary evidence mutation")
if "air_evidence_required_count(const AIRProgram *air" not in air_validate_summary_counters_text:
    raise SystemExit("AIR summary-counter owner must expose required evidence counting")
if "air_increment_evidence_required_count(AIRProgram *air" not in air_validate_summary_counters_text:
    raise SystemExit("AIR summary-counter owner must expose required evidence mutation")
for path in (root / "src" / "compiler").glob("air*.c"):
    if path.name == "air.c":
        continue
    text = path.read_text(encoding="utf-8", errors="ignore")
    if "->strict_evidence" in text or ".strict_evidence" in text:
        raise SystemExit(
            "AIR strict-evidence consumers must use air_requires_strict_evidence: "
            + str(path)
        )
if "air_increment_evidence_summary_count(" not in air_evidence_dag_text:
    raise SystemExit("AIR DAG evidence must mutate summary counters through the summary owner")
if "air->dag_metadata_evidence_count++" in air_evidence_dag_text:
    raise SystemExit("AIR DAG evidence reintroduced direct metadata summary mutation")
if "air_increment_evidence_summary_count(air, kind)" not in air_evidence_runtime_path.read_text(encoding="utf-8"):
    raise SystemExit("AIR runtime evidence must mutate summary counters through the summary owner")
if "&air->runtime_frontier_policy_evidence_count" in air_evidence_runtime_path.read_text(encoding="utf-8"):
    raise SystemExit("AIR runtime evidence reintroduced raw summary counter pointers")
air_evidence_mir_text = air_evidence_path.read_text(encoding="utf-8")
air_evidence_mir_facts_text = air_evidence_mir_facts_path.read_text(encoding="utf-8")
if "air_collect_mir_requires_routine_inventory" not in air_evidence_mir_text:
    raise SystemExit("AIR MIR evidence must explicitly validate MIR routine inventory")
if "AIR MIR evidence requires block inventory" not in air_evidence_mir_text:
    raise SystemExit("AIR MIR evidence must fail closed on missing block inventory")
if "routine->block_count > 0 && routine->blocks == NULL" not in air_evidence_mir_facts_text:
    raise SystemExit("AIR MIR fact counters must be null-safe for missing block inventory")
if "routine->blocks != NULL" not in air_evidence_mir_facts_text:
    raise SystemExit("AIR MIR cleanup-root validation must require block storage")
for path in [
    air_evidence_hir_path,
    air_evidence_path,
    air_evidence_mir_pin_path,
    air_evidence_dag_path,
    air_evidence_rir_path,
    air_evidence_rir_propagation_path,
    air_evidence_rir_boundary_path,
    air_evidence_runtime_path,
]:
    evidence_text = path.read_text(encoding="utf-8")
    if "_evidence_count++" in evidence_text:
        raise SystemExit(
            "AIR evidence collectors must mutate summary counters through "
            "the summary owner: " + str(path)
        )
    if "_propagation_required_count++" in evidence_text:
        raise SystemExit(
            "AIR evidence collectors must mutate required counters through "
            "the summary owner: " + str(path)
        )
for path in [air_dump_path, air_dump_json_path]:
    dump_text = path.read_text(encoding="utf-8")
    for raw_counter in [
        "air->hir_routine_evidence_count",
        "air->hir_cfg_evidence_count",
        "air->rir_boundary_evidence_count",
        "air->rir_authority_evidence_count",
        "air->mir_cleanup_evidence_count",
        "air->mir_pin_cleanup_evidence_count",
        "air->mir_terminator_evidence_count",
        "air->mir_select_receive_evidence_count",
        "air->dag_metadata_evidence_count",
        "air->dag_generic_evidence_count",
        "air->dag_ability_evidence_count",
        "air->rir_effect_propagation_evidence_count",
        "air->rir_effect_propagation_required_count",
        "air->rir_relation_propagation_evidence_count",
        "air->rir_relation_propagation_required_count",
        "air->observability_schema_evidence_count",
        "air->runtime_frontier_policy_evidence_count",
    ]:
        if raw_counter in dump_text:
            raise SystemExit(
                "AIR dumps must consume summary counters through the "
                "summary owner: " + raw_counter
            )
    for raw_field in [
        "evidence->kind",
        "evidence->boundary_index",
        "evidence->provider_name",
        "evidence->subject_name",
        "evidence->fact_count",
        "evidence->fallback_count",
    ]:
        if raw_field in dump_text:
            raise SystemExit(
                "AIR dumps must consume evidence node fields through the "
                "evidence-node owner: " + raw_field
            )
for accessor in [
    "air_evidence_node_kind",
    "air_evidence_node_boundary_index_or",
    "air_evidence_node_provider_name_or",
    "air_evidence_node_subject_name_or",
    "air_evidence_node_fact_count",
    "air_evidence_node_fallback_count",
]:
    if accessor not in air_impl:
        raise SystemExit("AIR evidence-node owner missing accessor: " + accessor)
air_validate_global_evidence_text = air_validate_global_evidence_path.read_text(encoding="utf-8")
for raw_field in [
    "evidence->kind",
    "evidence->provider_name",
    "evidence->subject_name",
    "evidence->fact_count",
    "evidence->fallback_count",
]:
    if raw_field in air_validate_global_evidence_text:
        raise SystemExit(
            "AIR global evidence validator must consume evidence node fields "
            "through the evidence-node owner: " + raw_field
        )
air_validate_boundary_evidence_text = air_validate_boundary_evidence_path.read_text(encoding="utf-8")
for raw_field in [
    "evidence->kind",
    "evidence->boundary_index",
    "evidence->provider_name",
    "evidence->subject_name",
    "evidence->fact_count",
    "evidence->fallback_count",
]:
    if raw_field in air_validate_boundary_evidence_text:
        raise SystemExit(
            "AIR boundary evidence validator must consume evidence node fields "
            "through the evidence-node owner: " + raw_field
        )
allowed_raw_evidence_paths = {
    root / "src" / "compiler" / "air_evidence_node.c",
}
for path in (root / "src" / "compiler").glob("air*.[ch]"):
    if path in allowed_raw_evidence_paths:
        continue
    text = path.read_text(encoding="utf-8", errors="ignore")
    for raw_field in [
        "evidence->kind",
        "evidence->boundary_index",
        "evidence->provider_name",
        "evidence->subject_name",
        "evidence->fact_count",
        "evidence->fallback_count",
    ]:
        if raw_field in text:
            raise SystemExit(
                "AIR consumers must read AIREvidenceNode fields through "
                "the evidence-node owner: "
                + str(path.relative_to(root))
                + " "
                + raw_field
            )
for accessor in [
    "air_boundary_evidence_node(const AIRProgram *air",
    "air_boundary_evidence_provider(const AIRProgram *air",
    "air_boundary_evidence_subject(const AIRProgram *air",
]:
    if accessor not in air_validate_evidence_text:
        raise SystemExit("AIR evidence owner must expose boundary evidence accessor: " + accessor)
if "air_global_has_evidence_kind" not in air_verify_global_text:
    raise SystemExit("AIR global verify must consume global evidence through evidence owner")
if "evidence_nodes" in air_verify_global_text:
    raise SystemExit("AIR global verify must not scan evidence_nodes directly")
for raw_counter in [
    "mir_cleanup_evidence_count",
    "mir_pin_cleanup_evidence_count",
    "mir_terminator_evidence_count",
    "mir_select_receive_evidence_count",
    "rir_effect_propagation_required_count",
    "rir_relation_propagation_required_count",
    "dag_metadata_evidence_count",
    "dag_generic_evidence_count",
    "dag_ability_evidence_count",
]:
    if raw_counter in air_verify_global_text:
        raise SystemExit(
            "AIR global verify must consume summary counters through "
            "air_validate_summary_counters owner: " + raw_counter
        )

for rel in [
    "src/compiler/air_dump.c",
    "src/compiler/driver_diag.c",
]:
    text = (root / rel).read_text(encoding="utf-8", errors="ignore")
    for field in [
        "hir_routine_evidence_name",
        "rir_boundary_evidence_scope",
        "rir_authority_evidence_name",
    ]:
        if field in text:
            raise SystemExit(
                f"AIR user-facing evidence surface must use EvidenceNode accessor: {rel} uses {field}"
            )

required_test_terms = [
    "AIR synthesis creates intent and boundary nodes",
    "AIR drift checker reports sync/async mismatch",
    "AIR drift checker accepts matching async boundary",
    "AIR strict evidence reports missing RIR boundary",
    "AIR strict evidence requires HIR for implementation boundary",
    "AIR strict evidence rejects stale boundary summary flags",
    "AIR strict evidence rejects summary flags with real input",
    "AIR verify rejects boundary summary without inventory",
    "test_air_strict_evidence_rejects_summary_flags_with_real_input",
    "test_air_verify_rejects_summary_without_inventory",
    "test_air_has_evidence_ignores_summary_flags_with_real_input",
    "AIR task group boundary requires RIR and HIR evidence",
    "AIR verify rejects invalid boundary inventory",
    "AIR verify rejects missing inventory arrays",
    "AIR verify rejects boundary step mismatch",
    "AIR verify rejects boundary owner mismatch",
    "AIR verify rejects boundary sync shape mismatch",
    "AIR verify rejects invalid drift inventory",
    "AIR verify rejects invalid evidence inventory",
    "AIR verify rejects duplicate evidence nodes",
    "AIR verify rejects evidence boundary shape mismatch",
    "AIR verify rejects empty boundary evidence",
    "evidence count without evidence array",
    "references missing boundary node",
    "has no provider provenance",
    "duplicates evidence node",
    "undeclared authority subject",
    "global evidence node",
    "no matching HIR routine evidence",
    "no matching RIR boundary evidence",
    "subject/source mismatch",
    "AIR verify rejects authority evidence shape mismatch",
    "AIR verify rejects CFG evidence without routine evidence",
    "AIR verify rejects empty evidence provenance",
    "PGY_AIR_INVARIANT_INVALID",
    "AIR check_drift remains verify compatibility wrapper",
    "AIR synthesis collects all RIR authority evidence",
    "AIR strict evidence rejects mismatched authority participant",
    "AIR strict evidence requires all authority participants",
    "AIR dump prints evidence provenance",
    "AIR JSON dump prints stable graph schema",
    "AIR strict evidence rejects observability counter only",
    "AIR rejects invalid runtime frontier policy provider",
    "AIR rejects empty runtime frontier policy evidence",
    "expected=15 actual=0",
    "AIR strict evidence rejects frontier counter only",
    "pgy.air.graph.v1",
    "pgy.intent.observability.v1",
    "pgy.intent.trace.v1",
    "PGY_OBSERVABILITY_SURFACE_LAST",
    "PGY_OBSERVABILITY_EVENT_INTENT_ENTER",
    "PGY_FRONTIER_PASS_LIMIT_FACT_COUNT",
    "PGY_FRONTIER_OVERFLOW_REASON_FACT_COUNT",
    "PGY_FRONTIER_POLICY_FACT_COUNT",
    "pass_limit_fact_count",
    "overflow_reason_fact_count",
    "PGY_FRONTIER_PUBLISH_READY",
    "PGY_FRONTIER_PUBLISH_CLEAR_DIRTY",
    "mir_pin_cleanup_evidence_count",
    "mir_terminator_evidence_count",
    "mir_select_receive_evidence_count",
    "AIR collects MIR terminator evidence",
    "AIR collects MIR select receive evidence",
    "AIR append rejects unknown evidence kind",
    "AIR rejects MIR evidence counter mismatch",
    "AIR collects Void fallthrough terminator evidence",
    "AIR collects MIR pin cleanup evidence",
    "AIR rejects orphan MIR pin cleanup evidence",
    "AIR strict evidence rejects pin cleanup without cleanup root",
    "AIR rejects MIR pin cleanup evidence fact-count mismatch",
    "AIR rejects MIR pin cleanup counter mismatch",
    "AIR strict evidence rejects MIR pin cleanup counter only",
    "AIR rejects MIR pin cleanup without global cleanup evidence",
    "AIR strict evidence requires MIR pin cleanup",
    "AIR strict evidence requires MIR terminator evidence",
    "AIR strict evidence rejects MIR terminator counter only",
    "AIR strict evidence rejects MIR cleanup counter only",
    "AIR strict evidence rejects MIR select receive counter only",
    "AIR collects MIR cleanup block evidence",
    "AIR ignores orphan MIR cleanup root evidence",
    "AIR ignores unreachable MIR cleanup root evidence",
    "AIR ignores unreachable MIR cleanup source evidence",
    "AIR rejects empty MIR cleanup evidence",
    "AIR collects DAG generic ability evidence",
    "AIR collects RIR effect relation propagation evidence",
    "AIR rejects RIR evidence without scope provider",
    "AIR reports DAG dead-end drift",
    "AIR strict evidence rejects DAG counter only",
    "AIR RIR evidence requires scope name or owner provenance",
    "test_air_rejects_rir_evidence_without_scope_provider",
    "AIR rejects empty RIR propagation evidence",
    "AIR rejects RIR propagation evidence counter mismatch",
    "AIR strict evidence rejects RIR propagation counter only",
    "RIR effect propagation evidence counter does not match evidence facts",
    "RIR relation propagation evidence counter does not match evidence facts",
    "AIR MIR pin cleanup evidence counter does not match evidence nodes",
    "AIR MIR evidence counter has no matching boundary evidence node",
    "test_air_rejects_rir_propagation_counter_mismatch",
    "test_air_rejects_pin_cleanup_counter_mismatch",
    "test_air_strict_evidence_rejects_pin_cleanup_counter_only",
    "AIR rejects invalid DAG evidence provider",
    "AIR rejects empty DAG evidence",
    "AIR rejects DAG dead-end evidence",
    "DAG evidence node 0 has unresolved metadata dead-end facts",
    "AIR DAG evidence counter has no matching evidence node",
    "strict_evidence=yes hir_input=yes rir_input=yes",
    "mir_input",
    "AIR pin boundary has no matching MIR pin cleanup evidence",
    "evidence hir=yes(reserve) hir_cfg=yes",
    "evidence_node[0] kind=hir_routine",
    "evidence_node[3] kind=rir_authority",
    "AIR_EVIDENCE_MIR_CLEANUP",
    "AIR_EVIDENCE_MIR_PIN_CLEANUP",
    "AIR_EVIDENCE_MIR_TERMINATOR",
    "AIR_EVIDENCE_MIR_SELECT_RECEIVE",
    "AIR_EVIDENCE_OBSERVABILITY_SCHEMA",
    "AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY",
    "AIR singleton global evidence has conflicting counts",
    "AIR collects singleton global evidence idempotently",
    "AIR rejects conflicting singleton global evidence",
    "AIR has no runtime observability schema evidence",
    "AIR has no runtime frontier policy evidence",
    "AIR MIR input has no CFG terminator evidence",
    "must carry exactly one boundary fact",
    "cleanup-block",
    "AIR_EVIDENCE_DAG_GENERIC",
    "AIR_EVIDENCE_DAG_METADATA",
    "AIR_EVIDENCE_DAG_ABILITY",
    "AIR_DRIFT_DAG_DEAD_END_PRESENT",
    "air_collect_mir_evidence",
    "air_collect_dag_evidence",
    "mir_pin_cleanup_evidence_count",
    "mir_terminator_evidence_count",
    "mir_select_receive_evidence_count",
    "dag_metadata_evidence_count",
    "dag_generic_evidence_count",
    "dag_ability_evidence_count",
    "type_resolution_metadata_entries",
    "AIR world boundary requires transfer evidence",
    "AIR world boundary accepts transfer evidence",
    "AIR world boundary rejects mismatched transfer AST evidence",
    "expected authority participant(s): shipper",
    "AIR synthesis collects HIR/RIR evidence without mutation",
    "dir_owner_before",
    "hir_owner_before",
    "rir_kind_before",
    "hir_routine_evidence_name",
    "rir_boundary_evidence_scope",
    "rir_authority_evidence_name",
    "AIR lowers parsed intent source without drift",
    "AIR synthesis captures spawn boundary from intent step AST",
    "AIR synthesis captures boundary from let initializer",
    "test_air_synthesizes_boundary_from_let_initializer",
    "AIR synthesis captures boundary from event handler payload",
    "test_air_synthesizes_boundary_from_event_handler_payload",
    "AIR await boundary accepts exact RIR evidence",
    "AIR await boundary rejects generic RIR scope evidence",
    "AIR channel boundary accepts exact RIR op evidence",
    "AIR HIR evidence accepts nested execution boundary AST",
    "AIR HIR evidence accepts loop condition boundary AST",
    "AIR synthesis captures IO boundary without sync drift",
    "AIR synthesis captures stable execution boundary set",
    "found_pin",
    "found_event_subscribe",
    "found_event_unsubscribe",
    "found_task_group",
    "found_nested_io",
    "air->boundary_count == 14",
    "AIR parsed IO boundary accepts exact RIR evidence",
    "AIR parsed transfer emits zone and world boundaries",
    "AIR parsed transfer reports world missing transfer evidence",
    "AIR parsed transfer reports zone missing authority evidence",
    "found_zone_evidence",
    "found_world_evidence",
    "found_world_drift",
    "found_zone_authority_drift",
    "found_world_transfer_evidence",
]
missing_test = [term for term in required_test_terms if term not in air_test]
if missing_test:
    raise SystemExit("AIR test missing term(s): " + ", ".join(missing_test))

if "fallback DAG facts" in air_impl or "fallback DAG facts" in air_global:
    raise SystemExit("AIR DAG validator regressed to fallback-era DAG evidence wording")

required_rir_test_terms = [
    "RIR materializes parallel async and spawn boundary ops",
    "RIR_OP_SPAWN",
    "RIR_OP_ASYNC",
    "RIR_OP_PARALLEL",
]
missing_rir_test = [term for term in required_rir_test_terms if term not in rir_test]
if missing_rir_test:
    raise SystemExit("RIR test missing AIR boundary term(s): " + ", ".join(missing_rir_test))

required_diag_docs_terms = [
    "PGY_SEM_INTENT_BOUNDARY_DRIFT",
    "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING",
    "PGY_AIR_INVARIANT_INVALID",
    "PGY_CAUSE_INTENT_BOUNDARY_EVIDENCE",
    "PGY_FIX_ALIGN_INTENT_BOUNDARY_EVIDENCE",
]
missing_diag_docs = [term for term in required_diag_docs_terms if term not in diag_docs]
if missing_diag_docs:
    raise SystemExit("diagnostic docs missing AIR term(s): " + ", ".join(missing_diag_docs))

required_nonimpact_terms = [
    "PGY_AIR_STRICT_EVIDENCE=0",
    "PGY_AIR_NONIMPACT_SOURCE",
    "PGY_AIR_NONIMPACT_CASE_LIMIT",
    "PGY_AIR_NONIMPACT_SHARD_COUNT",
    "PGY_AIR_NONIMPACT_SHARD_INDEX",
    "find tests/cases/backend_compare",
    "intent_cross_world_transfer",
    "handoff_projection_frontier",
    "handoff_world_state_frontier",
    "world_zone_projection_visibility",
    "world_embedded_action_frontier",
    "relation_effect_propagation",
    "authority_failure_surface",
    "--emit-c",
    "--emit-llvm",
    "generated $name output changed under default strict AIR",
]
missing_nonimpact = [term for term in required_nonimpact_terms if term not in air_backend_nonimpact]
if missing_nonimpact:
    raise SystemExit("AIR backend nonimpact smoke missing term(s): " + ", ".join(missing_nonimpact))

required_semantics_terms = [
    "## Theorem: AIR Synthesis Read-Only",
    "## Theorem: Intent Node Coverage",
    "## Theorem: Boundary Closure",
    "## Theorem: Strict Evidence Failure Soundness",
    "## Theorem: Drift Detection Soundness",
    "## Theorem: Codegen Non-Impact",
    "PGY_SEM_INTENT_BOUNDARY_DRIFT",
    "PGY_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING",
]
missing_semantics = [term for term in required_semantics_terms if term not in air_semantics]
if missing_semantics:
    raise SystemExit("AIR semantics doc missing term(s): " + ", ".join(missing_semantics))

print("AIR drift smoke: ok")
PY
fi
