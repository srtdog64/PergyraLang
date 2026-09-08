#!/usr/bin/env bash
# Structural residue gate only; execution belongs to unsafe_block_execution.py.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

fail() { echo "[mir-lexical-binding] $*" >&2; exit 1; }

grep -Fq 'semantic_binding_syntax_id' src/parser/ast.h ||
    fail 'identifier reads lost the semantic declaration identity'
grep -Eq 'HIRLocalBinding[[:space:]]+\*local_defs' src/compiler/hir.h ||
    fail 'HIR definitions no longer carry binding identities'
grep -Fq 'mir_find_ssa_binding_index(' src/compiler/mir_ssa_use_edges.c ||
    fail 'MIR uses no longer resolve the binding identity index'
grep -Fq 'llvm_mir_seed_instruction_use_scope(inst, ctx, vars, var_count)' \
    src/codegen/llvm_mir_block_emit.c || fail 'LLVM lost per-instruction use binding'
grep -Fq 'if (required)' src/codegen/llvm_mir_block_scope.c ||
    fail 'required SSA storage no longer fails closed'
if grep -Eq 'first_ver|"%s\.1"' src/codegen/llvm_mir_block_scope.c; then
    fail 'LLVM repaired missing storage with a first-version spelling'
fi

grep -Fq 'semantic_binding_is_host_field' src/parser/ast.h ||
    fail 'identifier identity lost its hosted-field distinction'
grep -Fq 'ast_identifier_binding_is_host_field(' src/compiler/hir_cfg_phi.c ||
    fail 'HIR local definitions no longer distinguish hosted storage'
grep -Fq 'MIR value-result entry' src/codegen/transpiler_mir_func_ssa_locals_emit.c ||
    fail 'C formal entry no longer requires admitted parameter storage'
for owner in src/self_hosted/mir/local_ref_json_projection_owner.pgy \
    src/self_hosted/compiler/direct_mir_scalar_cfg_wire_local_ref_owner.pgy; do
    grep -Fq 'SelfMirLocalRefSpellingCollision(' "$owner" ||
        fail 'LocalRef producer/reader recreated a spelling-collision policy'
done
if grep -Fq 'signature.parameters.names[parameter] ==' \
    src/self_hosted/compiler/driver_rung2_scalar_c_substitution_owner.pgy; then
    fail 'C entrypoint recreated its own formal/local collision authority'
fi
if grep -Fq 'mir_find_ssa_name_index(' src/compiler/mir_ssa_rename.c \
    src/compiler/mir_ssa_use_edges.c; then
    fail 'MIR SSA resolution reintroduced display-name authority'
fi
if grep -Fq 'mir_phi_carries_value_result_parameter(' src/compiler/mir_dce.c; then
    fail 'DCE recreated inout copy-out liveness from parameter spelling'
fi
grep -Fq 'MIR_PARAM_CARRIAGE_VALUE_RESULT' src/compiler/mir_ssa_use_edges.c ||
    fail 'MIR return uses lost the admitted inout carriage consumer'
grep -Fq 'return_expression_use_count' src/compiler/mir_json_dump.c ||
    fail 'JSON expression uses no longer separate implicit copy-out liveness'
grep -Fq 'ast_func_param_stable_id(param) != binding_id' \
    src/compiler/mir_json_expression_graph_materialize.c ||
    fail 'expression formal binding no longer joins the semantic identity'
grep -Fq 'local->binding_syntax_id == binding_id' \
    src/compiler/mir_stmt_population_source.c ||
    fail 'assignment target mode no longer joins the resolved local identity'
if grep -Fq 'mir_assignment_target_root_name(' src/compiler/mir_stmt_population_source.c; then
    fail 'assignment target mode recreated a root-spelling owner'
fi
grep -Fq 'mir_block_binding_exit_ssa_name(routine, block,' \
    src/codegen/llvm_mir_param_emit.c || fail 'LLVM copy-out lost binding-ID exit lookup'
if grep -Fq 'strncmp(candidate, param->name' src/codegen/llvm_mir_param_emit.c; then
    fail 'LLVM copy-out reintroduced spelling-based exit selection'
fi
echo '[mir-lexical-binding] semantic identity carriage / old-path residue: PASS'
