#ifndef PERGYRA_MIR_H
#define PERGYRA_MIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "mir_program.h"

typedef struct SemanticResult SemanticResult;

/*
 * Versioned MIR-lowering admission contract.  The request is an in-process
 * ABI row, not a convenience wrapper: every native caller must enter through
 * the same protocol identity/version before MIR facts can be produced.
 */
#define PGY_MIR_LOWER_PROTOCOL_ID "pergyra.compiler-lowering-api"
#define PGY_MIR_LOWER_PROTOCOL_VERSION 1u

typedef struct
{
    const char             *protocol_id;
    uint32_t                protocol_version;
    const HIRProgram       *hir;
    const DIRProgram       *dir;
    const RIRProgram       *rir;
    const SemanticResult   *semantic;
} MIRLowerRequest;

void mir_lower_request_init(MIRLowerRequest *request,
                            const HIRProgram *hir,
                            const RIRProgram *rir,
                            const SemanticResult *semantic);
void mir_lower_request_bind_dir(MIRLowerRequest *request,
                                const DIRProgram *dir);
MIRProgram *mir_lower(const MIRLowerRequest *request,
                      char **error_message);
void        mir_instruction_capture_source_provenance(
                MIRInstruction *inst,
                const ASTNode *source);
void        mir_instruction_record_surface_usage(MIRInstruction *inst);
bool        mir_instruction_is_intent_stmt(const MIRInstruction *inst,
                                           const char *name);
bool        mir_instruction_is_with_slot_claim(const MIRInstruction *inst);
bool        mir_instruction_source_is_with_slot_claim(
                const MIRInstruction *inst);
bool        mir_instruction_source_is_with_slot_release(
                const MIRInstruction *inst);
bool        mir_instruction_resource_op_is_claim(const MIRInstruction *inst);
bool        mir_instruction_resource_op_is_read(const MIRInstruction *inst);
bool        mir_instruction_resource_op_is_write(const MIRInstruction *inst);
bool        mir_instruction_has_source_payload(const MIRInstruction *inst);
ASTNode    *mir_instruction_source_payload(const MIRInstruction *inst);
bool        mir_instruction_has_source_location(const MIRInstruction *inst);
int         mir_instruction_source_node_type_or(const MIRInstruction *inst,
                                               int fallback_type);
const char *mir_source_node_type_name(ASTNodeType type);
bool        mir_instruction_source_location_matches_node(
                const MIRInstruction *inst,
                const ASTNode *node);
bool        mir_instruction_source_line_matches_node(
                const MIRInstruction *inst,
                const ASTNode *node);
uint32_t    mir_instruction_source_line(const MIRInstruction *inst);
uint32_t    mir_instruction_source_column(const MIRInstruction *inst);
uint32_t    mir_instruction_source_stable_id(const MIRInstruction *inst);
const char *mir_instruction_source_inline_text(const MIRInstruction *inst);
bool        mir_instruction_has_source_terminator_kind(
                const MIRInstruction *inst);
bool        mir_instruction_source_terminator_matches(
                const MIRInstruction *inst,
                HIRBlockTerminatorKind expected_kind);
bool        mir_instruction_source_terminator_has_value(
                const MIRInstruction *inst);
bool        mir_instruction_has_source_statement_order(
                const MIRInstruction *inst);
bool        mir_instruction_is_first_source_statement(
                const MIRInstruction *inst);
size_t      mir_instruction_source_statement_index_or(
                const MIRInstruction *inst,
                size_t fallback_index);
int         mir_instruction_source_statement_order_compare(
                const MIRInstruction *left,
                const MIRInstruction *right);
bool        mir_instructions_share_source_statement(
                const MIRInstruction *left,
                const MIRInstruction *right);
bool        mir_instruction_consumes_resource_source(
                const MIRInstruction *resource,
                const MIRInstruction *consumer);
bool        mir_instruction_branch_requires_source_emit(
                const MIRInstruction *inst);
bool        mir_instruction_source_branch_payload_matches_shape(
                const MIRInstruction *inst);
bool        mir_instruction_has_required_branch_condition_fact(
                const MIRInstruction *inst);
bool        mir_instruction_has_required_source_branch_emit_fact(
                const MIRInstruction *inst);
bool        mir_instruction_has_required_branch_lowering_fact(
                const MIRInstruction *inst);
size_t      mir_instruction_match_pattern_count(const MIRInstruction *inst);
ASTNode    *mir_instruction_match_pattern_at(const MIRInstruction *inst,
                                             size_t index);
ASTNode    *mir_instruction_match_guard(const MIRInstruction *inst);
size_t      mir_instruction_match_binding_count(const MIRInstruction *inst);
const char *mir_instruction_match_binding_type_at(
                const MIRInstruction *inst, size_t index);
size_t      mir_instruction_destructure_binding_count(
                const MIRInstruction *inst);
const char *mir_instruction_destructure_binding_name_at(
                const MIRInstruction *inst,
                size_t index);
bool        mir_instruction_destructure_binding_index(
                const MIRInstruction *inst,
                const char *base_name,
                size_t *index_out);
#include "mir_source_emit_predicates.h"
#include "mir_source_lifecycle_shape.h"
bool        mir_source_node_type_stmt_has_side_effect_hint(
                ASTNodeType type,
                const char *callee_name);
bool        mir_instruction_source_matches_ast_type(
                const MIRInstruction *inst,
                ASTNodeType expected_type);
bool        mir_instruction_uses_channel_receive_statement_emit(
                const MIRInstruction *inst);
bool        mir_instruction_uses_select_receive_statement_emit(
                const MIRInstruction *inst);
bool        mir_block_has_hir_source_mapping(const MIRBasicBlock *block);
bool        mir_block_has_source_location(const MIRBasicBlock *block);
size_t      mir_block_source_hir_id(const MIRBasicBlock *block);
uint32_t    mir_block_source_line(const MIRBasicBlock *block);
uint32_t    mir_block_source_column(const MIRBasicBlock *block);
bool        mir_instruction_is_intent_semantic_carrier(
                const MIRInstruction *inst);
bool        mir_instruction_intent_step_matches(const MIRInstruction *inst,
                                                const char *step_name);
bool        mir_instruction_intent_phase_matches(const MIRInstruction *inst,
                                                 const char *phase_name);
const char *mir_instruction_intent_payload(const MIRInstruction *inst);
const char *mir_instruction_intent_step_name(const MIRInstruction *inst);
bool        mir_validate_intent_instruction_fact(const MIRRoutine *routine,
                                                 const MIRBasicBlock *block,
                                                 size_t block_index,
                                                 char **error_message);
void        mir_active_inventory(const MIRProgram *mir,
                                 ASTNodeType decl_type,
                                 ASTNode ***nodes_out,
                                 size_t *count_out);
void        mir_active_externs(const MIRProgram *mir,
                               ASTNode ***nodes_out,
                               size_t *count_out);
void        mir_routine_inventory_from_program(
                const MIRProgram *mir,
                MIRRoutineInventory *inventory);
const MIRRoutine *mir_routine_inventory_get(
                const MIRRoutineInventory *inventory,
                size_t index);
void        mir_decl_header_inventory_from_program(
                const MIRProgram *mir,
                MIRDeclHeaderInventory *inventory);
const MIRDeclHeader *mir_decl_header_inventory_get(
                const MIRDeclHeaderInventory *inventory,
                size_t index);
ASTNode    *mir_routine_source_decl(const MIRRoutine *routine);
ASTNode    *mir_routine_source_decl_of_type(const MIRRoutine *routine,
                                            MIRScopeKind expected_kind,
                                            ASTNodeType expected_ast_type);
MIRScopeKind mir_routine_kind(const MIRRoutine *routine);
const char *mir_routine_name(const MIRRoutine *routine);
const char *mir_routine_owner_name(const MIRRoutine *routine);
ASTNodeType mir_routine_owner_ast_type(const MIRRoutine *routine);
bool        mir_routine_has_signature(const MIRRoutine *routine);
size_t      mir_routine_generic_param_count(const MIRRoutine *routine);
const char *mir_routine_generic_param_name(const MIRRoutine *routine,
                                            size_t index);
size_t      mir_routine_param_count(const MIRRoutine *routine);
FuncParam  *mir_routine_param(const MIRRoutine *routine, size_t index);
const char *mir_routine_param_type_name(const MIRRoutine *routine,
                                        size_t index);
MIRParamCarriage mir_routine_param_carriage(const MIRRoutine *routine,
                                            size_t index);
MIRParamResourceKind mir_routine_param_resource_kind(
    const MIRRoutine *routine,
    size_t index);
bool mir_routine_param_passes_indirect(const MIRRoutine *routine,
                                       size_t index);
MIRParamCarriage mir_param_carriage_from_source_mode(ParamMode mode);
MIRParamResourceKind mir_param_resource_kind_from_type_name(
    const char *type_name);
const char *mir_param_carriage_name(MIRParamCarriage carriage);
const char *mir_param_resource_kind_name(MIRParamResourceKind kind);
ASTNode    *mir_routine_return_type(const MIRRoutine *routine);
const char *mir_routine_return_type_name(const MIRRoutine *routine);
const MIRCallableSig *mir_routine_param_callable_sig(const MIRRoutine *routine,
                                                    size_t index);
const MIRCallableSig *mir_routine_return_callable_sig(const MIRRoutine *routine);
size_t      mir_routine_function_param_flow_summary_count(
                const MIRRoutine *routine);
const MIRFunctionParamFlowSummary *
            mir_routine_function_param_flow_summary_at(
                const MIRRoutine *routine,
                size_t index);
const MIRIterationTypeFact *mir_routine_iteration_type_fact(
                const MIRRoutine *routine,
                uint32_t iteration_syntax_id);
/* Source-local binding type facts are materialized during MIR lowering so
 * backends do not rescan function bodies. */
const char *mir_routine_source_local_type_name(
    const MIRRoutine *routine, const char *local_name);
const MIRSourceLocalType *mir_routine_source_local_type_fact(
    const MIRRoutine *routine, const char *local_name);
size_t      mir_routine_source_local_type_count(const MIRRoutine *routine);
const char *mir_routine_source_local_name_at(const MIRRoutine *routine,
                                             size_t index);
const char *mir_routine_source_local_type_name_at(const MIRRoutine *routine,
                                                  size_t index);
const char *mir_routine_within_zone(const MIRRoutine *routine);
void        mir_mutable_routine_inventory_from_program(
                MIRProgram *mir,
                MIRMutableRoutineInventory *inventory);
MIRRoutine *mir_mutable_routine_inventory_get(
                const MIRMutableRoutineInventory *inventory,
                size_t index);
const MIRDeclHeader *mir_find_decl_header(const MIRProgram *mir, const char *name);
const MIRDeclHeader *mir_find_decl_header_of_type(
                const MIRProgram *mir,
                ASTNodeType ast_type,
                const char *name);
bool        mir_program_has_main_function(const MIRProgram *mir);
const char *mir_program_main_function_name(const MIRProgram *mir);
bool        mir_program_has_top_level_exec(const MIRProgram *mir);
bool        mir_run_liveness_pass(MIRProgram *mir, char **error_message);
bool        mir_run_dce_pass(MIRProgram *mir, char **error_message);
void        mir_refresh_non_cfg_body_fallback_inventory(MIRProgram *mir);
bool        mir_validate(const MIRProgram *mir, char **error_message);
bool        mir_validate_emission_topology(const MIRRoutine *routine,
                                          bool require_cleanup,
                                          bool require_cleanup_source_mapping,
                                          char **error_message);
bool        mir_validate_emission_contract(const MIRRoutine *routine,
                                          bool require_cleanup,
                                          bool require_cleanup_source_mapping,
                                          char **error_message);
void        mir_destroy(MIRProgram *mir);
void        mir_dump(const MIRProgram *mir, FILE *out);
/* JSON serialization of MIR facts (schema pgy.mir.v1). The rung-0 self-host
 * lowering path still carries a compatibility source-payload field through the
 * MIR source-shape owner; later consumers must not reopen raw instruction AST
 * payloads directly. */
void        mir_dump_json(const MIRProgram *mir, FILE *out);

const char *mir_scope_kind_name(MIRScopeKind kind);
const char *mir_inst_kind_name(MIRInstKind kind);
const char *mir_branch_shape_name(MIRBranchShape shape);

#endif
