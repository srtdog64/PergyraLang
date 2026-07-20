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
    if (resource == NULL || consumer == NULL)
        return false;
    if (mir_instructions_share_source_statement(resource, consumer))
        return true;
    if (resource->has_source_statement_stable_id
        && consumer->has_source_statement_stable_id
        && resource->source_statement_stable_id != 0
        && resource->source_statement_stable_id
            == consumer->source_statement_stable_id) {
        return true;
    }
    if (resource->source_stable_id != 0
        && resource->source_stable_id == consumer->source_stable_id) {
        return true;
    }
    return false;
}
