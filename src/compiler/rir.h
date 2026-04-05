#ifndef PERGYRA_RIR_H
#define PERGYRA_RIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../parser/ast.h"

typedef struct RIRProgram RIRProgram;
typedef struct HIRProgram HIRProgram;

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
    RIR_RESOURCE_QUBIT_HANDLE,
    RIR_RESOURCE_REMOTE_FUTURE_HANDLE,
    RIR_RESOURCE_PROJECTION_OBJECT,
    RIR_RESOURCE_PROJECTION_DTO,
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
    RIR_STATE_SYNCED,
    RIR_STATE_DIRTY,
    RIR_STATE_DETACHED,
    RIR_STATE_PUBLISHED
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
    RIR_OP_AWAIT_REMOTE,
    RIR_OP_COMMIT_INTENT,
    RIR_OP_ABORT_INTENT,
    RIR_OP_COMPENSATE_INTENT_STEP
} RIROpKind;

typedef enum
{
    RIR_FLOW_NONE = 0,
    RIR_FLOW_AUTHORITY = 1 << 0,
    RIR_FLOW_PROJECTION = 1 << 1,
    RIR_FLOW_WORLD_HANDOFF = 1 << 2,
    RIR_FLOW_INVALIDATION = 1 << 3
} RIRFlowSemanticFlags;

typedef struct
{
    const char       *name;
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
    RIRResourceState  entry_state;
    RIRResourceState  exit_state;
    bool              merged_from_join;
    bool              widened_by_loop;
    bool              entry_conflict;
    bool              has_merge_conflict;
} RIRFlowFact;

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
    const char      *arg0;
    const char      *arg1;
    ASTNode         *ast;
} RIROp;

typedef struct
{
    size_t        id;
    RIRScopeKind  kind;
    const char   *owner_name;
    const char   *name;
    ASTNode      *ast;
    RIRFact      *facts;
    size_t        fact_count;
    RIROp        *ops;
    size_t        op_count;
    RIRStateSummary *state_summaries;
    size_t           state_summary_count;
    bool             has_state_errors;
    unsigned int     conservative_semantics;
    RIRFlowBlock    *flow_blocks;
    size_t           flow_block_count;
    bool             has_flow_sensitive_merge;
} RIRScope;

struct RIRProgram
{
    RIRScope *scopes;
    size_t    scope_count;
};

RIRProgram *rir_lower(ASTNode *annotated_ast, char **error_message);
bool        rir_enrich_with_hir_flow(RIRProgram *rir, const HIRProgram *hir, char **error_message);
bool        rir_validate(const RIRProgram *rir, char **error_message);
void        rir_destroy(RIRProgram *rir);
void        rir_dump(const RIRProgram *rir, FILE *out);

const char *rir_scope_kind_name(RIRScopeKind kind);
const char *rir_fact_kind_name(RIRFactKind kind);
const char *rir_resource_kind_name(RIRResourceKind kind);
const char *rir_resource_state_name(RIRResourceState state);
const char *rir_op_kind_name(RIROpKind kind);

#endif
