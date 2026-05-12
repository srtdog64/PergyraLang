#include "mir.h"

#include "mir_cfg_contract_control.h"

#include <stdlib.h>
#include <string.h>

bool
mir_instruction_source_is_with_slot_claim(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && inst->name != NULL
        && strcmp(inst->name, "Claim") == 0
        && inst->source_ast_type == AST_WITH_STMT;
}

bool
mir_instruction_has_source_payload(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->ast != NULL
        && inst->has_source_location;
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
mir_instruction_source_ast_type_or(const MIRInstruction *inst,
                                   int fallback_type)
{
    if (!mir_instruction_has_source_location(inst))
        return fallback_type;
    return (int)inst->source_ast_type;
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
    if (inst == NULL || !inst->has_source_location)
        return false;
    if (inst->branch_shape == MIR_BRANCH_MATCH_CASE)
        return inst->source_ast_type == AST_MATCH_CASE;
    if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH)
        return inst->source_ast_type == AST_BLOCK;
    return true;
}

bool
mir_instruction_uses_source_statement_emit(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_DEF
        && inst->requires_source_statement_emit
        && inst->ast != NULL
        && inst->has_source_location;
}

bool
mir_instruction_uses_source_local_decl_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_source_statement_emit(inst)
        && inst->requires_source_local_decl_emit
        && inst->source_ast_type == AST_LET_DECL;
}

bool
mir_instruction_source_is_local_decl(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->has_source_location
        && inst->source_ast_type == AST_LET_DECL;
}

bool
mir_instruction_source_is_local_destructure(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->has_source_location
        && inst->source_ast_type == AST_LET_DESTRUCTURE;
}

bool
mir_instruction_source_is_assignment(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->has_source_location
        && inst->source_ast_type == AST_ASSIGNMENT;
}

bool
mir_instruction_source_is_defer_stmt(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->has_source_location
        && inst->source_ast_type == AST_DEFER_STMT;
}

bool
mir_instruction_source_is_intent_step(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->has_source_location
        && inst->source_ast_type == AST_INTENT_STEP;
}

bool
mir_source_ast_type_is_cfg_container(ASTNodeType type)
{
    switch (type) {
    case AST_WITH_STMT:
    case AST_UNSAFE_BLOCK:
    case AST_DEFER_STMT:
    case AST_IF_STMT:
    case AST_WHILE_LOOP:
    case AST_FOR_LOOP:
    case AST_SELECT_STMT:
    case AST_MATCH_STMT:
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_RETURN:
        return true;
    default:
        return false;
    }
}

bool
mir_instruction_source_is_cfg_container(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->has_source_location
        && mir_source_ast_type_is_cfg_container(inst->source_ast_type);
}

bool
mir_instruction_source_is_cfg_owned_control(const MIRInstruction *inst)
{
    if (inst == NULL)
        return false;
    if (inst->has_source_location)
        return mir_stmt_ast_type_is_cfg_owned_control(inst->source_ast_type);
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
mir_source_ast_type_stmt_has_side_effect_hint(ASTNodeType type,
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
mir_source_ast_stmt_has_side_effect_hint(const ASTNode *stmt)
{
    const char *callee = NULL;

    if (stmt == NULL)
        return false;
    if (stmt->type == AST_CALL
        && stmt->data.call.callee != NULL
        && stmt->data.call.callee->type == AST_IDENTIFIER) {
        callee = stmt->data.call.callee->data.identifier.name;
    }
    return mir_source_ast_type_stmt_has_side_effect_hint(stmt->type, callee);
}

bool
mir_instruction_source_stmt_has_side_effect_hint(const MIRInstruction *inst)
{
    if (inst == NULL || !inst->has_source_location)
        return false;
    return mir_source_ast_type_stmt_has_side_effect_hint(
        inst->source_ast_type, inst->arg0);
}

bool
mir_instruction_source_stmt_fallback_is_allowed(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return false;
    if (mir_instruction_is_intent_semantic_carrier(inst))
        return true;
    if (mir_instruction_source_payload(inst) == NULL
        || !inst->has_source_statement_index)
        return false;
    if (mir_instruction_source_is_cfg_owned_control(inst))
        return false;
    return mir_source_ast_type_stmt_has_side_effect_hint(
        inst->source_ast_type, inst->arg0);
}

bool
mir_instruction_source_matches_ast_type(const MIRInstruction *inst,
                                        ASTNodeType expected_type)
{
    return inst != NULL
        && inst->has_source_location
        && inst->source_ast_type == expected_type;
}

bool
mir_instruction_source_matches_ast_node(const MIRInstruction *inst,
                                        const ASTNode *node)
{
    return node != NULL
        && mir_instruction_source_matches_ast_type(inst, node->type);
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
