#ifndef PERGYRA_MIR_SOURCE_EMIT_PREDICATES_H
#define PERGYRA_MIR_SOURCE_EMIT_PREDICATES_H

bool mir_instruction_uses_source_statement_emit(const MIRInstruction *inst);
bool mir_instruction_uses_source_local_decl_emit(const MIRInstruction *inst);
bool mir_instruction_source_is_local_decl(const MIRInstruction *inst);
bool mir_instruction_source_is_local_destructure(const MIRInstruction *inst);
bool mir_instruction_source_is_assignment(const MIRInstruction *inst);
bool mir_instruction_source_is_defer_stmt(const MIRInstruction *inst);
bool mir_instruction_source_is_intent_step(const MIRInstruction *inst);
bool mir_source_node_type_is_cfg_container(ASTNodeType type);
bool mir_instruction_source_is_cfg_container(const MIRInstruction *inst);
bool mir_instruction_source_is_cfg_owned_control(const MIRInstruction *inst);
bool mir_instruction_source_stmt_has_side_effect_hint(const MIRInstruction *inst);
bool mir_instruction_source_stmt_residual_emit_is_allowed(const MIRInstruction *inst);
bool mir_instruction_source_stmt_reemit_is_redundant(const MIRInstruction *inst);
bool mir_instruction_source_stmt_call_emit_is_allowed(const MIRInstruction *inst);
bool mir_instruction_source_stmt_runtime_boundary_emit_is_allowed(const MIRInstruction *inst);
bool mir_instruction_resource_op_keeps_residual_statement_emit(const MIRInstruction *inst);
bool mir_instruction_has_inherent_concurrency_fact(const MIRInstruction *inst);

#endif
