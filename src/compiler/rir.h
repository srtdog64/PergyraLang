#ifndef PERGYRA_RIR_H
#define PERGYRA_RIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "../parser/ast.h"

typedef struct RIRProgram RIRProgram;
typedef struct HIRProgram HIRProgram;
typedef struct DIRProgram DIRProgram;

typedef enum
{
    RIR_SCOPE_FUNCTION,
    RIR_SCOPE_METHOD,
    RIR_SCOPE_INTENT,
    RIR_SCOPE_ZONE,
    RIR_SCOPE_RELATION,
    RIR_SCOPE_EFFECT,
    RIR_SCOPE_WORLD
} RIRScopeKind;

typedef enum
{
    RIR_FACT_RESOURCE,
    RIR_FACT_PROJECTION,
    RIR_FACT_AUTHORITY,
    RIR_FACT_CAPABILITY,
    RIR_FACT_INTENT_POLICY
} RIRFactKind;

typedef enum
{
    RIR_RESOURCE_UNKNOWN,
    RIR_RESOURCE_LOCAL_SLOT,
    RIR_RESOURCE_SECURE_SLOT,
    RIR_RESOURCE_DEVICE_SLOT,
    RIR_RESOURCE_AUTHORITY_HANDLE,
    RIR_RESOURCE_CAPABILITY_TOKEN,
    RIR_RESOURCE_SUBJECT_SLOT,
    RIR_RESOURCE_OBJECT_SLOT,
    RIR_RESOURCE_TOBJECT_SLOT,
    RIR_RESOURCE_VESSEL_SLOT,
    RIR_RESOURCE_QUBIT_HANDLE,
    RIR_RESOURCE_LOCAL_FUTURE_HANDLE,
    RIR_RESOURCE_REMOTE_FUTURE_HANDLE,
    RIR_RESOURCE_PROJECTION_OBJECT,
    RIR_RESOURCE_PROJECTION_TOBJECT,
    RIR_RESOURCE_EFFECT_INSTANCE,
    RIR_RESOURCE_RELATION_INSTANCE,
    RIR_RESOURCE_ZONE_HANDLE,
    RIR_RESOURCE_WORLD_HANDLE
} RIRResourceKind;

typedef enum
{
    RIR_STATE_UNINIT,
    RIR_STATE_OWNED,
    RIR_STATE_BORROWED_READ,
    RIR_STATE_BORROWED_WRITE,
    RIR_STATE_MOVED,
    RIR_STATE_RELEASED,
    RIR_STATE_INVALID,
    RIR_STATE_MEASURED,
    RIR_STATE_REMOTE_PENDING,
    RIR_STATE_AUTHORIZED,
    RIR_STATE_AUTHORITY_LOST,
    RIR_STATE_SYNCED,
    RIR_STATE_DIRTY,
    RIR_STATE_STALE,
    RIR_STATE_DETACHED,
    RIR_STATE_PUBLISHED,
    RIR_STATE_HANDOFF_PENDING,
    RIR_STATE_HANDED_OFF,
    RIR_STATE_COMPENSATED
} RIRResourceState;

typedef enum
{
    RIR_OP_CLAIM,
    RIR_OP_READ,
    RIR_OP_WRITE,
    RIR_OP_RELEASE,
    RIR_OP_MOVE,
    RIR_OP_BORROW_READ,
    RIR_OP_BORROW_WRITE,
    RIR_OP_PROJECT_REFRESH,
    RIR_OP_PROJECT_PUBLISH,
    RIR_OP_ATTACH_EFFECT,
    RIR_OP_DETACH_EFFECT,
    RIR_OP_LINK_RELATION,
    RIR_OP_UNLINK_RELATION,
    RIR_OP_AUTHORIZE,
    RIR_OP_AWAIT_LOCAL,
    RIR_OP_AWAIT_REMOTE,
    RIR_OP_SPAWN,
    RIR_OP_ASYNC,
    RIR_OP_PARALLEL,
    RIR_OP_IO,
    RIR_OP_CHANNEL_SEND,
    RIR_OP_CHANNEL_RECV,
    RIR_OP_CHANNEL_SELECT,
    RIR_OP_COMMIT_INTENT,
    RIR_OP_ABORT_INTENT,
    RIR_OP_COMPENSATE_INTENT_STEP
} RIROpKind;

/* Machine-layer contact is an owner-directed semantic fact.  It deliberately
 * sits beside the generic RIR operation kind: the latter preserves resource
 * flow, while this tag records that an operation crosses the abstract machine
 * boundary and therefore needs the target manifest/lease/authority contract. */
typedef enum
{
    RIR_MACHINE_CONTACT_NONE = 0,
    RIR_MACHINE_CONTACT_CLAIM,
    RIR_MACHINE_CONTACT_READ,
    RIR_MACHINE_CONTACT_WRITE,
    RIR_MACHINE_CONTACT_RELEASE,
    RIR_MACHINE_CONTACT_SUBMIT_READ
} RIRMachineContactKind;

typedef enum
{
    RIR_FLOW_NONE = 0,
    RIR_FLOW_AUTHORITY = 1 << 0,
    RIR_FLOW_PROJECTION = 1 << 1,
    RIR_FLOW_WORLD_HANDOFF = 1 << 2,
    RIR_FLOW_INVALIDATION = 1 << 3,
    RIR_FLOW_AUTHORITY_LOSS = 1 << 4,
    RIR_FLOW_PROJECTION_INVALIDATION = 1 << 5
} RIRFlowSemanticFlags;

/* RIR-owned routine-local projection of the semantic ResourceFlowUniverse.
 * HIR is the lowering adapter; once enrichment succeeds, RIR consumers use
 * this validated identity table instead of reopening HIR rows. */
typedef struct
{
    size_t   stable_index;
    uint32_t declaration_syntax_id;
    uint32_t line;
    uint32_t column;
    uint32_t symbol_kind;
    bool     is_parameter;
    size_t   parameter_index;
    char    *name;
} RIRResourceFlowSymbol;

typedef struct
{
    const char       *name;
    const char       *slot_anchor;
    bool              has_flow_identity;
    size_t            stable_index;
    uint32_t          declaration_syntax_id;
    bool              is_parameter;
    size_t            parameter_index;
    RIRFactKind       origin_kind;
    RIRResourceKind   resource_kind;
    RIRResourceState  initial_state;
    RIRResourceState  final_state;
    const char       *last_op_name;
    bool              has_transition_error;
    ASTNode          *ast;
} RIRStateSummary;

typedef struct
{
    const char       *name;
    const char       *slot_anchor;
    bool              has_flow_identity;
    size_t            stable_index;
    uint32_t          declaration_syntax_id;
    bool              is_parameter;
    size_t            parameter_index;
    RIRResourceState  entry_state;
    RIRResourceState  exit_state;
    bool              merged_from_join;
    bool              widened_by_loop;
    bool              entry_conflict;
    bool              has_merge_conflict;
} RIRFlowFact;

typedef struct
{
    size_t   parameter_index;
    uint32_t mask;
} RIRFunctionParamFlowSummary;

typedef struct
{
    size_t        block_id;
    bool          is_reachable;
    bool          is_join;
    unsigned int  entry_semantics;
    unsigned int  exit_semantics;
    RIRFlowFact  *facts;
    size_t        fact_count;
} RIRFlowBlock;

typedef struct
{
    RIRFactKind      kind;
    const char      *name;
    const char      *slot_anchor;
    bool             has_flow_identity;
    size_t           stable_index;
    uint32_t         declaration_syntax_id;
    bool             is_parameter;
    size_t           parameter_index;
    const char      *arg0;
    const char      *arg1;
    RIRResourceKind  resource_kind;
    RIRResourceState state;
    ASTNode         *ast;
} RIRFact;

typedef struct
{
    RIROpKind        kind;
    const char      *subject;
    const char      *slot_anchor;
    const char      *arg0;
    const char      *arg1;
    RIRMachineContactKind machine_contact_kind;
    ASTNode         *ast;
    bool             has_source_statement_syntax_id;
    uint32_t         source_statement_syntax_id;
} RIROp;

typedef struct
{
    size_t        id;
    RIRScopeKind  kind;
    uint32_t      source_syntax_id;
    bool          resource_identity_verified;
    /* RIR-owned callable signature cardinality copied from HIR. */
    size_t        parameter_count;
    const char   *owner_name;
    const char   *name;
    ASTNode      *ast;
    RIRFact      *facts;
    size_t        fact_count;
    size_t        fact_capacity;
    RIROp        *ops;
    size_t        op_count;
    size_t        op_capacity;
    RIRStateSummary *state_summaries;
    size_t           state_summary_count;
    size_t           state_summary_capacity;
    RIRResourceFlowSymbol *resource_flow_symbols;
    size_t           resource_flow_symbol_count;
    size_t           resource_flow_symbol_capacity;
    RIRFunctionParamFlowSummary *function_param_flow_summaries;
    size_t           function_param_flow_summary_count;
    size_t           function_param_flow_summary_capacity;
    bool             has_state_errors;
    unsigned int     conservative_semantics;
    ASTNode         *program_root;
    RIRFlowBlock    *flow_blocks;
    size_t           flow_block_count;
    bool             has_flow_sensitive_merge;
} RIRScope;

struct RIRProgram
{
    RIRScope *scopes;
    size_t    scope_count;
    size_t    scope_capacity;
    ASTNode  *program_root;
};

typedef struct
{
    const RIRScope *scopes;
    size_t          count;
} RIRScopeInventory;

typedef struct
{
    RIRScope *scopes;
    size_t    count;
} RIRMutableScopeInventory;

RIRProgram *rir_lower(ASTNode *annotated_ast, char **error_message);
bool        rir_enrich_with_hir_flow(RIRProgram *rir, const HIRProgram *hir, char **error_message);
bool        rir_validate(const RIRProgram *rir, char **error_message);
bool        rir_validate_against_dir(const RIRProgram *rir, const DIRProgram *dir, char **error_message);
void        rir_destroy(RIRProgram *rir);
void        rir_dump(const RIRProgram *rir, FILE *out);
void        rir_dump_json(const RIRProgram *rir, FILE *out);
void        rir_scope_inventory_from_program(
                const RIRProgram *rir,
                RIRScopeInventory *inventory);
const RIRScope *rir_scope_inventory_get(
                const RIRScopeInventory *inventory,
                size_t index);
void        rir_mutable_scope_inventory_from_program(
                RIRProgram *rir,
                RIRMutableScopeInventory *inventory);
RIRScope   *rir_mutable_scope_inventory_get(
                const RIRMutableScopeInventory *inventory,
                size_t index);
RIRScopeKind rir_scope_kind(const RIRScope *scope);
const char *rir_scope_name(const RIRScope *scope);
const char *rir_scope_owner_name(const RIRScope *scope);
const char *rir_scope_display_name(const RIRScope *scope);
bool        rir_scope_has_state_errors(const RIRScope *scope);
size_t      rir_scope_fact_count(const RIRScope *scope);
const RIRFact *rir_scope_fact_at(const RIRScope *scope, size_t index);
size_t      rir_scope_op_count(const RIRScope *scope);
const RIROp *rir_scope_op_at(const RIRScope *scope, size_t index);
size_t      rir_scope_state_summary_count(const RIRScope *scope);
const RIRStateSummary *rir_scope_state_summary_at(const RIRScope *scope,
                                                  size_t index);
const RIRStateSummary *rir_scope_find_state_summary(const RIRScope *scope,
                                                    const char *name);
size_t      rir_scope_function_param_flow_summary_count(
                const RIRScope *scope);
const RIRFunctionParamFlowSummary *
            rir_scope_function_param_flow_summary_at(
                const RIRScope *scope,
                size_t index);
size_t      rir_scope_resource_flow_symbol_count(const RIRScope *scope);
const RIRResourceFlowSymbol *rir_scope_resource_flow_symbol_at(
                const RIRScope *scope,
                size_t index);
const RIRFact *rir_scope_find_fact_by_name_kind(const RIRScope *scope,
                                                RIRFactKind kind,
                                                const char *name);
const RIRFact *rir_scope_find_projection_fact(const RIRScope *scope,
                                              const char *name);
bool rir_scope_has_capability_fact(const RIRScope *scope,
                                   const char *participant,
                                   const char *ability);
unsigned int rir_scope_conservative_semantics(const RIRScope *scope);
size_t      rir_scope_flow_block_count(const RIRScope *scope);
const RIRFlowBlock *rir_scope_flow_block_at(const RIRScope *scope,
                                            size_t index);
size_t      rir_flow_block_id(const RIRFlowBlock *block);
bool        rir_flow_block_is_reachable(const RIRFlowBlock *block);
bool        rir_flow_block_is_join(const RIRFlowBlock *block);
unsigned int rir_flow_block_entry_semantics(const RIRFlowBlock *block);
unsigned int rir_flow_block_exit_semantics(const RIRFlowBlock *block);
size_t      rir_flow_block_fact_count(const RIRFlowBlock *block);
const RIRFlowFact *rir_flow_block_fact_at(const RIRFlowBlock *block,
                                          size_t index);

const char *rir_scope_kind_name(RIRScopeKind kind);
const char *rir_fact_kind_name(RIRFactKind kind);
const char *rir_resource_kind_name(RIRResourceKind kind);
const char *rir_resource_state_name(RIRResourceState state);
const char *rir_op_kind_name(RIROpKind kind);
const char *rir_machine_contact_kind_name(RIRMachineContactKind kind);
bool        rir_machine_contact_kind_is_present(RIRMachineContactKind kind);
const RIROp *rir_scope_find_op_by_ast(const RIRScope *scope,
                                      const ASTNode *ast);
RIRResourceState rir_merge_state_for_kind(RIRResourceKind kind,
                                          RIRResourceState a,
                                          RIRResourceState b,
                                          bool *conflict);

#endif
