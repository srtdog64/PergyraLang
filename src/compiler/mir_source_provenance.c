#include "mir.h"

#include "../parser/ast_api.h"

#include <stdlib.h>
#include <string.h>

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
mir_instruction_consumes_resource_source(
    const MIRInstruction *resource,
    const MIRInstruction *consumer)
{
    ASTNode *consumer_expr;
    ASTNode *consumer_arg;
    ASTNode *resource_callee;
    ASTNode *consumer_callee;
    const char *resource_callee_name;
    const char *consumer_callee_name;
    uint32_t resource_id;
    uint32_t consumer_id;

    if (resource == NULL || consumer == NULL || resource->ast == NULL)
        return false;
    if (mir_instructions_share_source_statement(resource, consumer))
        return true;
    if (consumer->ast == resource->ast
        || consumer->expr0 == resource->ast
        || consumer->expr1 == resource->ast) {
        return true;
    }
    consumer_expr = consumer->expr0 != NULL
        ? consumer->expr0
        : consumer->ast;
    resource_id = ast_node_stable_id(resource->ast);
    consumer_id = ast_node_stable_id(consumer_expr);
    if (resource_id != 0 && resource_id == consumer_id)
        return true;
    if (consumer_expr == NULL || consumer_expr->type != AST_CALL
        || resource->ast->type != AST_CALL
        || resource->slot_anchor == NULL
        || ast_call_arg_count(consumer_expr) == 0) {
        return false;
    }
    resource_callee = ast_call_callee(resource->ast);
    consumer_callee = ast_call_callee(consumer_expr);
    if (resource_callee == NULL || consumer_callee == NULL
        || resource_callee->type != AST_IDENTIFIER
        || consumer_callee->type != AST_IDENTIFIER) {
        return false;
    }
    resource_callee_name = ast_identifier_name(resource_callee);
    consumer_callee_name = ast_identifier_name(consumer_callee);
    if (resource_callee_name == NULL || consumer_callee_name == NULL
        || strcmp(resource_callee_name, consumer_callee_name) != 0) {
        return false;
    }
    consumer_arg = ast_call_argument(consumer_expr, 0);
    return consumer_arg != NULL
        && consumer_arg->type == AST_IDENTIFIER
        && ast_identifier_name(consumer_arg) != NULL
        && strcmp(ast_identifier_name(consumer_arg),
                  resource->slot_anchor) == 0;
}
