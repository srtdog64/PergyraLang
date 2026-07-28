#ifndef PERGYRA_MIR_TYPES_H
#define PERGYRA_MIR_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "hir.h"
#include "mir_abi.h"
#include "rir.h"
#include "../semantic/loop_flow_fact.h"
#include "../semantic/iteration_type_fact.h"
#include "../semantic/destructure_type_fact.h"
#include "../semantic/match_binding_type_fact.h"

typedef enum
{
    MIR_PARALLEL_CAPTURE_SNAPSHOT_COPY,
    MIR_PARALLEL_CAPTURE_JOIN_INDEX_DISJOINT,
    MIR_PARALLEL_CAPTURE_JOIN_READONLY
} MIRParallelCaptureDispositionKind;

typedef struct
{
    char *name;
    MIRParallelCaptureDispositionKind kind;
    size_t writer_task;
} MIRParallelCaptureDispositionRow;

typedef struct
{
    uint32_t source_stable_id;
    size_t task_count;
    bool sealed;
    MIRParallelCaptureDispositionRow *rows;
    size_t row_count;
} MIRParallelCaptureBoundaryFact;

typedef struct
{
    size_t  stable_index;
    uint32_t declaration_syntax_id;
    uint32_t line;
    uint32_t column;
    uint32_t symbol_kind;
    bool    is_parameter;
    size_t  parameter_index;
    char   *name;
} MIRResourceFlowSymbol;

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
    MIR_RECEIVER_CARRIAGE_NONE = 0,
    MIR_RECEIVER_CARRIAGE_VALUE,
    MIR_RECEIVER_CARRIAGE_MUTABLE_IDENTITY
} MIRReceiverCarriage;

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
    bool             has_source_statement_stable_id;
    uint32_t         source_statement_stable_id;
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
    const char     **match_binding_type_names;
    size_t           match_binding_type_count;
    const char     **destructure_binding_names;
    size_t           destructure_binding_count;
    const char      *destructure_element_type_name;
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
    /* Lowering-owned stable ABI layout identity.  Backends must not recover
     * layout identity from the table address or row position. */
    uint32_t         abi_layout_id;
    /* Lowering-owned runtime-call ABI row.  It is always present on the
     * resource operation and may be copied to an unambiguous DEF/STMT
     * consumer. Multi-operation expressions consume the exact resource row
     * through their stable source identity instead of collapsing rows here. */
    bool             resource_runtime_fact_present;
    MIRResourceRuntimeRow resource_runtime_fact;
    /* A slot definition can own more than one lowered runtime operation:
     * Claim establishes the container while slot sugar/auto-read emits a
     * concrete Write or Read from the same source definition.  Keep those
     * additional rows in the MIR-owned fact set instead of asking a backend
     * to reconstruct them from the global ABI vocabulary. */
    uint8_t          resource_runtime_aux_fact_count;
    MIRResourceRuntimeRow resource_runtime_aux_facts[4];
    const MIRTextBuilderRuntimeRow *text_builder_runtime_row;
    /* Owner-directed machine contact fact. Backends must not infer this
     * boundary from the source call or ABI type alone. */
    RIRMachineContactKind machine_contact_kind;
    /* The RIR owner has declared that this instruction crosses the machine
     * boundary. Validation uses this typed requirement, never an AST/source
     * spelling scan. */
    bool                 machine_layer_fact_required;
    bool                 machine_layer_fact_present;
    const char          *machine_layer_manifest_id;
    const char          *machine_layer_physical_grant_id;
    uint64_t             machine_layer_physical_base;
    uint64_t             machine_layer_physical_size;
    const char          *machine_layer_physical_mode;
    const char          *machine_layer_runtime_operation;
    bool                 machine_layer_hardware_adequate;
    bool                 machine_layer_authority_required;
    bool                 machine_layer_live_lease_required;
} MIRInstruction;

typedef struct
{
    size_t           id;
    bool             is_entry;
    bool             is_reachable;
    bool             is_cleanup;
    bool             is_intent_execution_plan_block;
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
    size_t   parameter_index;
    uint32_t mask;
} MIRFunctionParamFlowSummary;

typedef PgyIterationTypeFact MIRIterationTypeFact;
typedef PgyDestructureTypeFact MIRDestructureTypeFact;
typedef PgyMatchBindingTypeFact MIRMatchBindingTypeFact;

typedef enum
{
    MIR_INTENT_TERMINAL_SUCCESS = 1,
    MIR_INTENT_TERMINAL_FAILURE = 2
} MIRIntentTerminalRole;

typedef struct
{
    uint32_t transition_id;
    uint32_t expression_syntax_id;
    size_t   instruction_block_id;
    size_t   instruction_id;
    size_t   graph_root_id;
    uint32_t graph_digest;
    const char *call_target_name;
    uint32_t call_target_syntax_id;
    ASTNode *expression;
} MIRIntentCompensationFact;

typedef struct
{
    size_t      variant_index;
    const char *variant_name;
    const char *payload_name;
    const char *payload_type_name;
    size_t      successor_block_id;
} MIRIntentOutcomeBranchFact;

/* Fully materialized execution authority for one typed intent step.  A row
 * becomes visible to consumers only after every referenced instruction and
 * block has been materialized and cross-validated. */
typedef struct
{
    bool        sealed;
    uint32_t    transition_id;
    uint32_t    routine_syntax_id;
    uint32_t    step_syntax_id;
    const char *step_name;
    bool        has_predecessor;
    uint32_t    predecessor_transition_id;
    uint32_t    predecessor_step_syntax_id;
    const char *predecessor_step_name;
    uint32_t    action_syntax_id;
    size_t      outcome_instruction_block_id;
    size_t      outcome_instruction_id;
    const char *outcome_result_name;
    const char *outcome_type_name;
    const char *outcome_enum_name;
    uint32_t    outcome_enum_syntax_id;
    ASTNode    *outcome_expression;
    size_t      branch_block_id;
    size_t      branch_instruction_id;
    MIRIntentOutcomeBranchFact success;
    MIRIntentOutcomeBranchFact failure;
    size_t      completion_block_id;
    size_t      completion_instruction_id;
    MIRIntentCompensationFact *compensations;
    size_t      compensation_count;
} MIRIntentStepTransitionFact;

typedef struct
{
    bool        sealed;
    uint32_t    terminal_transition_id;
    uint32_t    routine_syntax_id;
    MIRIntentTerminalRole role;
    uint32_t    source_transition_id;
    uint32_t    source_step_syntax_id;
    const char *source_step_name;
    size_t      source_variant_index;
    const char *source_variant_name;
    const char *source_payload_name;
    const char *source_payload_type_name;
    size_t      result_instruction_block_id;
    size_t      result_instruction_id;
    const char *result_definition_name;
    const char *result_type_name;
    const char *result_enum_name;
    uint32_t    result_enum_syntax_id;
    size_t      result_variant_index;
    const char *result_variant_name;
    const char *result_payload_name;
    const char *result_payload_type_name;
    uint32_t    expression_syntax_id;
    size_t      graph_root_id;
    uint32_t    graph_digest;
    ASTNode    *expression;
} MIRIntentTerminalTransitionFact;

typedef struct
{
    size_t             id;
    MIRScopeKind       kind;
    const char        *owner_name;
    ASTNodeType        owner_ast_type;
    MIRReceiverCarriage receiver_carriage;
    const char        *name;
    ASTNode           *ast;
    bool               is_action_like;
    bool               has_signature;
    size_t             generic_param_count;
    char             **generic_param_names;
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
    uint32_t           source_syntax_id;
    MIRResourceFlowSymbol *resource_flow_symbols;
    size_t             resource_flow_symbol_count;
    size_t             resource_flow_symbol_capacity;
    MIRFunctionParamFlowSummary *function_param_flow_summaries;
    size_t             function_param_flow_summary_count;
    size_t             function_param_flow_summary_capacity;
    PgyLoopFlowSummaryFact *loop_flow_summaries;
    size_t             loop_flow_summary_count;
    size_t             loop_flow_summary_capacity;
    PgyLoopFlowStateFact *loop_flow_states;
    size_t             loop_flow_state_count;
    size_t             loop_flow_state_capacity;
    MIRIterationTypeFact *iteration_type_facts;
    size_t             iteration_type_fact_count;
    size_t             iteration_type_fact_capacity;
    MIRDestructureTypeFact *destructure_type_facts;
    size_t             destructure_type_fact_count;
    size_t             destructure_type_fact_capacity;
    MIRMatchBindingTypeFact *match_binding_type_facts;
    size_t             match_binding_type_fact_count;
    size_t             match_binding_type_fact_capacity;
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
    bool               intent_execution_plan_admitted;
    uint32_t           intent_execution_plan_digest;
    MIRIntentStepTransitionFact *intent_step_transitions;
    size_t             intent_step_transition_count;
    size_t             intent_step_transition_capacity;
    MIRIntentTerminalTransitionFact *intent_terminal_transitions;
    size_t             intent_terminal_transition_count;
    size_t             intent_terminal_transition_capacity;
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

#endif /* PERGYRA_MIR_TYPES_H */
