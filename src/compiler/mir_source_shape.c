#include "mir.h"

#include "mir_cfg_contract_control.h"

#include <stdlib.h>
#include <string.h>

bool
mir_instruction_resource_op_is_claim(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && inst->name != NULL
        && strcmp(inst->name, "Claim") == 0;
}

bool
mir_instruction_resource_op_is_read(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && inst->name != NULL
        && strcmp(inst->name, "Read") == 0;
}

bool
mir_instruction_resource_op_is_write(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && inst->name != NULL
        && strcmp(inst->name, "Write") == 0;
}

bool
mir_instruction_source_matches_ast_type(const MIRInstruction *inst,
                                        ASTNodeType expected_type)
{
    return inst != NULL
        && inst->has_source_location
        && inst->source_node_type == expected_type;
}

bool
mir_instruction_source_is_with_slot_claim(const MIRInstruction *inst)
{
    return mir_instruction_resource_op_is_claim(inst)
        && mir_instruction_source_matches_ast_type(inst, AST_WITH_STMT);
}

bool
mir_instruction_has_source_payload(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->ast != NULL;
}

ASTNode *
mir_instruction_source_payload(const MIRInstruction *inst)
{
    return mir_instruction_has_source_payload(inst) ? inst->ast : NULL;
}

bool
mir_instruction_has_source_location(const MIRInstruction *inst)
{
    return inst != NULL && inst->has_source_location;
}

int
mir_instruction_source_node_type_or(const MIRInstruction *inst,
                                   int fallback_type)
{
    if (!mir_instruction_has_source_location(inst))
        return fallback_type;
    return (int)inst->source_node_type;
}

const char *
mir_source_node_type_name(ASTNodeType type)
{
    switch (type) {
    case AST_PROGRAM: return "AST_PROGRAM";
    case AST_BLOCK: return "AST_BLOCK";
    case AST_FUNC_DECL: return "AST_FUNC_DECL";
    case AST_CLASS_DECL: return "AST_CLASS_DECL";
    case AST_EXTERN_BLOCK: return "AST_EXTERN_BLOCK";
    case AST_LET_DECL: return "AST_LET_DECL";
    case AST_LET_DESTRUCTURE: return "AST_LET_DESTRUCTURE";
    case AST_TYPE_ALIAS: return "AST_TYPE_ALIAS";
    case AST_WITH_STMT: return "AST_WITH_STMT";
    case AST_PARALLEL_BLOCK: return "AST_PARALLEL_BLOCK";
    case AST_FOR_LOOP: return "AST_FOR_LOOP";
    case AST_WHILE_LOOP: return "AST_WHILE_LOOP";
    case AST_IF_STMT: return "AST_IF_STMT";
    case AST_RETURN: return "AST_RETURN";
    case AST_BREAK: return "AST_BREAK";
    case AST_CONTINUE: return "AST_CONTINUE";
    case AST_ENUM_DECL: return "AST_ENUM_DECL";
    case AST_SELECT_STMT: return "AST_SELECT_STMT";
    case AST_MATCH_STMT: return "AST_MATCH_STMT";
    case AST_MATCH_CASE: return "AST_MATCH_CASE";
    case AST_BINARY: return "AST_BINARY";
    case AST_UNARY: return "AST_UNARY";
    case AST_CALL: return "AST_CALL";
    case AST_MEMBER_ACCESS: return "AST_MEMBER_ACCESS";
    case AST_ARRAY_ACCESS: return "AST_ARRAY_ACCESS";
    case AST_ARRAY_LITERAL: return "AST_ARRAY_LITERAL";
    case AST_TUPLE_LITERAL: return "AST_TUPLE_LITERAL";
    case AST_MAP_LITERAL: return "AST_MAP_LITERAL";
    case AST_CAST: return "AST_CAST";
    case AST_TYPE_TEST: return "AST_TYPE_TEST";
    case AST_ASSIGNMENT: return "AST_ASSIGNMENT";
    case AST_AWAIT_EXPR: return "AST_AWAIT_EXPR";
    case AST_CHANNEL_SEND: return "AST_CHANNEL_SEND";
    case AST_CHANNEL_RECV: return "AST_CHANNEL_RECV";
    case AST_NUMBER: return "AST_NUMBER";
    case AST_STRING: return "AST_STRING";
    case AST_BOOLEAN: return "AST_BOOLEAN";
    case AST_IDENTIFIER: return "AST_IDENTIFIER";
    case AST_TYPE: return "AST_TYPE";
    case AST_CHANNEL_TYPE: return "AST_CHANNEL_TYPE";
    case AST_FUTURE_TYPE: return "AST_FUTURE_TYPE";
    case AST_ASYNC_BLOCK: return "AST_ASYNC_BLOCK";
    case AST_SPAWN_EXPR: return "AST_SPAWN_EXPR";
    case AST_TASK_GROUP: return "AST_TASK_GROUP";
    case AST_ABILITY_DECL: return "AST_ABILITY_DECL";
    case AST_ROLE_DECL: return "AST_ROLE_DECL";
    case AST_INCLUDE_STMT: return "AST_INCLUDE_STMT";
    case AST_REQUIRE_FIELD: return "AST_REQUIRE_FIELD";
    case AST_IMPL_ABILITY: return "AST_IMPL_ABILITY";
    case AST_OVERRIDE_FUNC: return "AST_OVERRIDE_FUNC";
    case AST_PARTY_DECL: return "AST_PARTY_DECL";
    case AST_ROLE_SLOT: return "AST_ROLE_SLOT";
    case AST_PARTY_SHARED: return "AST_PARTY_SHARED";
    case AST_PARTY_METHOD: return "AST_PARTY_METHOD";
    case AST_CONTEXT_ACCESS: return "AST_CONTEXT_ACCESS";
    case AST_PARTY_INSTANCE: return "AST_PARTY_INSTANCE";
    case AST_ROSTER_DECL: return "AST_ROSTER_DECL";
    case AST_SYSTEMIC_SLOT: return "AST_SYSTEMIC_SLOT";
    case AST_WORLD_DECL: return "AST_WORLD_DECL";
    case AST_WORLD_SYSTEMIC: return "AST_WORLD_SYSTEMIC";
    case AST_WORLD_ZONE: return "AST_WORLD_ZONE";
    case AST_WORLD_ACTIVATE: return "AST_WORLD_ACTIVATE";
    case AST_WORLD_DEACTIVATE: return "AST_WORLD_DEACTIVATE";
    case AST_WORLD_MAINTAIN: return "AST_WORLD_MAINTAIN";
    case AST_WORLD_STATE: return "AST_WORLD_STATE";
    case AST_INTENT_DECL: return "AST_INTENT_DECL";
    case AST_INTENT_INVOLVES: return "AST_INTENT_INVOLVES";
    case AST_INTENT_VALUE: return "AST_INTENT_VALUE";
    case AST_INTENT_STEP: return "AST_INTENT_STEP";
    case AST_RELATION_DECL: return "AST_RELATION_DECL";
    case AST_EFFECT_DECL: return "AST_EFFECT_DECL";
    case AST_ZONE_DECL: return "AST_ZONE_DECL";
    case AST_DOMAIN_SLOT: return "AST_DOMAIN_SLOT";
    case AST_ZONE_LAYER_SLOT: return "AST_ZONE_LAYER_SLOT";
    case AST_ZONE_APPLY: return "AST_ZONE_APPLY";
    case AST_ZONE_LINK: return "AST_ZONE_LINK";
    case AST_ZONE_DETACH: return "AST_ZONE_DETACH";
    case AST_ZONE_UNLINK: return "AST_ZONE_UNLINK";
    case AST_ZONE_REFRESH: return "AST_ZONE_REFRESH";
    case AST_ZONE_MAINTAIN_EFFECT: return "AST_ZONE_MAINTAIN_EFFECT";
    case AST_ZONE_MAINTAIN_RELATION: return "AST_ZONE_MAINTAIN_RELATION";
    case AST_ZONE_MAINTAIN_STATE: return "AST_ZONE_MAINTAIN_STATE";
    case AST_ZONE_AUTHORITY: return "AST_ZONE_AUTHORITY";
    case AST_ZONE_STATE: return "AST_ZONE_STATE";
    case AST_EVENT_DECL: return "AST_EVENT_DECL";
    case AST_EVENT_SUBSCRIBE: return "AST_EVENT_SUBSCRIBE";
    case AST_EVENT_UNSUBSCRIBE: return "AST_EVENT_UNSUBSCRIBE";
    case AST_EVENT_INVOKE: return "AST_EVENT_INVOKE";
    case AST_EVENT_HANDLER_TYPE: return "AST_EVENT_HANDLER_TYPE";
    case AST_LAMBDA_EXPR: return "AST_LAMBDA_EXPR";
    case AST_IMPORT_DECL: return "AST_IMPORT_DECL";
    case AST_USE_DECL: return "AST_USE_DECL";
    case AST_NAMESPACE_DECL: return "AST_NAMESPACE_DECL";
    case AST_UNSAFE_BLOCK: return "AST_UNSAFE_BLOCK";
    case AST_TRANSACTION_BLOCK: return "AST_TRANSACTION_BLOCK";
    case AST_DEFER_STMT: return "AST_DEFER_STMT";
    case AST_BIND_STMT: return "AST_BIND_STMT";
    }
    return "AST_UNKNOWN";
}

bool
mir_instruction_source_location_matches_node(const MIRInstruction *inst,
                                             const ASTNode *node)
{
    return mir_instruction_has_source_location(inst)
        && node != NULL
        && node->line != 0
        && inst->source_line == node->line
        && inst->source_column == node->column;
}

bool
mir_instruction_source_line_matches_node(const MIRInstruction *inst,
                                         const ASTNode *node)
{
    return mir_instruction_has_source_location(inst)
        && node != NULL
        && node->line != 0
        && inst->source_line == node->line;
}

uint32_t
mir_instruction_source_line(const MIRInstruction *inst)
{
    return mir_instruction_has_source_location(inst) ? inst->source_line : 0;
}

uint32_t
mir_instruction_source_column(const MIRInstruction *inst)
{
    return mir_instruction_has_source_location(inst) ? inst->source_column : 0;
}

uint32_t
mir_instruction_source_stable_id(const MIRInstruction *inst)
{
    return mir_instruction_has_source_location(inst)
        ? inst->source_stable_id
        : 0;
}

bool
mir_instruction_has_source_terminator_kind(const MIRInstruction *inst)
{
    return inst != NULL && inst->has_source_terminator_kind;
}

bool
mir_instruction_source_terminator_matches(
        const MIRInstruction *inst,
        HIRBlockTerminatorKind expected_kind)
{
    return mir_instruction_has_source_terminator_kind(inst)
        && inst->source_terminator_kind == expected_kind;
}

bool
mir_instruction_source_terminator_has_value(const MIRInstruction *inst)
{
    return inst != NULL && inst->source_terminator_has_value;
}

bool
mir_instruction_has_source_statement_order(const MIRInstruction *inst)
{
    return inst != NULL && inst->has_source_statement_index;
}

bool
mir_instruction_is_first_source_statement(const MIRInstruction *inst)
{
    return mir_instruction_has_source_statement_order(inst)
        && inst->source_statement_index == 0;
}

size_t
mir_instruction_source_statement_index_or(const MIRInstruction *inst,
                                          size_t fallback_index)
{
    return mir_instruction_has_source_statement_order(inst)
        ? inst->source_statement_index
        : fallback_index;
}

int
mir_instruction_source_statement_order_compare(const MIRInstruction *left,
                                               const MIRInstruction *right)
{
    bool left_has = mir_instruction_has_source_statement_order(left);
    bool right_has = mir_instruction_has_source_statement_order(right);

    if (left_has && right_has) {
        if (left->source_statement_index < right->source_statement_index)
            return -1;
        if (left->source_statement_index > right->source_statement_index)
            return 1;
        return 0;
    }
    if (left_has != right_has)
        return left_has ? -1 : 1;
    return 0;
}

bool
mir_instruction_branch_requires_source_emit(const MIRInstruction *inst)
{
    return inst != NULL
        && (inst->branch_shape == MIR_BRANCH_MATCH_CASE
            || inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH);
}

bool
mir_instruction_source_branch_payload_matches_shape(const MIRInstruction *inst)
{
    if (!mir_instruction_has_source_location(inst))
        return false;
    if (inst->branch_shape == MIR_BRANCH_MATCH_CASE)
        return mir_instruction_source_matches_ast_type(inst, AST_MATCH_CASE);
    if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH)
        return mir_instruction_source_matches_ast_type(inst, AST_BLOCK);
    return true;
}

bool
mir_instruction_has_required_branch_condition_fact(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_BRANCH)
        return false;
    if (mir_instruction_branch_requires_source_emit(inst))
        return mir_instruction_source_payload(inst) != NULL
            && mir_instruction_source_branch_payload_matches_shape(inst);
    return inst->expr0 != NULL;
}

bool
mir_instruction_uses_source_statement_emit(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_DEF
        && inst->requires_source_statement_emit
        && mir_instruction_source_payload(inst) != NULL;
}

bool
mir_instruction_uses_source_local_decl_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_source_statement_emit(inst)
        && inst->requires_source_local_decl_emit
        && mir_instruction_source_matches_ast_type(inst, AST_LET_DECL);
}

bool
mir_instruction_source_is_local_decl(const MIRInstruction *inst)
{
    return mir_instruction_source_matches_ast_type(inst, AST_LET_DECL);
}

bool
mir_instruction_source_is_local_destructure(const MIRInstruction *inst)
{
    return mir_instruction_source_matches_ast_type(inst, AST_LET_DESTRUCTURE);
}

bool
mir_instruction_source_is_assignment(const MIRInstruction *inst)
{
    return mir_instruction_source_matches_ast_type(inst, AST_ASSIGNMENT);
}

bool
mir_instruction_source_is_defer_stmt(const MIRInstruction *inst)
{
    return mir_instruction_source_matches_ast_type(inst, AST_DEFER_STMT);
}

bool
mir_instruction_source_is_intent_step(const MIRInstruction *inst)
{
    return mir_instruction_source_matches_ast_type(inst, AST_INTENT_STEP);
}

bool
mir_source_node_type_is_cfg_container(ASTNodeType type)
{
    return mir_stmt_ast_type_is_cfg_container(type);
}

bool
mir_instruction_source_is_cfg_container(const MIRInstruction *inst)
{
    int source_type = mir_instruction_source_node_type_or(inst, -1);

    return source_type >= 0
        && mir_source_node_type_is_cfg_container((ASTNodeType)source_type);
}

bool
mir_instruction_source_is_cfg_owned_control(const MIRInstruction *inst)
{
    int source_type;

    if (inst == NULL)
        return false;
    source_type = mir_instruction_source_node_type_or(inst, -1);
    if (source_type >= 0)
        return mir_stmt_ast_type_is_cfg_owned_control(
            (ASTNodeType)source_type);
    return mir_stmt_ast_is_cfg_owned_control(inst->ast);
}

static int
mir_string_pointer_compare(const void *key, const void *entry)
{
    const char *const *name = (const char *const *)key;
    const char *const *candidate = (const char *const *)entry;

    if (name == NULL || *name == NULL)
        return candidate != NULL && *candidate != NULL ? -1 : 0;
    if (candidate == NULL || *candidate == NULL)
        return 1;
    return strcmp(*name, *candidate);
}

static bool
mir_source_call_is_pure_query(const char *callee)
{
    static const char *const k_pure_query_builtins[] = {
        "ChannelCapacity",
        "ChannelClosed",
        "ChannelFull",
        "ChannelLength",
        "ChannelSpace",
        "HasLayer",
        "HasProjection",
        "HasState",
        "HasZone",
        "HasZoneLayer",
        "HasZoneProjection",
        "HasZoneState",
    };

    if (callee == NULL)
        return false;
    return bsearch(&callee,
                   k_pure_query_builtins,
                   sizeof(k_pure_query_builtins)
                       / sizeof(k_pure_query_builtins[0]),
                   sizeof(k_pure_query_builtins[0]),
                   mir_string_pointer_compare) != NULL;
}

bool
mir_source_node_type_stmt_has_side_effect_hint(ASTNodeType type,
                                              const char *callee_name)
{
    if (mir_stmt_ast_type_is_cfg_owned_control(type))
        return true;
    switch (type) {
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_SPAWN_EXPR:
    case AST_AWAIT_EXPR:
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
    case AST_EVENT_INVOKE:
    case AST_ASSIGNMENT:
    case AST_LET_DECL:
    case AST_LET_DESTRUCTURE:
    case AST_BIND_STMT:
    case AST_UNSAFE_BLOCK:
    case AST_TRANSACTION_BLOCK:
    case AST_DEFER_STMT:
    case AST_INTENT_STEP:
    case AST_WITH_STMT:
        return true;
    case AST_CALL:
        return !mir_source_call_is_pure_query(callee_name);
    default:
        return false;
    }
}

bool
mir_source_node_stmt_has_side_effect_hint(const ASTNode *stmt)
{
    const char *callee = NULL;

    if (stmt == NULL)
        return false;
    if (stmt->type == AST_CALL
        && ast_call_callee(stmt) != NULL
        && ast_call_callee(stmt)->type == AST_IDENTIFIER) {
        callee = ast_identifier_name(ast_call_callee(stmt));
    }
    return mir_source_node_type_stmt_has_side_effect_hint(stmt->type, callee);
}

bool
mir_instruction_source_stmt_has_side_effect_hint(const MIRInstruction *inst)
{
    int source_type = mir_instruction_source_node_type_or(inst, -1);

    if (source_type < 0)
        return false;
    return mir_source_node_type_stmt_has_side_effect_hint(
        (ASTNodeType)source_type, inst->arg0);
}

bool
mir_instruction_source_stmt_fallback_is_allowed(const MIRInstruction *inst)
{
    int source_type;

    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return false;
    if (mir_instruction_is_intent_semantic_carrier(inst))
        return true;
    if (mir_instruction_source_payload(inst) == NULL
        || !inst->has_source_statement_index)
        return false;
    if (mir_instruction_source_is_cfg_owned_control(inst))
        return false;
    source_type = mir_instruction_source_node_type_or(inst, -1);
    if (source_type == AST_LET_DECL
        || source_type == AST_LET_DESTRUCTURE
        || source_type == AST_ASSIGNMENT)
        return false;
    return mir_instruction_source_stmt_has_side_effect_hint(inst);
}

bool
mir_instruction_resource_op_keeps_residual_statement_emit(
        const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_RESOURCE_OP
        || inst->name == NULL)
        return false;
    return mir_instruction_resource_op_is_read(inst)
        || strcmp(inst->name, "IO") == 0
        || strcmp(inst->name, "ChannelSend") == 0
        || strcmp(inst->name, "ChannelRecv") == 0
        || strcmp(inst->name, "ChannelSelect") == 0;
}

bool
mir_instruction_uses_channel_receive_statement_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_source_statement_emit(inst)
        && inst->requires_channel_receive_statement_emit;
}

bool
mir_instruction_uses_select_receive_statement_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_channel_receive_statement_emit(inst)
        && inst->requires_select_receive_statement_emit;
}

bool
mir_block_has_hir_source_mapping(const MIRBasicBlock *block)
{
    return block != NULL && block->source_hir_block_id != SIZE_MAX;
}

bool
mir_block_has_source_location(const MIRBasicBlock *block)
{
    return block != NULL && block->has_source_location;
}

size_t
mir_block_source_hir_id(const MIRBasicBlock *block)
{
    return mir_block_has_hir_source_mapping(block)
        ? block->source_hir_block_id
        : SIZE_MAX;
}

uint32_t
mir_block_source_line(const MIRBasicBlock *block)
{
    return mir_block_has_source_location(block) ? block->source_line : 0;
}

uint32_t
mir_block_source_column(const MIRBasicBlock *block)
{
    return mir_block_has_source_location(block) ? block->source_column : 0;
}
