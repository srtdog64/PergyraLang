#include "air_internal.h"
#include "io_boundary_builtin.h"
#include "../parser/ast_api.h"

typedef struct
{
    ASTNodeType     ast_type;
    AIRBoundaryKind kind;
    const char     *source_name;
} AIRAstBoundaryRule;

static const AIRAstBoundaryRule kAstBoundaryRules[] = {
    {AST_PARALLEL_BLOCK, AIR_BOUNDARY_PARALLEL, "parallel"},
    {AST_ASYNC_BLOCK, AIR_BOUNDARY_PARALLEL, "async"},
    {AST_SPAWN_EXPR, AIR_BOUNDARY_PARALLEL, "spawn"},
    {AST_AWAIT_EXPR, AIR_BOUNDARY_PARALLEL, "await"},
    {AST_CHANNEL_SEND, AIR_BOUNDARY_CHANNEL, "channel-send"},
    {AST_CHANNEL_RECV, AIR_BOUNDARY_CHANNEL, "channel-recv"},
    {AST_SELECT_STMT, AIR_BOUNDARY_CHANNEL, "select"},
    {AST_WITH_STMT, AIR_BOUNDARY_EXECUTION, "with"},
    {AST_UNSAFE_BLOCK, AIR_BOUNDARY_EXECUTION, "unsafe"},
    {AST_TRANSACTION_BLOCK, AIR_BOUNDARY_EXECUTION, "transaction"},
    {AST_DEFER_STMT, AIR_BOUNDARY_EXECUTION, "defer"},
    {AST_EVENT_SUBSCRIBE, AIR_BOUNDARY_EXECUTION, "event-subscribe"},
    {AST_EVENT_UNSUBSCRIBE, AIR_BOUNDARY_EXECUTION, "event-unsubscribe"},
};

bool
air_step_has_zone_boundary(const DIRIntentStep *step)
{
    return step != NULL && step->where_type_name != NULL;
}

bool
air_step_has_world_boundary(const DIRIntentStep *step)
{
    return step != NULL
        && (step->transfer_from_alias != NULL || step->transfer_to_alias != NULL);
}

static const char *
air_call_callee_name(const ASTNode *node)
{
    ASTNode *callee = ast_call_callee(node);
    if (callee == NULL)
        return NULL;
    if (callee->type == AST_IDENTIFIER)
        return ast_identifier_name(callee);
    if (callee->type == AST_MEMBER_ACCESS)
        return ast_member_name(callee);
    return NULL;
}

static const AIRAstBoundaryRule *
air_ast_boundary_rule_for_node(const ASTNode *node)
{
    if (node == NULL)
        return NULL;
    for (size_t i = 0; i < sizeof(kAstBoundaryRules) / sizeof(kAstBoundaryRules[0]); i++) {
        if (kAstBoundaryRules[i].ast_type == node->type)
            return &kAstBoundaryRules[i];
    }
    return NULL;
}

static bool
air_call_is_io_boundary(const ASTNode *node)
{
    const char *name = air_call_callee_name(node);
    return pgy_compiler_io_boundary_builtin_is_stable(name);
}

AIRBoundaryKind
air_boundary_kind_from_ast(const ASTNode *node)
{
    const AIRAstBoundaryRule *rule;

    if (node == NULL)
        return AIR_BOUNDARY_UNKNOWN;
    if (node->type == AST_BLOCK && ast_block_is_pin_block(node))
        return AIR_BOUNDARY_EXECUTION;
    if (node->type == AST_CALL)
        return air_call_is_io_boundary(node)
            ? AIR_BOUNDARY_IO
            : AIR_BOUNDARY_UNKNOWN;
    rule = air_ast_boundary_rule_for_node(node);
    return rule != NULL ? rule->kind : AIR_BOUNDARY_UNKNOWN;
}

AIRSyncClass
air_boundary_sync_from_kind(AIRBoundaryKind kind)
{
    switch (kind) {
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_CHANNEL:
        return AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_IO:
        return AIR_SYNC_EITHER;
    case AIR_BOUNDARY_EXECUTION:
    case AIR_BOUNDARY_ZONE:
        return AIR_SYNC_SYNC;
    /* World handoff is an async abstraction boundary by AIR contract. */
    case AIR_BOUNDARY_WORLD:
        return AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return AIR_SYNC_UNKNOWN;
    }
}

const char *
air_boundary_source_from_ast(const ASTNode *node)
{
    AIRBoundaryKind kind = air_boundary_kind_from_ast(node);
    const AIRAstBoundaryRule *rule;

    if (kind == AIR_BOUNDARY_IO) {
        const char *name = air_call_callee_name(node);
        return name != NULL ? name : "io";
    }
    if (kind == AIR_BOUNDARY_EXECUTION
        && node != NULL
        && node->type == AST_BLOCK
        && ast_block_is_pin_block(node)) {
        return "pin";
    }
    rule = air_ast_boundary_rule_for_node(node);
    if (rule != NULL)
        return rule->source_name;
    return kind == AIR_BOUNDARY_EXECUTION ? "execution" : "boundary";
}

bool
air_boundary_authority_storage_valid(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && (boundary->authority_name_count == 0
            || boundary->authority_names != NULL);
}

bool
air_boundary_required_ability_storage_valid(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && (boundary->required_ability_count == 0
            || boundary->required_abilities != NULL);
}

size_t
air_boundary_authority_name_count(const AIRBoundaryNode *boundary)
{
    return boundary != NULL ? boundary->authority_name_count : 0;
}

const char *
air_boundary_authority_name_at(const AIRBoundaryNode *boundary, size_t index)
{
    if (boundary == NULL || index >= boundary->authority_name_count)
        return NULL;
    return boundary->authority_names[index];
}

size_t
air_boundary_required_ability_count(const AIRBoundaryNode *boundary)
{
    return boundary != NULL ? boundary->required_ability_count : 0;
}

const char *
air_boundary_required_ability_at(const AIRBoundaryNode *boundary, size_t index)
{
    if (boundary == NULL || index >= boundary->required_ability_count)
        return NULL;
    return boundary->required_abilities[index];
}

const char *
air_boundary_first_authority_name_or(const AIRBoundaryNode *boundary,
                                     const char *fallback)
{
    const char *name = air_boundary_authority_name_at(boundary, 0);
    return !air_name_is_empty(name) ? name : fallback;
}

bool
air_boundary_declares_authority_name(const AIRBoundaryNode *boundary,
                                     const char *authority_name)
{
    if (boundary == NULL || authority_name == NULL)
        return false;
    for (size_t i = 0; i < air_boundary_authority_name_count(boundary); i++) {
        if (air_name_matches(
                air_boundary_authority_name_at(boundary, i),
                authority_name)) {
            return true;
        }
    }
    return false;
}

static bool
air_boundary_has_authority_contract(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && (boundary->authority_required
            || boundary->authority_from_zone
            || boundary->authority_from_action
            || air_boundary_authority_name_count(boundary) > 0);
}

AIRCompressionBudget
air_boundary_compression_budget(const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return AIR_COMPRESSION_UNKNOWN;
    switch (boundary->kind) {
    case AIR_BOUNDARY_ZONE:
        return air_boundary_has_authority_contract(boundary)
            ? AIR_COMPRESSION_RETAIN
            : AIR_COMPRESSION_SUMMARIZE;
    case AIR_BOUNDARY_WORLD:
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_IO:
    case AIR_BOUNDARY_CHANNEL:
    case AIR_BOUNDARY_EXECUTION:
        return AIR_COMPRESSION_RETAIN;
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return AIR_COMPRESSION_UNKNOWN;
    }
}

/*
 * The A/B bucket of a boundary's retain. A RETAIN boundary is runtime-visible
 * coordination/authority that no analysis can erase (INHERENT / bucket A); a
 * SUMMARIZE boundary is kept as a policy digest for traceability (POLICY /
 * bucket B). ERASE/FORBID/UNKNOWN retain nothing. The UNPROVEN (bucket C) cause
 * does not originate at a boundary - it is the lifecycle CHECK-guard residue,
 * counted program-wide via air_unproven_retain_count().
 */
AIRRetainCause
air_boundary_retain_cause(const AIRBoundaryNode *boundary)
{
    switch (air_boundary_compression_budget(boundary)) {
    case AIR_COMPRESSION_RETAIN:
        return AIR_RETAIN_CAUSE_INHERENT;
    case AIR_COMPRESSION_SUMMARIZE:
        return AIR_RETAIN_CAUSE_POLICY;
    case AIR_COMPRESSION_ERASE:
    case AIR_COMPRESSION_FORBID:
    case AIR_COMPRESSION_UNKNOWN:
    default:
        return AIR_RETAIN_CAUSE_NONE;
    }
}

const char *
air_boundary_compression_reason(const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return "missing boundary cannot be compressed";
    switch (boundary->kind) {
    case AIR_BOUNDARY_ZONE:
        return air_boundary_has_authority_contract(boundary)
            ? "authority-bearing zone boundary is runtime-visible"
            : "static zone contract is semantic provenance only";
    case AIR_BOUNDARY_WORLD:
        return "world/transfer boundary is runtime-visible coordination";
    case AIR_BOUNDARY_PARALLEL:
        return "parallel boundary is runtime-visible coordination";
    case AIR_BOUNDARY_IO:
        return "io boundary is runtime-visible";
    case AIR_BOUNDARY_CHANNEL:
        return "channel boundary is runtime-visible coordination";
    case AIR_BOUNDARY_EXECUTION:
        return "execution boundary is runtime-visible";
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return "unknown boundary kind cannot be compressed";
    }
}

static bool
air_intent_has_boundary_budget(const AIRProgram *air,
                               size_t intent_index,
                               AIRCompressionBudget budget)
{
    if (air == NULL)
        return false;
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        const AIRBoundaryNode *boundary = air_boundary_node_at(air, i);
        if (boundary != NULL
            && boundary->intent_index == intent_index
            && air_boundary_compression_budget(boundary) == budget) {
            return true;
        }
    }
    return false;
}

AIRCompressionBudget
air_intent_compression_budget(const AIRProgram *air, size_t intent_index)
{
    const AIRIntentNode *intent = air_intent_node_at(air, intent_index);
    if (intent == NULL)
        return AIR_COMPRESSION_UNKNOWN;
    if (air_intent_has_boundary_budget(air, intent_index, AIR_COMPRESSION_RETAIN))
        return AIR_COMPRESSION_RETAIN;
    if (intent->failure_class == AIR_FAILURE_FATAL
        || intent->failure_class == AIR_FAILURE_COMPENSABLE
        || intent->sync_class == AIR_SYNC_ASYNC
        || intent->sync_class == AIR_SYNC_EITHER) {
        return AIR_COMPRESSION_RETAIN;
    }
    if (intent->requires_from_action
        || intent->causes_from_action
        || air_intent_has_boundary_budget(air,
                                          intent_index,
                                          AIR_COMPRESSION_SUMMARIZE)) {
        return AIR_COMPRESSION_SUMMARIZE;
    }
    return AIR_COMPRESSION_ERASE;
}

const char *
air_intent_compression_reason(const AIRProgram *air, size_t intent_index)
{
    const AIRIntentNode *intent = air_intent_node_at(air, intent_index);
    if (intent == NULL)
        return "missing intent cannot be compressed";
    if (air_intent_has_boundary_budget(air, intent_index, AIR_COMPRESSION_RETAIN))
        return "runtime-visible boundary keeps the intent step";
    if (intent->failure_class == AIR_FAILURE_FATAL
        || intent->failure_class == AIR_FAILURE_COMPENSABLE)
        return "failure/effect policy keeps the intent step";
    if (intent->sync_class == AIR_SYNC_ASYNC
        || intent->sync_class == AIR_SYNC_EITHER)
        return "async/either sync policy keeps the intent step";
    if (intent->requires_from_action || intent->causes_from_action)
        return "action contract evidence is retained as summary";
    if (air_intent_has_boundary_budget(air,
                                       intent_index,
                                       AIR_COMPRESSION_SUMMARIZE))
        return "static boundary evidence is retained as summary";
    return "verified pure orchestration lowers to call sequence";
}
