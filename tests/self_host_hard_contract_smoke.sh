#!/usr/bin/env bash
# Gates the hard self-host contract: SoT closure is a substitution pass
# condition, C/LLVM remain the oracle pair, and active hard rungs stay wired
# into the preparation gate.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-hard-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains forbidden term: $term"
    fi
}

function_body() {
    local rel="$1"
    local function_name="$2"
    awk -v signature="func ${function_name}(" '
        index($0, signature) == 1 { in_function = 1 }
        in_function && $0 ~ /^func / && index($0, signature) != 1 { exit }
        in_function { print }
    ' "$ROOT_DIR/$rel"
}

require_function_text() {
    local rel="$1"
    local function_name="$2"
    local term="$3"
    function_body "$rel" "$function_name" | grep -F -- "$term" >/dev/null ||
        fail "$rel function $function_name missing term: $term"
}

forbid_function_text() {
    local rel="$1"
    local function_name="$2"
    local term="$3"
    if function_body "$rel" "$function_name" | grep -F -- "$term" >/dev/null; then
        fail "$rel function $function_name contains forbidden term: $term"
    fi
}

source "$ROOT_DIR/tests/self_hosted/compiler_world_manifest.sh"

bash "$ROOT_DIR/tests/mir_json_expression_graph_owner_smoke.sh"

require_file "docs/self_hosted/10_hard_self_host_contract.md"
require_file "docs/self_hosted/11_compiler_world_architecture.md"
require_file "docs/self_hosted/12_intent_zone_self_host_architecture.md"
require_file "tests/self_host_hard_contract_smoke.sh"
require_file "tests/self_hosted/compiler_world_manifest.sh"
require_file "tests/self_hosted/parity/self_host_compiler_build.sh"

pgy_compiler_world_require_manifest_paths "$ROOT_DIR" ||
    fail "compiler world path manifest is incomplete"

for term in \
    "Hard self-host is active as staged substitution" \
    "SoT is not a separate cleanup project during hard self-host. It is a pass" \
    "condition." \
    "the C compiler remains the primary oracle during hard substitution" \
    "LLVM remains the second oracle whenever the current build enables it" \
    "Bridge code is allowed. Fallback is not." \
    "self-hosted code rereading source AST text" \
    "missing MIR fact" \
    "Hard substitution"; do
    require_text "docs/self_hosted/10_hard_self_host_contract.md" "$term"
done

require_text "docs/INDEX.md" "self_hosted/10_hard_self_host_contract.md"
require_text "docs/self_hosted/README.md" "10_hard_self_host_contract.md"
require_text "docs/self_hosted/00_agent_entry.md" \
    "Keep the C compiler as the oracle during soft, partial, and hard substitution work."
require_text "src/self_hosted/README.md" \
    "The C compiler remains the oracle during soft, partial, and hard substitution."
require_text "tests/self_hosted/parity/README.md" \
    "Hard substitution rungs are parity gates promoted to pass conditions"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    '`src/self_hosted/` is for Pergyra source and owner documentation.'
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    '`tests/self_hosted/` is for parity scripts, committed fixtures, expected'
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "## Compiler World Rule"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "src/self_hosted/compiler/world.pgy"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "PgyCompilerWorld"
require_text "docs/self_hosted/11_compiler_world_architecture.md" \
    'PgyCompilerWorld` is the self-host compiler orchestration owner.'
require_text "src/self_hosted/compiler/world.pgy" \
    "world PgyCompilerWorld"
require_text "src/self_hosted/compiler/world.pgy" \
    "intent CompilePergyraProgram"
require_text "src/self_hosted/compiler/world.pgy" \
    "intent ProveSelfHostedParity"
require_text "src/self_hosted/compiler/stage_intents.pgy" \
    "intent FrontendPipeline"
require_text "src/self_hosted/compiler/stage_intents.pgy" \
    "intent BackendPipeline"
require_text "tests/self_hosted/README.md" \
    '`src/self_hosted/` is for Pergyra source owners.'
require_text "src/self_hosted/PROGRESS.md" \
    "Hard self-host contract"
require_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "func CompileMirJsonToCVerified"
require_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "let emission: CompilerEmissionArtifact = CompilerEmissionArtifact("
require_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "return CompileMirJsonTextToCVerified(json, machine_declaration);"
require_text "src/self_hosted/mir_lower/program_lower.pgy" \
    "func EmitMirProgramTreeProjectionFromRoutineIndexAndIntentPlanObserved("
require_text "src/self_hosted/compiler/driver_rung2_intent_consumer_owner.pgy" \
    "EmitMirProgramTreeProjectionFromRoutineIndexAndIntentPlanObserved("
require_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved" \
    "DriverRung2IntentTreeEmissionOrDie(admitted, observe_pressure)"
forbid_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "EmitMirProgramTreeProjectionFromRoutineIndexObserved("
forbid_text "src/self_hosted/compiler/driver_rung2_intent_consumer_owner.pgy" \
    "EmitMirProgramTreeProjectionFromRoutineIndexObserved("
require_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved" \
    "tree_emission.expression_order"
require_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "SemanticAstArtifactAnalyzeTyped(artifact, true)"
require_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "SemanticAstArtifactAnalyzeWithExpressionGraph("
require_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "MIR instruction expression graph is missing or invalid"
require_text "tests/self_hosted/parity/driver_rung2_body_parity.sh" \
    "producer-first source/MIR parity ok"
require_text "tests/self_hosted/parity/driver_rung2_mir_graph_negative_owner.sh" \
    "missing expression graph was accepted"
require_text "tests/self_hosted/parity/driver_rung2_mir_graph_negative_owner.sh" \
    "invalid expression graph was accepted"
require_text "tests/self_hosted/parity/driver_rung2_mir_graph_negative_owner.sh" \
    '"expr0_graph_removed"'
require_file "src/compiler/mir_json_expression_graph.c"
require_file "src/compiler/mir_json_expression_graph.h"
require_file "src/compiler/mir_json_expression_graph_materialize.c"
require_file "src/compiler/mir_json_expression_graph_materialize.h"
require_text "src/compiler/mir_json_dump.c" \
    "mir_json_emit_instruction_expression_graph(out, inst, 0);"
require_text "src/compiler/mir_json_dump.c" \
    "mir_json_emit_instruction_expression_graph(out, inst, 1);"
require_text "src/compiler/mir_json_expression_graph.c" \
    "mir_json_instruction_expression(const MIRInstruction *inst, int lane)"
require_text "src/compiler/mir_json_expression_graph.c" \
    "mir_json_expression_graph_build(&graph, expr)"
require_text "src/compiler/mir_json_expression_graph_materialize.c" \
    "case AST_ARRAY_LITERAL:"
require_text "src/compiler/mir_json_expression_graph_materialize.c" \
    'if (type == TOKEN_QUESTION)'
require_text "src/compiler/mir_json_expression_graph_materialize.c" \
    'if (ast_call_uses_braced_initializer_syntax(expr))'
require_text "src/compiler/mir_json_expression_graph_materialize.c" \
    'graph, "generic_type_actual", actual_text'
require_text "src/compiler/mir_json_expression_graph_materialize.c" \
    'graph, "generic_callee", callee_text'
require_text "src/compiler/mir_json_expression_graph_materialize.c" \
    'graph, "type_name", target_text'
require_text "src/compiler/mir_json_expression_graph_materialize.c" \
    'kind = "cast";'
require_text "src/self_hosted/mir_lower/expression_graph_sequence_owner.pgy" \
    'if kind == "float_literal" {'
require_text "src/self_hosted/codegen/input/ast_expression_usage_owner.pgy" \
    'func CodegenExpressionGraphUsagePresenceFromSemantic('
forbid_text "src/self_hosted/codegen/input/ast_expression_usage_owner.pgy" \
    'func CodegenSemanticCheckedIntegerCastTargetPresent('
require_text "src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy" \
    'if kind == AstExpressionNodeCast() &&'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" \
    'CheckedArithmeticRuntimeCProgramBlock(usage.uses_checked_int_cast, usage.uses_checked_long_cast)'
require_text "src/self_hosted/parser/expr_precedence_owner.pgy" \
    'func ParseCastFact('
forbid_function_text "src/self_hosted/parser/expr_precedence_owner.pgy" \
    "func ParseCastFact(" "ParserExpressionLeaf(Concat("
require_text "tests/self_host_live_replacement_smoke.sh" \
    'array_mir_source="src/self_hosted/codegen/fixture/array_return_literal.pgy"'
require_text "tests/self_host_live_replacement_smoke.sh" \
    'cast_mir_source="src/self_hosted/mir_lower/fixture/cast_numeric.pgy"'
require_text "tests/self_hosted/parity/driver_rung2_mir_graph_negative_owner.sh" \
    "value leaf was accepted as a cast target type"
require_text "tests/self_host_live_replacement_smoke.sh" \
    'check_live_mir_source "$array_mir_source" "array-return-literal"'
require_text "tests/self_host_live_replacement_smoke.sh" \
    'check_live_mir_source "$try_mir_source" "option-try"'
require_text "tests/self_host_live_replacement_smoke.sh" \
    'check_live_mir_source "$struct_mir_source" "struct-point"'
require_text "tests/self_host_live_replacement_smoke.sh" \
    'check_live_mir_source "$generic_mir_source" "generic-struct-field"'
require_text "tests/self_host_live_replacement_smoke.sh" \
    'check_live_mir_source "$generic_multi_mir_source" "generic-multi-actual"'
require_text "tests/self_host_live_replacement_smoke.sh" \
    'check_live_mir_source "$generic_member_mir_source" "generic-member-inferred"'
require_text "tests/self_host_live_replacement_smoke.sh" \
    'check_live_mir_source "$generic_vessel_member_mir_source" "generic-vessel-member-inferred"'
require_text "tests/self_host_live_replacement_smoke.sh" \
    '--canonicalize-oracle-mir-json "$live_arg"'
forbid_text "src/compiler/mir_json_expression_graph.c" \
    "mir_json_expression_graph_build_call("
forbid_text "src/compiler/mir_json_expression_graph.c" \
    "case AST_ARRAY_LITERAL:"
forbid_text "src/compiler/mir_json_expression_graph.c" \
    "parser_parse"
forbid_text "src/compiler/mir_json_expression_graph.c" \
    "ParseExpr"
forbid_text "src/compiler/mir_json_expression_graph_materialize.c" \
    "parser_parse"
forbid_text "src/compiler/mir_json_expression_graph_materialize.c" \
    "ParseExpr"
forbid_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CheckProgram("
forbid_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "SemanticAstArtifactAnalyze(artifact, true)"
forbid_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "SemanticExpressionGraphBuildFromText"
require_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "VerifyArtifactForMirProduction" \
    "SemanticAstArtifactAnalyzeTyped(artifact, true)"
forbid_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "VerifyArtifactForMirProduction" \
    "SemanticAstArtifactAnalyzeCompactBridge"
require_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CanonicalizeMirJsonVerified" \
    "emission.expression_order"
require_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CanonicalizeMirJsonVerified" \
    "SemanticAstArtifactAnalyzeWithExpressionGraph("
forbid_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CanonicalizeMirJsonVerified" \
    "SemanticAstArtifactAnalyzeCompactBridge"
require_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CanonicalizeOracleMirJsonBridge" \
    "SemanticAstArtifactAnalyzeCompactBridge(artifact, true)"
forbid_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CompileSourceToMirJsonVerified" \
    "CanonicalizeOracleMirJsonBridge("
forbid_function_text "src/self_hosted/compiler/driver_rung2_owner.pgy" \
    "CompileSourceToCVerified" \
    "CanonicalizeOracleMirJsonBridge("
require_text "src/self_hosted/compiler/driver_rung2_cli_owner.pgy" \
    'args[0] == "--canonicalize-oracle-mir-json"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/device_slot_machine_layer/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/device_slot_remote/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/device_slot_routine/main.pgy"'
require_file "tests/self_hosted/parity/driver_rung2_machine_mir_parity_owner.sh"
require_text "tests/self_hosted/parity/driver_rung2_machine_mir_parity_owner.sh" \
    "machine MIR producer accepted missing declaration"
require_file "tests/self_hosted/parity/mir_abi_first_lane.sh"
require_file "tests/self_hosted/fixtures/machine_layer_declaration.json"
forbid_text "tests/self_hosted/parity/driver_rung2_machine_mir_parity_owner.sh" \
    '"$PGY" --machine-manifest-json'
require_text "tests/self_hosted/parity/mir_abi_first_lane.sh" \
    'PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER="device_slot_machine_layer,device_slot_remote,device_slot_routine,option_string_core,array_sum_filtered,str_array,array_scalar_aggregate_core,array_double_aggregate_core"'
require_text "tests/self_hosted/parity/mir_abi_first_lane.sh" \
    "abi_layout_row_manifest_parity.sh"
require_text "tests/self_hosted/parity/mir_abi_first_lane.sh" \
    "runtime_call_abi_row_manifest_parity.sh"
require_file "tests/self_hosted/parity/driver_rung2_resource_runtime_abi_negative_owner.sh"
require_text "tests/self_hosted/parity/driver_rung2_resource_runtime_abi_negative_owner.sh" \
    "resource instruction or consumer is missing its lowered runtime-call ABI row"
require_file "src/self_hosted/compiler/target_projection_fact_owner.pgy"
require_text "src/self_hosted/compiler/target_projection_fact_owner.pgy" \
    "func CompilerTargetProjectionFactReadyFor("
require_text "src/self_hosted/compiler/target_projection_fact_owner.pgy" \
    "target_capability_fingerprint: Int;"
require_text "src/self_hosted/compiler/target_projection_fact_owner.pgy" \
    "CompilerTargetCapabilityFingerprint()"
require_text "src/self_hosted/compiler/target_projection_fact_owner.pgy" \
    "fact.target_capability_fingerprint !="
require_text "src/self_hosted/codegen/emission/program_entry_owner.pgy" \
    "CompilerTargetProjectionFactReadyFor("
require_file "tests/self_hosted/parity/driver_rung2_target_projection_negative_owner.sh"
require_text "tests/self_hosted/parity/driver_rung2_target_projection_negative_owner.sh" \
    "self-host C emission target projection fact is missing or invalid"
forbid_function_text "src/self_hosted/codegen/emission/program_entry_owner.pgy" \
    "GenerateCFromVerifiedSemanticArtifact" \
    "CompilerTargetCapabilityEnvelopeReady()"
require_text "src/self_hosted/mir_lower/routine_lower.pgy" \
    "MirResourceRuntimeRowFactReady(routines, instruction)"
require_text "src/self_hosted/mir/routine_build_owner.pgy" \
    "CompilerRuntimeCallAbiFactForNativeResource("
require_file "src/self_hosted/mir/routine_local_inventory_owner.pgy"
require_text "src/self_hosted/mir/routine_local_inventory_owner.pgy" \
    "func SelfMirRoutineLocalInventoryFromInput("
require_text "src/self_hosted/mir/artifact_lower_owner.pgy" \
    "SelfMirRoutineLocalInventoryFromInput(input, function_node_id)"
forbid_text "src/self_hosted/mir/artifact_lower_owner.pgy" \
    "ArrayLength(build.local_names)"
forbid_text "src/self_hosted/mir/artifact_lower_owner.pgy" \
    "build.local_names[local_i]"
require_text "src/self_hosted/mir/routine_expression_use_owner.pgy" \
    "func SelfMirExpressionGraphUses("
require_text "src/self_hosted/mir/routine_assignment_owner.pgy" \
    "SelfMirExpressionGraphUses(build, target_graph)"
require_text "src/self_hosted/mir/routine_assignment_owner.pgy" \
    "SelfMirExpressionGraphUses(build, value_graph)"
forbid_text "src/self_hosted/mir/routine_assignment_owner.pgy" \
    "SelfMirExpressionUses(build, target_text)"
forbid_text "src/self_hosted/mir/routine_assignment_owner.pgy" \
    "SelfMirExpressionUses(build, expression)"
require_text "src/self_hosted/mir/routine_build_owner.pgy" \
    "SelfMirSsaBaseName(cfg.instructions.results[instruction_index])"
forbid_text "src/self_hosted/mir/routine_build_owner.pgy" \
    "return cfg.instructions.expr1s[instruction_index];"
require_text "src/self_hosted/mir/routine_build_owner.pgy" \
    "cfg.instructions.expr0_graphs"
require_text "src/self_hosted/mir/routine_build_owner.pgy" \
    "AstExpressionNodeCallArgument()"
require_text "src/self_hosted/mir/routine_build_owner.pgy" \
    "SemanticExpressionGraphLeftChild("
forbid_text "src/self_hosted/mir/routine_build_owner.pgy" \
    "cfg.instructions.uses[cfg.instructions.use_starts[instruction_index]]"
forbid_text "src/self_hosted/mir/routine_build_owner.pgy" \
    "SelfMirTextContainsIdentifier("
require_text "src/self_hosted/mir/routine_let_owner.pgy" \
    "SelfMirRoutineAttachLastAbiTypeName(build, local_type)"
require_text "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" \
    "rows.source_types[instruction_index] == \"AST_CALL\""
require_text "src/self_hosted/mir/instruction_validation_owner.pgy" \
    "SelfMirRuntimeCallAbiRowValidationError("
forbid_text "src/self_hosted/compiler/runtime_call_abi_structured_fact_owner.pgy" \
    "Split("

require_text "Makefile" "self-host-hard-contract-test-smoke"
require_text "Makefile" "self-host-compiler: self-host-codegen-bootstrap-seed-test-smoke"
require_text "Makefile" "tests/self_hosted/parity/self_host_compiler_build.sh"
require_text "Makefile" 'PGY_SELFHOST_CC="$(CC)"'
forbid_text "Makefile" '$(call pgy_run_native,"$(PGY)" src/self_hosted/compiler/driver_rung2_main.pgy'
require_text "tests/self_hosted/parity/self_host_compiler_build.sh" \
    'DRIVER_SOURCE="src/self_hosted/compiler/driver_bootstrap_main.pgy"'
forbid_text "tests/self_hosted/parity/self_host_compiler_build.sh" \
    'DRIVER_SOURCE="src/self_hosted/compiler/driver_rung2_main.pgy"'
require_text "src/pgy_driver.c" "if (flags.emit_c_only) {"
require_text "src/pgy_driver.c" "driver_run_self_host_c_emit_artifact("
require_text "src/pgy_driver.c" "driver_plain_c_binary_target_requested("
require_text "src/pgy_driver.c" "c_runner_execute_installed_self_host_c("
require_text "Makefile" "self-host-default-c-emit-replacement-test-smoke:"
require_text "Makefile" \
    "tests/self_hosted/parity/default_c_emit_installed_self_host_owner.sh"
require_text "Makefile" "self-host-default-c-compile-replacement-test-smoke:"
require_text "Makefile" \
    "tests/self_hosted/parity/default_c_compile_installed_self_host_owner.sh"
require_text "src/compiler/c_runner.c" \
    "compiler_compile_link_self_host_c_artifact("
forbid_text "src/compiler/self_host_driver.c" "driver_run_pipeline("
require_text "src/pgy_driver.c" "driver_plain_llvm_binary_target_requested("
require_text "src/pgy_driver.c" \
    "driver_self_host_llvm_artifact_request_supported("
require_text "src/pgy_driver.c" \
    "llvm_runner_execute_installed_self_host_llvm("
require_text "Makefile" "self-host-default-llvm-replacement-test-smoke:"
require_text "Makefile" \
    "tests/self_hosted/parity/default_llvm_installed_self_host_owner.sh"
require_text "Makefile" "self-host-one-mir-array-int-projection-test-smoke:"
require_text "Makefile" \
    "tests/self_hosted/parity/one_mir_array_int_projection.sh"
require_file "tests/self_hosted/parity/one_mir_array_int_projection.sh"
require_text "tests/self_hosted/parity/one_mir_array_int_projection.sh" \
    'printf '\''3\n10\n'\'''
require_text "tests/self_hosted/parity/one_mir_array_int_projection.sh" \
    'DirectMirArrayIntPlanMutationRejected'
require_text "Makefile" \
    "self-host-one-mir-array-return-projection-test-smoke: self-host-compiler"
require_text "Makefile" \
    "tests/self_hosted/parity/one_mir_array_return_projection.sh"
require_file "tests/self_hosted/parity/one_mir_array_return_projection.sh"
require_file "tests/self_hosted/parity/one_mir_array_return_mutations.py"
require_text "tests/self_hosted/parity/one_mir_array_return_projection.sh" \
    'printf '\''4\n3\n'\'''
require_text "tests/self_hosted/parity/one_mir_array_return_projection.sh" \
    'routine-order-swap'
require_text "src/self_hosted/compiler/direct_mir_array_return_plan_owner.pgy" \
    'caller_owned_fixed_array'
require_text "tests/self_hosted/parity/one_mir_array_return_projection.sh" \
    'abi-field-shape-repaired-id'
require_text "Makefile" '$(SELFHOST_ONE_MIR_ARRAY_RETURN_GATE)'
require_text "Makefile" \
    "self-host-one-mir-array-argument-projection-test-smoke: self-host-compiler"
require_text "Makefile" \
    "tests/self_hosted/parity/one_mir_array_argument_projection.sh"
require_file "tests/self_hosted/parity/one_mir_array_argument_projection.sh"
require_file "tests/self_hosted/parity/one_mir_array_argument_mutations.py"
require_text "tests/self_hosted/parity/one_mir_array_argument_projection.sh" \
    'printf '\''11\n'\'''
require_text "tests/self_hosted/parity/one_mir_array_argument_projection.sh" \
    'routine-order-cycle'
require_text "src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy" \
    'caller_owned_fixed_array_by_value'
require_text "src/self_hosted/compiler/direct_mir_array_argument_plan_owner.pgy" \
    'DirectMirArrayArgumentPlanMutationRejected'
require_text "tests/self_hosted/parity/one_mir_array_argument_projection.sh" \
    'repaired-param-abi'
require_text "Makefile" '$(SELFHOST_ONE_MIR_ARRAY_ARGUMENT_GATE)'
require_text "Makefile" \
    "self-host-one-mir-struct-argument-projection-test-smoke: self-host-compiler"
require_file "tests/self_hosted/parity/one_mir_struct_argument_projection.sh"
require_file "tests/self_hosted/parity/one_mir_struct_argument_mutations.py"
require_text "tests/self_hosted/parity/one_mir_struct_argument_projection.sh" \
    'C/LLVM exact 6'
require_text "tests/self_hosted/parity/one_mir_struct_argument_projection.sh" \
    'routine-declaration-order-cycle'
require_text "tests/self_hosted/parity/one_mir_struct_argument_projection.sh" \
    'repaired-line-layout-offset'
require_text "src/self_hosted/compiler/direct_mir_struct_argument_plan_owner.pgy" \
    'caller_owned_nested_value_by_value'
require_text "src/self_hosted/compiler/direct_mir_struct_argument_plan_owner.pgy" \
    'DirectMirStructArgumentPlanMutationRejected'
require_text "Makefile" '$(SELFHOST_ONE_MIR_STRUCT_ARGUMENT_GATE)'
require_file "src/self_hosted/mir/instruction_abi_receipt_fact_owner.pgy"
require_file \
    "src/self_hosted/mir/instruction_abi_receipt_json_projection_owner.pgy"
require_text "src/self_hosted/mir/program_fact_owner.pgy" \
    'instruction_abi_receipts: SelfMirInstructionAbiReceiptRows'
require_text "src/self_hosted/mir/artifact_lower_owner.pgy" \
    'SelfMirInstructionAbiReceiptRowsAppend('
forbid_text "src/self_hosted/mir/abi_layout_json_projection_owner.pgy" \
    'rows.expr1s[instruction_index]'
forbid_text "src/self_hosted/mir/json_projection_owner.pgy" \
    'SelfMirJsonOwnedAbiLayoutForType('
forbid_text "src/self_hosted/mir/instruction_json_artifact_writer_owner.pgy" \
    'SelfMirJsonOwnedAbiLayoutForType('
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-array-return-projection-test-smoke'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-array-argument-projection-test-smoke'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-struct-argument-projection-test-smoke'
require_text "Makefile" \
    'self-host-one-mir-struct-value-flow-projection-test-smoke: self-host-compiler'
require_file "tests/self_hosted/parity/one_mir_struct_value_flow_projection.sh"
require_file "tests/self_hosted/parity/one_mir_struct_value_flow_mutations.py"
require_text "tests/self_hosted/parity/one_mir_struct_value_flow_projection.sh" \
    'gate must produce source MIR exactly once'
require_text "tests/self_hosted/parity/one_mir_struct_value_flow_projection.sh" \
    'C/LLVM exact 11'
require_text "tests/self_hosted/parity/one_mir_struct_value_flow_projection.sh" \
    'routine-order-swap'
require_text "tests/self_hosted/parity/one_mir_struct_value_flow_projection.sh" \
    'repaired-all-layout-offsets'
require_text "src/self_hosted/compiler/direct_mir_struct_value_flow_plan_owner.pgy" \
    'independent_nominal_values_by_value'
require_text "src/self_hosted/compiler/direct_mir_array_return_program_identity_owner.pgy" \
    'JsonArrayObjectFactCount(admitted.document.declarations) == 0'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-struct-value-flow-projection-test-smoke'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-option-struct-value-flow-projection-test-smoke'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-generic-struct-value-flow-projection-test-smoke'
require_text "Makefile" \
    'self-host-one-mir-generic-struct-value-flow-projection-test-smoke: self-host-compiler'
require_file \
    "tests/self_hosted/parity/one_mir_generic_struct_value_flow_projection.sh"
require_file \
    "tests/self_hosted/parity/one_mir_generic_struct_value_flow_mutations.py"
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-inferred-generic-nominal-projection-test-smoke'
require_text "Makefile" \
    'self-host-one-mir-inferred-generic-nominal-projection-test-smoke: self-host-compiler'
require_file \
    "tests/self_hosted/parity/one_mir_inferred_generic_nominal_projection.sh"
require_file \
    "tests/self_hosted/parity/one_mir_inferred_generic_nominal_mutations.py"
require_text \
    "src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" \
    'DirectMirTwoRoutineClassificationFromAdmitted(admitted)'
require_text \
    "src/self_hosted/compiler/direct_mir_inferred_generic_nominal_plan_owner.pgy" \
    'inferred_generic_scalar_calls_into_nominal_value_by_value'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-inferred-generic-scalar-projection-test-smoke'
require_text "Makefile" \
    'self-host-one-mir-inferred-generic-scalar-projection-test-smoke: self-host-compiler'
require_file \
    "tests/self_hosted/parity/one_mir_inferred_generic_scalar_projection.sh"
require_file \
    "tests/self_hosted/parity/one_mir_inferred_generic_scalar_mutations.py"
require_text \
    "src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy" \
    'DirectMirThreeRoutineInferredGenericScalar()'
require_text \
    "src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy" \
    'CompileAdmittedDirectMirInferredGenericScalar('
require_text \
    "src/self_hosted/compiler/direct_mir_inferred_generic_scalar_plan_owner.pgy" \
    'mixed_lane_inferred_generic_return_assignment_by_value'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-inferred-generic-member-projection-test-smoke'
require_text "Makefile" \
    'self-host-one-mir-inferred-generic-member-projection-test-smoke: self-host-compiler'
require_file \
    "tests/self_hosted/parity/one_mir_inferred_generic_member_projection.sh"
require_file \
    "tests/self_hosted/parity/one_mir_inferred_generic_member_mutations.py"
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-constructed-generic-member-projection-test-smoke'
require_text "Makefile" \
    'self-host-one-mir-constructed-generic-member-projection-test-smoke: self-host-compiler'
require_file \
    "tests/self_hosted/parity/one_mir_constructed_generic_member_projection.sh"
require_file \
    "tests/self_hosted/parity/one_mir_constructed_generic_member_mutations.py"
require_text \
    "src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy" \
    'DirectMirThreeRoutineConstructedGenericMember()'
require_text \
    "src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy" \
    'CompileAdmittedDirectMirConstructedGenericMember('
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_generic_member_plan_owner.pgy" \
    'some_to_nested_member_option_to_checked_unwrap'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-constructed-array-member-projection-test-smoke'
require_text "Makefile" \
    'self-host-one-mir-constructed-array-member-projection-test-smoke: self-host-compiler'
require_file \
    "tests/self_hosted/parity/one_mir_constructed_array_member_projection.sh"
require_file \
    "tests/self_hosted/parity/one_mir_constructed_array_member_mutations.py"
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_generic_member_projection_owner.pgy" \
    'DirectMirConstructedMemberVariantFromPair(pair)'
require_text \
    "src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy" \
    'caller_owned_single_storage_nested_member_by_value'
require_text ".github/workflows/ci.yml" \
    'self-host-one-mir-constructed-record-array-member-projection-test-smoke'
require_text "Makefile" \
    'self-host-one-mir-constructed-record-array-member-projection-test-smoke: self-host-compiler'
require_file \
    "tests/self_hosted/parity/one_mir_constructed_record_array_member_projection.sh"
require_file \
    "tests/self_hosted/parity/one_mir_constructed_record_array_member_mutations.py"
require_text \
    "src/self_hosted/compiler/direct_mir_three_routine_classification_owner.pgy" \
    'DirectMirThreeRoutineMixedConstructedGenericMember()'
require_text \
    "src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy" \
    'CompileAdmittedDirectMirConstructedRecordArrayMember('
require_text \
    "src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy" \
    'DirectMirAggregateValueFlowTypedArrayAbsenceNominalElementAbi()'
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_program_admission_owner.pgy" \
    'JsonObjectFactCount(admitted.document.root) != 5'
for aggregate_representation_adapter in \
    direct_mir_constructed_array_member_representation_owner.pgy \
    direct_mir_constructed_record_array_member_representation_owner.pgy; do
    require_text "src/self_hosted/compiler/$aggregate_representation_adapter" \
        'DirectMirAggregateValueRepresentationFromSealedIdentity('
    forbid_text "src/self_hosted/compiler/$aggregate_representation_adapter" \
        'DirectMirInferredGenericMemberRepresentation()'
done
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_source_identity_owner.pgy" \
    'DirectMirConstructedRecordArraySourceIdentitiesReady('
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_instruction_envelope_owner.pgy" \
    'JsonObjectFactCount(instruction) == 25'
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_join_owner.pgy" \
    'DirectMirConstructedRecordArrayProgramJoinReady('
require_file \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_array_abi_absence_owner.pgy"
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_array_abi_absence_owner.pgy" \
    'DirectMirInstructionHasNoPhysicalAbi('
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_array_abi_absence_owner.pgy" \
    'fact.physical_abi_absent'
require_text \
    "src/self_hosted/compiler/direct_mir_array_storage_layout_contract_owner.pgy" \
    'pgy.runtime.pointer64-size_t64.v1'
require_text \
    "src/self_hosted/compiler/direct_mir_array_storage_abi_projection_owner.pgy" \
    'DirectMirArrayStorageAbiProjectionFromTarget('
require_text \
    "src/self_hosted/compiler/direct_mir_array_storage_symbol_owner.pgy" \
    '_pgy_array_storage_'
require_text \
    "src/self_hosted/compiler/direct_mir_array_storage_c_assertion_owner.pgy" \
    'Array storage size receipt'
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_array_member_plan_owner.pgy" \
    'DirectMirAggregateValueFlowFactReady(plan.aggregate_flow)'
for aggregate_flow_consumer in \
    direct_mir_constructed_array_member_plan_owner.pgy \
    direct_mir_constructed_record_array_member_plan_owner.pgy; do
    require_text "src/self_hosted/compiler/$aggregate_flow_consumer" \
        'direct_mir_aggregate_value_flow_fact_owner.pgy'
    forbid_text "src/self_hosted/compiler/$aggregate_flow_consumer" \
        'direct_mir_closed_module_call_abi_owner.pgy'
    forbid_text "src/self_hosted/compiler/$aggregate_flow_consumer" \
        'DirectMirAggregateValueFlowFactSeal('
    for aggregate_old_decision in \
        'let representation: DirectMirInferredGenericMemberRepresentationFact;' \
        'let call_abi: DirectMirClosedModuleCallAbiFact;' \
        'let storage_count: Int;' 'let selected_index: Int;' \
        'let value_carriage: String;' 'let array_abi_policy: String;' \
        'let target_capability_schema: String;' \
        'let target_capability_fingerprint: Int;' \
        'target_capability_owner.pgy' \
        'DirectMirConstructedArrayMemberCarriage' \
        'DirectMirConstructedRecordArrayMemberCarriage' \
        'DirectMirConstructedRecordArrayMemberAbiPolicy' \
        'caller_owned_fixed_array_through_nested_member_by_value' \
        'caller_owned_fixed_record_array_through_nested_member_by_value'; do
        forbid_text "src/self_hosted/compiler/$aggregate_flow_consumer" \
            "$aggregate_old_decision"
    done
done
for aggregate_forbidden_term in \
    MirMachineLayerAdmittedJsonInput JsonObjectFact ClassificationFact \
    target_projection CompilerTargetCpu; do
    forbid_text \
        "src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy" \
        "$aggregate_forbidden_term"
done
require_text \
    "src/self_hosted/compiler/direct_mir_aggregate_value_flow_fact_owner.pgy" \
    'if fact.call_abi.external_interop'
require_text \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_owner.pgy" \
    'plan.main.typed_array_abi_absence.digest'
forbid_text \
    "src/self_hosted/compiler/direct_mir_constructed_record_array_member_plan_owner.pgy" \
    'main.digest'
require_text \
    "src/self_hosted/compiler/direct_mir_aggregate_value_flow_target_projection_owner.pgy" \
    'projection.flow.call_abi.target_capability_fingerprint'
for aggregate_emitter in \
    direct_mir_constructed_array_member_c_emission_owner.pgy \
    direct_mir_constructed_array_member_llvm_emission_owner.pgy \
    direct_mir_constructed_record_array_member_c_emission_owner.pgy \
    direct_mir_constructed_record_array_member_llvm_emission_owner.pgy; do
    require_text "src/self_hosted/compiler/$aggregate_emitter" \
        'direct_mir_aggregate_value_flow_target_projection_owner.pgy'
    for aggregate_reowned_term in \
        'plan.representation' 'plan.call_abi' 'plan.storage_count' \
        'plan.selected_index' 'plan.value_carriage' \
        'plan.aggregate_flow.selected_index' 'define internal' '[1 x' \
        'i64 1,' 'ptr null' ', 1, 1, NULL' '[0] ='; do
        forbid_text "src/self_hosted/compiler/$aggregate_emitter" \
            "$aggregate_reowned_term"
    done
done
for array_c_emitter in \
    direct_mir_constructed_array_member_c_emission_owner.pgy \
    direct_mir_constructed_record_array_member_c_emission_owner.pgy; do
    forbid_text "src/self_hosted/compiler/$array_c_emitter" '__pgy_array_storage'
done
for array_storage_consumer in \
    direct_mir_array_int_abi_projection_owner.pgy \
    direct_mir_constructed_record_array_member_abi_projection_owner.pgy; do
    require_text "src/self_hosted/compiler/$array_storage_consumer" \
        'direct_mir_array_storage_abi_projection_owner.pgy'
    forbid_text "src/self_hosted/compiler/$array_storage_consumer" \
        'direct_mir_array_storage_layout_contract_owner.pgy'
done
require_text \
    "src/self_hosted/compiler/direct_mir_closed_module_call_abi_owner.pgy" \
    'closed_module_internal_only'
require_text \
    "src/self_hosted/mir/routine_tracked_statement_owner.pgy" \
    'return "AST_ARRAY_LITERAL"'
require_text \
    "src/self_hosted/compiler/direct_mir_two_routine_classification_owner.pgy" \
    'DirectMirTwoRoutineInferredGenericMember()'
require_text \
    "src/self_hosted/compiler/direct_mir_two_routine_classification_owner.pgy" \
    'DirectMirTwoRoutineInferredGenericVesselMember()'
require_text \
    "src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy" \
    'CompileAdmittedDirectMirInferredGenericMember('
require_text \
    "src/self_hosted/compiler/direct_mir_inferred_generic_member_representation_fact_owner.pgy" \
    'internal_single_int_value_class'
require_text \
    "src/self_hosted/compiler/direct_mir_inferred_generic_member_representation_fact_owner.pgy" \
    'internal_single_int_mutable_identity_vessel'
require_text \
    "src/self_hosted/compiler/direct_mir_inferred_generic_member_plan_owner.pgy" \
    'nested_inferred_generic_member_calls_on_admitted_receiver'
require_text "tests/self_hosted/parity/default_c_compile_installed_self_host_owner.sh" \
    'generic_member_inferred_flow.pgy'
require_text "tests/self_hosted/parity/default_c_compile_installed_self_host_owner.sh" \
    'generic_vessel_member_inferred_flow.pgy|vessel-member|42'
require_text "tests/self_hosted/parity/default_llvm_installed_self_host_owner.sh" \
    'src/self_hosted/mir_lower/fixture/generic_member_inferred_flow.pgy'
require_text "tests/self_hosted/parity/default_llvm_installed_self_host_owner.sh" \
    'generic_vessel_member_inferred_flow.pgy'
require_text "tests/self_hosted/parity/default_llvm_installed_self_host_owner.sh" \
    '$VESSEL_MEMBER_SOURCE|vessel-member|42'
for vessel_member_ratchet in \
    'Cell_Echo_Int(Cell *self' 'Cell _pgy_receiver_0 = {0};' \
    'Cell_Echo_Int(&(_pgy_receiver_0),' \
    'define internal i32 @Cell_Echo_Int(ptr %self' \
    '%box.field.0 = getelementptr inbounds %Cell' \
    'store i32 0, ptr %box.field.0' '@Cell_Echo_Int(ptr %box,' \
    host-kind-subject receiver-carriage-value-drift receiver-abi-forged \
    nested-use-drift 'two-routine classification is not single-shot'; do
    require_text \
        "tests/self_hosted/parity/one_mir_inferred_generic_member_projection.sh" \
        "$vessel_member_ratchet"
done
for vessel_member_emitter in \
    direct_mir_inferred_generic_member_c_emission_owner.pgy \
    direct_mir_inferred_generic_member_llvm_emission_owner.pgy; do
    require_text "src/self_hosted/compiler/$vessel_member_emitter" \
        'direct_mir_member_receiver_target_projection_owner.pgy'
    require_text "src/self_hosted/compiler/$vessel_member_emitter" \
        'DirectMirMemberReceiverTargetProjectionFromFacts('
done
require_text \
    "src/self_hosted/compiler/direct_mir_three_routine_projection_owner.pgy" \
    'classification.kind == DirectMirThreeRoutineGenericNominal()'
require_text \
    "src/self_hosted/compiler/direct_mir_generic_struct_value_flow_plan_owner.pgy" \
    'explicit_generic_scalar_calls_into_nominal_values_by_value'
require_text "tests/self_hosted/parity/default_llvm_installed_self_host_owner.sh" \
    'src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy'
require_text "tests/self_hosted/parity/default_llvm_installed_self_host_owner.sh" \
    'printf '\''7\n11\n5\n'\'''
require_text "tests/self_hosted/parity/default_c_compile_installed_self_host_owner.sh" \
    'src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy'
require_text "tests/self_hosted/parity/default_c_compile_installed_self_host_owner.sh" \
    'printf '\''7\n11\n5\n'\'''
require_text "src/compiler/llvm_runner.c" \
    "compiler_compile_link_self_host_llvm_artifact("
require_text "src/compiler/self_host_llvm_driver.c" \
    'producer_argv[1] = "--emit-mir-json-verified"'
require_text "src/compiler/self_host_llvm_driver.c" \
    'backend_argv[1] = "--mir-json-backend=llvm"'
forbid_text "src/compiler/self_host_llvm_driver.c" "driver_run_pipeline("
forbid_text "src/compiler/self_host_llvm_driver.c" \
    "compiler_build_native_llvm("
forbid_text "src/compiler/self_host_llvm_driver.c" \
    "compiler_runtime_object"
require_text "Makefile" \
    'ifneq ($(abspath $(PGY)),$(abspath $(REPO_BIN_DIR)/pgy$(EXEEXT)))'
require_text "tests/self_hosted/parity/self_host_compiler_build.sh" \
    'CODEGEN_BIN="${PGY_SELFHOST_CODEGEN_SEED:-$CODEGEN_BUILD/gen2.exe}"'
require_text "tests/self_hosted/parity/self_host_compiler_build.sh" \
    'PARSER_BIN="${PGY_SELFHOST_PARSER_SEED:-$CODEGEN_BUILD/parser_ast_producer.exe}"'
require_text "tests/self_hosted/parity/self_host_compiler_build.sh" \
    'Pergyra-built DRV-2 installed'
require_text "tests/self_hosted/parity/self_host_compiler_build.sh" \
    '"composed_ast=$(hash_file "$AST_FILE")"'
forbid_text "tests/self_hosted/parity/self_host_compiler_build.sh" \
    'PGY_SELFHOST_SOURCE_FINGERPRINT_FILE'
forbid_text "tests/self_hosted/parity/self_host_compiler_build.sh" \
    '"$PGY" "$DRIVER_SOURCE"'
require_text "tests/self_hosted/parity/driver_rung2_body_parity.sh" \
    'PREBUILT_DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"'
require_text "tests/self_hosted/parity/driver_rung2_body_parity.sh" \
    'if [[ "$backend" == "hard" ]]; then'
require_text "Makefile" "self-host-hard-driver-rung2-parity-test-smoke: self-host-compiler"
require_text "Makefile" "self-host-hard-driver-rung2-parity-full-test-smoke: self-host-compiler"
require_text "Makefile" "self-host-mir-abi-first-test-smoke"
require_text "Makefile" "self-host-compiler-world-contract-test-smoke"
require_text "Makefile" "self-host-preparation-contract-test-smoke"
require_text "Makefile" "self-host-preparation-parity-test-smoke"
require_text "Makefile" \
    "self-host-driver-bootstrap-test-smoke self-host-hard-driver-rung2-parity-test-smoke"
require_text "Makefile" "self-host-preparation-impact-test-smoke"
require_text "Makefile" "self-host-preparation-impact-changed-paths-test-smoke"
require_text "Makefile" "self-host-completeness-smoke"
require_text "Makefile" "self-host-completeness-incremental-cache-parity-test-smoke"
require_text "Makefile" "tests/self_host_hard_contract_smoke.sh"
require_text "Makefile" "tests/self_host_compiler_world_contract_smoke.sh"
require_text "Makefile" "tests/self_hosted/parity/completeness_ledger.sh"
require_text "Makefile" "tests/self_hosted/parity/completeness_incremental_cache_parity.sh"
require_text "Makefile" "self-host-preparation-test-smoke:"
require_text "tests/self_hosted/parity/completeness_incremental_cache_parity.sh" \
    "pgy_selfhost_read_test_harness_manifest"
require_text "tests/self_hosted/parity/completeness_incremental_cache_parity.sh" \
    "self-host-incremental-cache-parity"
require_text "src/self_hosted/compiler/incremental_fact_graph_owner.pgy" \
    "CompilerIncrementalCacheParitySuiteName"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "Normal compiler builds must not imply the heavy self-host parity bundle."
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "self-host-completeness-smoke"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "self-host-completeness-incremental-cache-parity-test-smoke"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "self-host-preparation-impact-test-smoke"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "self-host-preparation-impact-changed-paths-test-smoke"
require_file "src/self_hosted/compiler/completeness_ledger_owner.pgy"
require_file "tests/self_hosted/parity/completeness_ledger.sh"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessLedgerSchema"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessSourceInventory"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    'CompilerCompletenessPathContains(path, "_fixture/")'
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    'CompilerCompletenessPathContains(path, "_expected/")'
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessLexParsePassMin"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessLexParseSemanticPassMin"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "CompilerCompletenessFullPipelinePassMin"
require_text "src/self_hosted/compiler/completeness_ledger_owner.pgy" \
    "EmitCompilerCompletenessFullPipelineBaseline"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" \
    "EmitCompilerCompletenessSourceInventory();"
require_text "src/self_hosted/compiler/test_harness_manifest.pgy" \
    "EmitCompilerCompletenessFullPipelineBaseline();"
forbid_text "src/self_hosted/compiler/test_harness_manifest.pgy" \
    "EmitSelfHostCompletenessSources"
forbid_text "src/self_hosted/compiler/test_harness_manifest.pgy" \
    "EmitSelfHostCompletenessFullPipelineBaseline"
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "Out-of-subset codegen is a measured failure, not a skip."
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "pipeline identity regressed"
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "lex_parse_semantic"
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "PGY_SELFHOST_COMPLETENESS_STAGES"
require_text "tests/self_hosted/parity/completeness_ledger.sh" \
    "focused ledger ok"
require_text "docs/self_hosted/10_hard_self_host_contract.md" \
    "platform CI step lists must not set this variable directly."
for rel in \
    "Makefile" \
    "scripts/ci_linux_steps.sh" \
    "scripts/ci_windows_steps.sh" \
    "scripts/ci_macos_steps.sh"; do
    require_file "$rel"
    forbid_text "$rel" "PGY_SELFHOST_COMPLETENESS_STAGES"
done

active_stages=(lexer parser semantic codegen)
for stage in "${active_stages[@]}"; do
    require_file "src/self_hosted/$stage/main.pgy"
    require_file "src/self_hosted/$stage/intent.md"
    require_file "tests/self_hosted/parity/${stage}_parity.sh"
    require_text "Makefile" "self-host-${stage}-parity-test-smoke"
    require_text "Makefile" "tests/self_hosted/parity/${stage}_parity.sh"
    require_text "src/self_hosted/$stage/intent.md" "## Oracle"
done

for rel in \
    "tests/self_hosted/parity/codegen_bootstrap.sh" \
    "tests/self_hosted/parity/mir_json_parity.sh"; do
    require_file "$rel"
    require_text "$rel" "C oracle"
done

require_file "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh"
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" \
    "Pergyra-origin backend parity fuzz smoke"
require_text "tests/self_hosted/parity/fuzz_backend_parity_generator_parity.sh" \
    "generator parity ok"

require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "FIXPOINT"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "codegen compiles mir_lower"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "mir_lower_via_codegen.c"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "fuzz backend parity generator"
require_text "tests/self_hosted/parity/codegen_bootstrap.sh" "fuzz_generator_via_codegen.c"
require_text "tests/self_hosted/parity/mir_json_parity.sh" \
    "MIR JSON facts"
require_text "tests/self_hosted/parity/mir_json_parity.sh" \
    "must not read transitional ast compatibility text"
require_text "src/self_hosted/mir_lower/main.pgy" \
    "source_type"
require_text "src/self_hosted/mir_lower/main.pgy" \
    "source_locals"
forbid_text "src/self_hosted/mir_lower/main.pgy" \
    "JsonFieldString(json, kp, inst_end, \"\\\"ast\\\":\")"
forbid_text "src/self_hosted/mir_lower/main.pgy" \
    "StringLength(ast)"

mir_routine_owner_files=(
    "src/self_hosted/mir/routine_lower_owner.pgy"
    "src/self_hosted/mir/routine_assignment_owner.pgy"
    "src/self_hosted/mir/routine_control_transfer_owner.pgy"
    "src/self_hosted/mir/routine_if_owner.pgy"
    "src/self_hosted/mir/routine_while_owner.pgy"
    "src/self_hosted/mir/routine_for_owner.pgy"
    "src/self_hosted/mir/routine_tracked_statement_owner.pgy"
    "src/self_hosted/mir/routine_entry_owner.pgy"
)
for rel in "${mir_routine_owner_files[@]}"; do
    require_file "$rel"
    require_text "$rel" "ref input: SelfMirRoutineInput"
    if grep -Eq '^[[:space:]]+input: SelfMirRoutineInput,' "$ROOT_DIR/$rel"; then
        fail "$rel carries compiler-scale MIR input by value"
    fi
done
if grep -R -Fq -- "struct SelfMirRoutineState" \
    "$ROOT_DIR/src/self_hosted/mir"; then
    fail "self-host MIR reintroduced compiler-scale routine state carriage"
fi
require_file "src/self_hosted/mir/instruction_validation_owner.pgy"
require_text "src/self_hosted/mir/program_verify_owner.pgy" \
    "SelfMirAssignmentTargetGraphValidationError"
require_text "src/self_hosted/mir/program_verify_owner.pgy" \
    "valid_member, member_target_graph.graph"
require_text "src/self_hosted/mir/program_verify_owner.pgy" \
    "missing_member_base_use, member_target_graph.graph"
target_attach_count="$(grep -Fc -- \
    "SelfMirRoutineAttachLastSecondaryExpressionGraph(" \
    "$ROOT_DIR/src/self_hosted/mir/routine_assignment_owner.pgy")"
[[ "$target_attach_count" -eq 1 ]] ||
    fail "routine assignment target graph must be attached exactly once"
require_text "src/self_hosted/mir/routine_input_owner.pgy" \
    "target_binding_modes: Array<String>;"
require_function_text "src/self_hosted/mir/routine_assignment_owner.pgy" \
    "SelfMirLowerAssignmentFromArtifact" 'binding_mode == "owner_field"'
require_function_text "src/self_hosted/codegen/emission/assign_emit_owner.pgy" \
    "EmitAssign" 'LookupKindType(env, name, "cbind")'
forbid_function_text "src/self_hosted/codegen/emission/assign_emit_owner.pgy" \
    "EmitAssign" "CompilerSymbolCBindingName(name)"
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"src/self_hosted/mir_lower/fixture/owner_field_assignment.pgy"'
require_text "tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh" \
    '"uses":["balance.0"]'
require_text "tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh" \
    '"kind":"leaf","text":"amount"'
require_text "tests/self_hosted/parity/driver_rung2_owner_field_parity_owner.sh" \
    '"name":"missing_balance","type":"Int"'
require_function_text "src/self_hosted/parser/stmt_owner.pgy" \
    "ParseOneStmt" "ParserExpressionCallStatementKind(expr_fact)"
require_function_text "src/self_hosted/hir/ast_expression_owner_kind_binding.pgy" \
    "AstTreeArtifactBindExpressionOwnerKinds" \
    "kind == TypedAstKindUnknownTag()"
require_function_text "src/self_hosted/hir/ast_expression_owner_kind_binding.pgy" \
    "AstTreeArtifactBindExpressionOwnerKinds" \
    "UnwrapOption(provenance) == graph_text"
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/class_method_self_access/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/action_outcome_dispatch/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/aggregate_param_loop_phi/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/and_or_mix_chain_branches/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/arith_grand_total/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/arithmetic_overflow_check/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/array_avg_class/main.pgy"'
for array_mir_fixture in \
    array_avg_dev_chain \
    array_balanced_split \
    array_binary_search \
    array_cond_compound \
    array_count_above_avg \
    array_count_inversions \
    array_count_occurrences \
    array_count_ones_bits \
    array_count_pairs_sum \
    array_count_sorted_pairs \
    array_dedup_inplace \
    array_element_assign \
    array_enum \
    array_filter_count_sum \
    array_filter_into_new \
    array_filter_predicate_class \
    array_first_missing_positive \
    array_fold_minmax_sum \
    array_index_loop_sum \
    array_inline_access \
    array_inline_class_weighted \
    array_insertion_sort \
    array_kadane_max_subarray \
    array_min_max_combined \
    array_min_max_loop \
    array_minmax_pair \
    array_minmax_range \
    array_of_strings_loop \
    array_pair_concat_sort \
    array_partition_pivot \
    array_prefix_sum \
    array_remove_value \
    array_reverse_in_place \
    array_rotate_left \
    array_running_avg_int \
    array_running_avg_window \
    array_running_distinct_count \
    array_running_max \
    array_running_xor \
    array_selection_sort \
    array_set_in_place_memo \
    array_skip_pattern \
    array_sliding_diff \
    array_squeeze_zeros \
    array_sum_filtered \
    array_swap_pairs \
    array_swap_pos_neg \
    array_zero_out_evens \
    atom_charged_match \
    bank_fluent_chain \
    bank_interest_recursive \
    basic \
    bid_max_score \
    bin_push_chain \
    binary_search_int \
    binary_to_int \
    bitwise_via_division \
    bool_compound_predicates \
    bool_cursor_equivalence \
    bool_expr_chain \
    bool_ladder_chain \
    bool_logic_helpers \
    bool_negate_branch \
    bool_short_circuit_calls \
    bool_short_circuit_chain \
    bool_short_circuit_method \
    bool_state_toggle \
    bool_to_string_concat \
    break_continue \
    bubble_sort_inline; do
    require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
        "\"tests/cases/backend_compare/$array_mir_fixture/main.pgy\""
done
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/array_elem_class_literal/main.pgy"'
require_text "src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy" \
    '"tests/cases/backend_compare/array_elem_class_method/main.pgy"'
require_text "tests/self_hosted/parity/driver_rung2_call_target_parity_owner.sh" \
    'expected_members=(P_V)'
require_function_text "src/self_hosted/mir/routine_entry_owner.pgy" \
    "SelfMirRoutineFromInput" \
    "build, UnwrapOption(param_name), UnwrapOption(param_type), 0"
forbid_function_text "src/self_hosted/mir/routine_entry_owner.pgy" \
    "SelfMirRoutineFromInput" \
    "build, UnwrapOption(param_name), UnwrapOption(param_type), 1"
require_function_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" \
    "StringRuntimeCStringCoreBlock" "StringRuntimeCBoolToStringFn()"
require_function_text "src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy" \
    "RewriteSemanticDirectCall" 'UnwrapOption(argument_type) == "Bool"'
require_function_text "src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy" \
    "RewriteSemanticDirectCall" "StringRuntimeCBoolToStringFn()"
require_function_text \
    "src/self_hosted/semantic/ast_body_call_target_resolution_owner.pgy" \
    "SemanticAstAnalysisResolveCallTargetsFromAdmittedBody" \
    "SemanticAstBodyExpressionEnvironmentSeed("
require_function_text \
    "src/self_hosted/semantic/ast_body_expression_environment_owner.pgy" \
    "SemanticAstBodyExpressionEnvironmentSeed" \
    "SemanticAstIterationSeedVisibleBindings("
require_function_text "src/self_hosted/codegen/emission/stmt_emit.pgy" \
    "EmitLet" 'LookupKindType(env, type_name, "enum") != ""'
require_text "tests/self_hosted/parity/driver_rung2_enum_argument_parity_owner.sh" \
    "missing enum local owner was accepted"
require_text "src/self_hosted/parser/cursor_owner.pgy" \
    'ReadNumber("1000000", 0, cursor) == "1000000"'
forbid_text "src/self_hosted/parser/cursor_owner.pgy" \
    'return Concat("1e+", exp_text);'
require_text "tests/self_hosted/parity/driver_rung2_call_target_parity_owner.sh" \
    'expected_members=(Account_Deposit)'
require_function_text "src/self_hosted/mir_lower/stmt_render.pgy" \
    "RenderStmtFromFacts" 'source_type == "AST_CALL"'
require_function_text "src/self_hosted/mir_lower/stmt_render.pgy" \
    "RenderStmtFromFacts" 'return Concat("Call: ", value)'
require_function_text "src/self_hosted/hir/ast_text_inventory_owner.pgy" \
    "TypedAstTextKindOf" 'StartsWith(text, "Call: ")'
array_set_index_attach_count="$(grep -Fc -- \
    "SelfMirRoutineAttachLastSecondaryExpressionGraph(" \
    "$ROOT_DIR/src/self_hosted/mir/routine_tracked_statement_owner.pgy")"
[[ "$array_set_index_attach_count" -eq 1 ]] ||
    fail "ArraySet index graph must be attached exactly once"
for collection_kind in \
    "TypedAstKindArrayPopStmtTag()" \
    "TypedAstKindArrayPushStmtTag()" \
    "TypedAstKindArraySetStmtTag()"; do
    forbid_function_text "src/self_hosted/mir/routine_statement_owner.pgy" \
        "SelfMirSimpleStatementKind" "$collection_kind"
done

echo "[self-host-hard-contract] hard substitution contract is wired"
