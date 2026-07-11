#include "mir_speculation_facts.h"

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

typedef struct MIRExpressionSafety {
    bool pure;
    bool non_trapping;
} MIRExpressionSafety;

static MIRExpressionSafety
mir_expression_safety(const MIRRoutine *routine, ASTNode *expr)
{
    MIRExpressionSafety unsafe = { false, false };
    MIRExpressionSafety safe = { true, true };

    if (expr == NULL)
        return unsafe;

    switch (expr->type) {
    case AST_NUMBER:
    case AST_STRING:
    case AST_BOOLEAN:
        return safe;
    case AST_IDENTIFIER: {
        const char *name = ast_identifier_name(expr);
        return name != NULL
                && mir_routine_source_local_type_name(routine, name) != NULL
            ? safe : unsafe;
    }
    case AST_UNARY:
        if (ast_unary_operator(expr).type == TOKEN_NOT)
            return mir_expression_safety(routine, ast_unary_operand(expr));
        return unsafe;
    default:
        /* Composite expressions require typed operator/effect facts. AST
         * spelling alone cannot prove overflow, overload, allocation, or
         * volatile/atomic behavior, so lowering stays fail-closed here. */
        return unsafe;
    }
}

bool
mir_capture_speculation_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t b = 0; b < routine->block_count; b++) {
        MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            MIRInstruction *inst = &block->instructions[i];
            MIRExpressionSafety safety;

            if (inst->expr0 == NULL)
                continue;
            safety = mir_expression_safety(routine, inst->expr0);
            inst->has_speculation_safety_fact = true;
            inst->speculation_is_pure = safety.pure;
            inst->speculation_is_non_trapping = safety.non_trapping;
        }
    }
    return true;
}

bool
mir_validate_speculation_facts(const MIRRoutine *routine,
                               char **error_message)
{
    if (routine == NULL)
        return false;

    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const MIRInstruction *inst = &block->instructions[i];
            if (inst->expr0 != NULL && !inst->has_speculation_safety_fact) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup_printf(
                        "MIR routine '%s' block[%zu] instruction[%zu] expression is missing speculation safety fact",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        b, i);
                }
                return false;
            }
            if (inst->has_speculation_safety_fact && inst->expr0 == NULL) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup_printf(
                        "MIR routine '%s' block[%zu] instruction[%zu] has speculation safety fact without expression",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        b, i);
                }
                return false;
            }
            if (inst->speculation_is_non_trapping
                && !inst->speculation_is_pure) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup_printf(
                        "MIR routine '%s' block[%zu] instruction[%zu] non-trapping speculation fact lacks purity",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        b, i);
                }
                return false;
            }
        }
    }
    return true;
}
