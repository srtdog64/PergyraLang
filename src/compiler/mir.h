#ifndef PERGYRA_MIR_H
#define PERGYRA_MIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "hir.h"
#include "rir.h"

typedef struct MIRProgram MIRProgram;

/* =================================================================
 * ABI Type Layout: explicit memory layout for MIR instructions
 *
 * This is the bridge between the Pergyra type system and the
 * C/LLVM backends. Instead of backends inventing their own struct
 * layouts, they read the layout from here.
 *
 * Source: src/runtime/pgy_abi_spec.h
 * ================================================================= */

#define MIR_MAX_TYPE_FIELDS 8

typedef struct
{
    const char *field_name;
    uint32_t    offset;      /* byte offset from struct start */
    uint32_t    field_size;
    uint32_t    field_align;
} MIRFieldLayout;

typedef struct
{
    const char *name;
    size_t     *incoming_predecessors;
    size_t      incoming_predecessor_count;
} MIRSourcePhiNode;

typedef struct
{
    const char      *abi_type_name;    /* Canonical surface type, e.g. "Slot<Int>" */
    uint32_t         size_bytes;
    uint32_t         align_bytes;
    uint16_t         field_count;
    MIRFieldLayout   fields[MIR_MAX_TYPE_FIELDS];
    const char      *runtime_fn;       /* e.g. "pgy_claim_Int" */
    const char      *inner_c_type;     /* e.g. "int32_t" for Slot<Int> */
} MIRTypeLayout;

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
    ASTNodeType      source_ast_type;
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
    /* ABI type layout: backends read this instead of inventing layouts. */
    ASTNode         *expr0;
    ASTNode         *expr1;
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
    size_t             id;
    MIRScopeKind       kind;
    const char        *owner_name;
    ASTNodeType        owner_ast_type;
    const char        *name;
    ASTNode           *ast;
    bool               is_action_like;
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
    /* Source compatibility/provenance only; declaration inventory lives below. */
    ASTNode    *source_ast;
    const char *owner_name;
    const char *name;
    FuncParam **params;
    size_t      param_count;
    ASTNode    *return_type;
    bool        is_action_like;
    const char *within_zone;
    bool        has_routine;
    size_t      routine_index;
} MIRDeclMethod;

typedef struct
{
    /* Source compatibility/provenance only; declaration inventory lives below. */
    ASTNode     *source_ast;
    ASTNodeType  ast_type;
    const char  *name;
    size_t       method_count;
    MIRDeclMethod *method_metadata;
    size_t       method_metadata_count;
    bool         uses_pointer_self;
} MIRDeclHeader;

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
};

MIRProgram *mir_lower(const HIRProgram *hir, const RIRProgram *rir, char **error_message);
void        mir_instruction_record_surface_usage(MIRInstruction *inst);
bool        mir_instruction_is_intent_stmt(const MIRInstruction *inst,
                                           const char *name);
bool        mir_instruction_is_with_slot_claim(const MIRInstruction *inst);
bool        mir_instruction_source_is_with_slot_claim(
                const MIRInstruction *inst);
bool        mir_instruction_has_source_payload(const MIRInstruction *inst);
ASTNode    *mir_instruction_source_payload(const MIRInstruction *inst);
bool        mir_instruction_has_source_location(const MIRInstruction *inst);
int         mir_instruction_source_ast_type_or(const MIRInstruction *inst,
                                               int fallback_type);
const char *mir_source_ast_type_name(ASTNodeType type);
bool        mir_instruction_source_location_matches_node(
                const MIRInstruction *inst,
                const ASTNode *node);
bool        mir_instruction_source_line_matches_node(
                const MIRInstruction *inst,
                const ASTNode *node);
uint32_t    mir_instruction_source_line(const MIRInstruction *inst);
uint32_t    mir_instruction_source_column(const MIRInstruction *inst);
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
bool        mir_instruction_branch_requires_source_emit(
                const MIRInstruction *inst);
bool        mir_instruction_source_branch_payload_matches_shape(
                const MIRInstruction *inst);
bool        mir_instruction_has_required_branch_condition_fact(
                const MIRInstruction *inst);
bool        mir_instruction_uses_source_statement_emit(
                const MIRInstruction *inst);
bool        mir_instruction_uses_source_local_decl_emit(
                const MIRInstruction *inst);
bool        mir_instruction_source_is_local_decl(const MIRInstruction *inst);
bool        mir_instruction_source_is_local_destructure(
                const MIRInstruction *inst);
bool        mir_instruction_source_is_assignment(const MIRInstruction *inst);
bool        mir_instruction_source_is_defer_stmt(const MIRInstruction *inst);
bool        mir_instruction_source_is_intent_step(const MIRInstruction *inst);
bool        mir_source_ast_type_is_cfg_container(ASTNodeType type);
bool        mir_instruction_source_is_cfg_container(const MIRInstruction *inst);
bool        mir_instruction_source_is_cfg_owned_control(
                const MIRInstruction *inst);
bool        mir_instruction_source_stmt_has_side_effect_hint(
                const MIRInstruction *inst);
bool        mir_instruction_source_stmt_fallback_is_allowed(
                const MIRInstruction *inst);
bool        mir_source_ast_type_stmt_has_side_effect_hint(
                ASTNodeType type,
                const char *callee_name);
bool        mir_source_ast_stmt_has_side_effect_hint(const ASTNode *stmt);
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
ASTNode     *mir_find_function_decl(const MIRProgram *mir, const char *name);
const MIRDeclHeader *mir_find_decl_header(const MIRProgram *mir, const char *name);
bool        mir_run_liveness_pass(MIRProgram *mir, char **error_message);
bool        mir_run_dce_pass(MIRProgram *mir, char **error_message);
void        mir_refresh_non_cfg_body_fallback_inventory(MIRProgram *mir);
bool        mir_validate(const MIRProgram *mir, char **error_message);
bool        mir_validate_emission_topology(const MIRRoutine *routine,
                                          bool require_cleanup,
                                          bool require_cleanup_source_mapping,
                                          char **error_message);
void        mir_destroy(MIRProgram *mir);
void        mir_dump(const MIRProgram *mir, FILE *out);

const char *mir_scope_kind_name(MIRScopeKind kind);
const char *mir_inst_kind_name(MIRInstKind kind);
const char *mir_branch_shape_name(MIRBranchShape shape);

/* ABI Type Layout Lookup: backends use this instead of inventing layouts. */
const MIRTypeLayout *mir_abi_lookup(const char *pergyra_type_name);
void                 mir_abi_table_init(void);

#endif
