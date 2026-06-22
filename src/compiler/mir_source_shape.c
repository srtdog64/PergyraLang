#include "mir.h"

#include "mir_cfg_contract_control.h"

#include "../parser/ast_api.h"

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

void
mir_instruction_capture_source_provenance(MIRInstruction *inst,
                                          const ASTNode *source)
{
    if (inst == NULL || source == NULL)
        return;
    char *inline_text = ast_capture_inline((ASTNode *)source);
    free(inst->source_inline_text);
    inst->source_inline_text = inline_text;
    inst->has_source_location = true;
    inst->source_line = source->line;
    inst->source_column = source->column;
    inst->source_stable_id = ast_node_stable_id(source);
    inst->source_node_type = source->type;
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

const char *
mir_instruction_source_inline_text(const MIRInstruction *inst)
{
    return inst != NULL ? inst->source_inline_text : NULL;
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
mir_instructions_share_source_statement(const MIRInstruction *left,
                                        const MIRInstruction *right)
{
    return mir_instruction_has_source_statement_order(left)
        && mir_instruction_has_source_statement_order(right)
        && left->source_statement_index == right->source_statement_index;
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
    if (inst->branch_shape == MIR_BRANCH_MATCH_CASE
        && inst->expr0 == NULL)
        return false;
    if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH)
        return inst->expr0 != NULL;
    if (mir_instruction_branch_requires_source_emit(inst))
        return true;
    return inst->expr0 != NULL;
}

bool
mir_instruction_has_required_source_branch_emit_fact(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_BRANCH)
        return false;
    if (!mir_instruction_branch_requires_source_emit(inst))
        return true;
    if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH)
        return inst->expr0 != NULL
            && mir_instruction_source_branch_payload_matches_shape(inst);
    if (inst->branch_shape == MIR_BRANCH_MATCH_CASE
        && mir_instruction_match_pattern_count(inst) == 0)
        return false;
    return mir_instruction_source_branch_payload_matches_shape(inst);
}

bool
mir_instruction_has_required_branch_lowering_fact(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_BRANCH)
        return false;
    if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH)
        return mir_instruction_has_required_source_branch_emit_fact(inst);
    return mir_instruction_has_required_branch_condition_fact(inst)
        && mir_instruction_has_required_source_branch_emit_fact(inst);
}

size_t
mir_instruction_match_pattern_count(const MIRInstruction *inst)
{
    if (inst == NULL || inst->branch_shape != MIR_BRANCH_MATCH_CASE)
        return 0;
    if (inst->match_case_pattern_count > 0)
        return inst->match_case_pattern_count;
    return inst->match_case_pattern != NULL ? 1 : 0;
}

ASTNode *
mir_instruction_match_pattern_at(const MIRInstruction *inst, size_t index)
{
    if (inst == NULL || index >= mir_instruction_match_pattern_count(inst))
        return NULL;
    if (inst->match_case_pattern_count > 0)
        return inst->match_case_patterns != NULL
            ? inst->match_case_patterns[index]
            : NULL;
    return index == 0 ? inst->match_case_pattern : NULL;
}

ASTNode *
mir_instruction_match_guard(const MIRInstruction *inst)
{
    return inst != NULL && inst->branch_shape == MIR_BRANCH_MATCH_CASE
        ? inst->match_case_guard
        : NULL;
}

size_t
mir_instruction_destructure_binding_count(const MIRInstruction *inst)
{
    return inst != NULL && inst->kind == MIR_INST_DESTRUCTURE
        ? inst->destructure_binding_count
        : 0;
}

const char *
mir_instruction_destructure_binding_name_at(const MIRInstruction *inst,
                                            size_t index)
{
    if (inst == NULL
        || inst->kind != MIR_INST_DESTRUCTURE
        || index >= inst->destructure_binding_count
        || inst->destructure_binding_names == NULL) {
        return NULL;
    }
    return inst->destructure_binding_names[index];
}

bool
mir_instruction_destructure_binding_index(const MIRInstruction *inst,
                                          const char *base_name,
                                          size_t *index_out)
{
    if (inst == NULL || inst->kind != MIR_INST_DESTRUCTURE
        || base_name == NULL || index_out == NULL) {
        return false;
    }
    for (size_t i = 0; i < inst->destructure_binding_count; i++) {
        const char *name =
            mir_instruction_destructure_binding_name_at(inst, i);
        if (name != NULL && strcmp(name, base_name) == 0) {
            *index_out = i;
            return true;
        }
    }
    return false;
}

bool
mir_instruction_uses_source_statement_emit(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_DEF
        && inst->requires_source_statement_emit
        && mir_instruction_has_source_location(inst)
        && inst->expr0 != NULL;
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
    return false;
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
        "ChannelCapacity", "ChannelClosed", "ChannelFull", "ChannelLength",
        "ChannelSpace", "HasLayer", "HasProjection", "HasState", "HasZone",
        "HasZoneLayer", "HasZoneProjection", "HasZoneState",
    };

    if (callee == NULL)
        return false;
    return bsearch(&callee, k_pure_query_builtins,
                   sizeof(k_pure_query_builtins) / sizeof(k_pure_query_builtins[0]),
                   sizeof(k_pure_query_builtins[0]), mir_string_pointer_compare) != NULL;
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
    case AST_FAIL_STMT:
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
mir_instruction_source_stmt_has_side_effect_hint(const MIRInstruction *inst)
{
    int source_type = mir_instruction_source_node_type_or(inst, -1);

    if (source_type < 0)
        return false;
    return mir_source_node_type_stmt_has_side_effect_hint(
        (ASTNodeType)source_type, inst->arg0);
}

bool
mir_instruction_source_stmt_residual_emit_is_allowed(const MIRInstruction *inst)
{
    int source_type;

    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return false;
    if (mir_instruction_is_intent_semantic_carrier(inst))
        return true;
    if (!mir_instruction_has_source_location(inst)
        || !mir_instruction_has_source_statement_order(inst))
        return false;
    if (mir_instruction_source_is_cfg_owned_control(inst))
        return false;
    source_type = mir_instruction_source_node_type_or(inst, -1);
    if (source_type == AST_LET_DECL || source_type == AST_LET_DESTRUCTURE
        || source_type == AST_ASSIGNMENT)
        return false;
    if (source_type == AST_CALL)
        return !mir_source_call_is_pure_query(inst->arg0);
    return source_type == AST_FAIL_STMT || source_type == AST_INTENT_STEP
        || source_type == AST_BIND_STMT || source_type == AST_DEFER_STMT
        || source_type == AST_PARALLEL_BLOCK || source_type == AST_ASYNC_BLOCK
        || source_type == AST_SPAWN_EXPR || source_type == AST_AWAIT_EXPR
        || source_type == AST_CHANNEL_SEND || source_type == AST_CHANNEL_RECV
        || source_type == AST_EVENT_SUBSCRIBE
        || source_type == AST_EVENT_UNSUBSCRIBE
        || source_type == AST_EVENT_INVOKE;
}

bool
mir_instruction_source_stmt_reemit_is_redundant(const MIRInstruction *inst)
{
    int source_type;

    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return false;
    if (!mir_instruction_has_source_statement_order(inst))
        return true;
    source_type = mir_instruction_source_node_type_or(inst, -1);
    return source_type == AST_BLOCK || source_type == AST_RETURN
        || source_type == AST_LET_DESTRUCTURE;
}

bool
mir_instruction_source_stmt_call_emit_is_allowed(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_STMT
        && inst->expr0 != NULL
        && mir_instruction_source_matches_ast_type(inst, AST_CALL);
}

bool
mir_instruction_source_stmt_runtime_boundary_emit_is_allowed(
        const MIRInstruction *inst)
{
    int source_type;

    if (inst == NULL || inst->kind != MIR_INST_STMT || inst->expr0 == NULL)
        return false;
    source_type = mir_instruction_source_node_type_or(inst, -1);
    return source_type == AST_SPAWN_EXPR || source_type == AST_AWAIT_EXPR
        || source_type == AST_CHANNEL_SEND || source_type == AST_CHANNEL_RECV
        || source_type == AST_EVENT_SUBSCRIBE
        || source_type == AST_EVENT_UNSUBSCRIBE
        || source_type == AST_EVENT_INVOKE
        || source_type == AST_PARALLEL_BLOCK
        || source_type == AST_ASYNC_BLOCK
        || source_type == AST_UNSAFE_BLOCK
        || source_type == AST_TRANSACTION_BLOCK;
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

static bool
mir_instruction_resource_op_is_channel_boundary(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_RESOURCE_OP
        || inst->name == NULL)
        return false;
    return strcmp(inst->name, "ChannelSend") == 0
        || strcmp(inst->name, "ChannelRecv") == 0
        || strcmp(inst->name, "ChannelSelect") == 0;
}

bool
mir_instruction_has_inherent_concurrency_fact(const MIRInstruction *inst)
{
    return inst != NULL
        && ((inst->has_surface_usage_facts && inst->uses_thread_pool_surface)
            || mir_instruction_resource_op_is_channel_boundary(inst)
            || inst->requires_channel_receive_statement_emit
            || inst->requires_select_receive_statement_emit);
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
