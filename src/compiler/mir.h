#ifndef PERGYRA_MIR_H
#define PERGYRA_MIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "hir.h"
#include "mir_abi.h"
#include "rir.h"
typedef struct MIRProgram MIRProgram;

typedef struct
{
    const char *name;
    size_t     *incoming_predecessors;
    size_t      incoming_predecessor_count;
} MIRSourcePhiNode;

typedef enum
{
    MIR_SCOPE_FUNCTION,
    MIR_SCOPE_METHOD,
    MIR_SCOPE_INTENT
} MIRScopeKind;

typedef enum
{
    MIR_INST_DEF,
    MIR_INST_RESOURCE_OP,
    MIR_INST_PHI,
    MIR_INST_BRANCH,
    MIR_INST_RETURN,
    MIR_INST_CLEANUP_EDGE,
    MIR_INST_LOOP_INIT,
    MIR_INST_DESTRUCTURE,
    MIR_INST_ASSIGN,
    MIR_INST_STMT
} MIRInstKind;

typedef enum
{
    MIR_BRANCH_EXPR,
    MIR_BRANCH_FOR_RANGE,
    MIR_BRANCH_FOR_IN,
    MIR_BRANCH_MATCH_CASE,
    MIR_BRANCH_SELECT_DISPATCH
} MIRBranchShape;

typedef enum
{
    MIR_LIFECYCLE_GUARD_NONE,
    MIR_LIFECYCLE_GUARD_SET,
    MIR_LIFECYCLE_GUARD_CHECK
} MIRLifecycleGuardKind;

typedef struct
{
    size_t      predecessor_block;
    const char *value_name;
} MIRPhiIncoming;

typedef struct
{
    ASTNode **items;
    size_t    count;
} MIRStatementInventory;

typedef struct
{
    size_t           id;
    MIRInstKind      kind;
    const char      *name;
    const char      *slot_anchor;
    /* Runtime owner slot for view-backed resource ops. The surface
     * slot_anchor remains matched to RIR for validation. */
    const char      *resource_owner_slot_anchor;
    bool             resource_owner_requires_metadata;
    const char      *arg0;
    const char      *arg1;
    const char      *result_name;
    const char     **uses;
    size_t           use_count;
    size_t           use_capacity;
    MIRPhiIncoming  *phi_incomings;
    size_t           phi_incoming_count;
    const RIROp     *rir_op;
    ASTNode         *ast;
    bool             has_source_location;
    uint32_t         source_line;
    uint32_t         source_column;
    uint32_t         source_stable_id;
    ASTNodeType      source_node_type;
    char            *source_inline_text;
    HIRBlockTerminatorKind source_terminator_kind;
    bool             has_source_terminator_kind;
    bool             source_terminator_has_value;
    size_t           source_statement_index;
    bool             has_source_statement_index;
    bool             has_surface_usage_facts;
    bool             uses_thread_pool_surface;
    bool             uses_intent_observability_surface;
    bool             requires_source_statement_emit;
    bool             requires_source_local_decl_emit;
    bool             requires_channel_receive_statement_emit;
    bool             requires_select_receive_statement_emit;
    bool             requires_source_branch_emit;
    MIRBranchShape   branch_shape;
    ASTNode         *match_case_pattern;
    ASTNode        **match_case_patterns;
    size_t           match_case_pattern_count;
    ASTNode         *match_case_guard;
    const char     **destructure_binding_names;
    size_t           destructure_binding_count;
    bool             has_lifecycle_guard_fact;
    MIRLifecycleGuardKind lifecycle_guard_kind;
    uint32_t         lifecycle_valid_mask;
    int              lifecycle_to_state;
    char            *lifecycle_receiver_name;
    char            *lifecycle_op;
    char            *lifecycle_subject;
    /* Lowering-owned speculation contract for expr0. Backends may only
     * if-convert when both facts are present and true. */
    bool             has_speculation_safety_fact;
    bool             speculation_is_pure;
    bool             speculation_is_non_trapping;
    /* Canonical ABI type name and layout: backends read these instead of
     * inventing layouts. abi_type_name remains populated for dynamic nominal
     * layouts such as Slot<Vec2> even when the static layout table has no
     * MIRTypeLayout entry. */
    ASTNode         *expr0;
    ASTNode         *expr1;
    const char      *abi_type_name;
    const MIRTypeLayout *type_layout;
} MIRInstruction;

typedef struct
{
    size_t           id;
    bool             is_entry;
    bool             is_reachable;
    bool             is_cleanup;
    bool             is_pin_region;
    bool             is_select_case_body;
    bool             pin_view_is_write;
    const char      *pin_source_name;
    const char      *pin_view_name;
    ASTNode         *pin_block_ast;
    size_t           source_hir_block_id;
    bool             has_source_location;
    uint32_t         source_line;
    uint32_t         source_column;
    MIRStatementInventory source_statement_inventory;
    const char     **source_local_defs;
    size_t           source_local_def_count;
    size_t          *source_dom_tree_children;
    size_t           source_dom_tree_child_count;
    MIRSourcePhiNode *source_phi_nodes;
    size_t           source_phi_node_count;
    size_t          *predecessors;
    size_t           predecessor_count;
    size_t           predecessor_capacity;
    size_t           succ_true;
    size_t           succ_false;
    bool             has_succ_true;
    bool             has_succ_false;
    size_t           cleanup_succ;
    bool             has_cleanup_succ;
    size_t           rollback_succ;
    bool             has_rollback_succ;
    size_t           invalidation_succ;
    bool             has_invalidation_succ;
    const char     **renamed_locals;
    size_t           renamed_local_count;
    size_t           renamed_local_capacity;
    const char     **ssa_entry_values;
    size_t           ssa_entry_value_count;
    size_t           ssa_entry_value_capacity;
    const char     **ssa_exit_values;
    size_t           ssa_exit_value_count;
    size_t           ssa_exit_value_capacity;
    const char     **use_names;
    size_t           use_name_count;
    size_t           use_name_capacity;
    const char     **def_names;
    size_t           def_name_count;
    size_t           def_name_capacity;
    const char     **live_in_names;
    size_t           live_in_name_count;
    size_t           live_in_name_capacity;
    const char     **live_out_names;
    size_t           live_out_name_count;
    size_t           live_out_name_capacity;
    size_t          *ssa_entry_versions;
    size_t          *ssa_exit_versions;
    size_t           ssa_version_count;
    MIRInstruction  *instructions;
    size_t           instruction_count;
    size_t           instruction_capacity;
} MIRBasicBlock;

typedef struct
{
    const char *name;
    const char *slot_anchor;
    size_t      def_block;
    size_t      def_inst;
    size_t      use_count;
    size_t      first_use_block;
    size_t      last_use_block;
    size_t      live_in_block_count;
    size_t      live_out_block_count;
    size_t      ast_write_count;
    bool        used_outside_def_block;
    bool        used_by_phi;
    bool        crosses_block_boundary;
    bool        has_ast_reassignment;
    bool        reaches_cleanup;
} MIRValueSummary;

typedef struct
{
    char *name;
    char *type_name;
    bool  is_callable;
    bool  is_closure_local; /* captured-lambda local: declared structurally,
                             * skipped by the SSA-locals pre-declaration */
    char *callable_return_type_name;
    char **callable_param_type_names;
    size_t callable_param_count;
} MIRSourceLocalType;

/*
 * Row 607 (SoT docs/125): lossless callable (EventHandler) routine-signature
 * payload. The flat param_type_names/return_type_name string cache cannot hold
 * a callable's nested return/param shape, so this structured descriptor carries
 * it. Populated during MIR signature lowering (the sanctioned AST-reading
 * owner) and consumed by C/LLVM signature emitters so routine callable
 * parameters/returns are MIR-owned, not reconstructed from source AST payloads.
 * is_callable=false means the slot is a plain type carried by param_type_names.
 */
typedef struct
{
    bool   is_callable;
    char  *return_type_name;        /* rendered MIR type name; NULL if void */
    char **param_type_names;        /* [param_count] rendered MIR type names */
    size_t param_count;
} MIRCallableSig;

typedef struct
{
    size_t             id;
    MIRScopeKind       kind;
    const char        *owner_name;
    ASTNodeType        owner_ast_type;
    const char        *name;
    ASTNode           *ast;
    bool               is_action_like;
    bool               has_signature;
    size_t             generic_param_count;
    FuncParam        **params;
    char             **param_type_names;
    MIRParamAbiFact   *param_abi_facts;
    size_t             param_count;
    /* Row 607: lossless callable signature payload (parallel to
     * param_type_names). param_callable_sigs[i].is_callable is true when
     * param i is an EventHandler; return_callable_sig.is_callable likewise.
     * NULL array / is_callable=false means the slot is a plain type. */
    MIRCallableSig    *param_callable_sigs;
    MIRCallableSig     return_callable_sig;
    ASTNode           *return_type;
    char              *return_type_name;
    const char        *within_zone;
    const HIRRoutine  *hir_routine;
    const RIRScope    *rir_scope;
    MIRBasicBlock     *blocks;
    size_t             block_count;
    size_t             block_capacity;
    size_t             entry_block;
    size_t             cleanup_block;
    bool               has_cleanup_block;
    size_t             rollback_block;
    bool               has_rollback_block;
    size_t             invalidation_block;
    bool               has_invalidation_block;
    size_t             instruction_count;
    size_t             cleanup_instruction_count;
    size_t             phi_inserted_count;
    size_t             renamed_value_count;
    size_t             cleanup_edge_count;
    size_t             use_edge_count;
    size_t             live_value_count;
    size_t             dce_removed_count;
    size_t             non_cfg_body_fallback_count;
    bool               used_non_cfg_body_fallback;
    bool               has_liveness;
    bool               has_dce;
    MIRSourceLocalType *source_local_types;
    size_t             source_local_type_count;
    size_t             source_local_type_capacity;
    MIRValueSummary   *value_summaries;
    size_t             value_summary_count;
    size_t             value_summary_capacity;
    bool               has_use_def_summary;
    /* Pass-local scratch arena: reused across MIR passes (SSA rename,
     * future liveness/DCE transforms).  Lifetime binds to the enclosing
     * MIRRoutine: initialised at construction, destroyed in mir_destroy().
     * Do NOT write to hir_routine->scratch from MIR passes; HIR is frozen
     * by the time MIR runs. */
    PgyArena           scratch;
} MIRRoutine;

typedef struct
{
    const MIRRoutine *routines;
    size_t            count;
} MIRRoutineInventory;

typedef struct
{
    MIRRoutine *routines;
    size_t      count;
} MIRMutableRoutineInventory;

#include "mir_decl.h"

struct MIRProgram
{
    MIRRoutine *routines;
    size_t      routine_count;
    size_t      routine_capacity;
    MIRDeclHeader *decl_headers;
    size_t      decl_header_count;
    size_t      decl_header_capacity;
    ASTNode   **externs;
    size_t      extern_count;
    ASTNode   **types;
    size_t      type_count;
    ASTNode   **abilities;
    size_t      ability_count;
    ASTNode   **roles;
    size_t      role_count;
    ASTNode   **parties;
    size_t      party_count;
    ASTNode   **rosters;
    size_t      roster_count;
    ASTNode   **worlds;
    size_t      world_count;
    ASTNode   **relations;
    size_t      relation_count;
    ASTNode   **effects;
    size_t      effect_count;
    ASTNode   **zones;
    size_t      zone_count;
    ASTNode   **events;
    size_t      event_count;
    ASTNode   **intents;
    size_t      intent_count;
    ASTNode   **functions;
    size_t      function_count;
    bool        has_inventory_surface_usage_facts;
    bool        inventory_uses_thread_pool_surface;
    bool        inventory_uses_intent_observability_surface;
    bool        has_non_cfg_body_fallback_inventory;
    size_t      non_cfg_body_fallback_total;
    size_t      non_cfg_body_fallback_routine_count;
    bool        has_top_level_exec;
    bool        has_main_function;
    const char *main_function_name;
    const char *source_path;  /* non-owning; NULL disables debug-line output */
};
MIRProgram *mir_lower(const HIRProgram *hir, const RIRProgram *rir, char **error_message);
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
size_t      mir_routine_param_count(const MIRRoutine *routine);
FuncParam  *mir_routine_param(const MIRRoutine *routine, size_t index);
const char *mir_routine_param_type_name(const MIRRoutine *routine,
                                        size_t index);
MIRParamCarriage mir_routine_param_carriage(const MIRRoutine *routine,
                                            size_t index);
bool mir_routine_param_passes_indirect(const MIRRoutine *routine,
                                       size_t index);
MIRParamCarriage mir_param_carriage_from_source_mode(ParamMode mode);
const char *mir_param_carriage_name(MIRParamCarriage carriage);
ASTNode    *mir_routine_return_type(const MIRRoutine *routine);
const char *mir_routine_return_type_name(const MIRRoutine *routine);
const MIRCallableSig *mir_routine_param_callable_sig(const MIRRoutine *routine,
                                                    size_t index);
const MIRCallableSig *mir_routine_return_callable_sig(const MIRRoutine *routine);
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

/* ABI Type Layout Lookup: backends use this instead of inventing layouts. */
const MIRTypeLayout *mir_abi_lookup(const char *pergyra_type_name);
void                 mir_abi_table_init(void);
#endif
