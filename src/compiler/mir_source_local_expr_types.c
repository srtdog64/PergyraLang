#include "mir_source_local_expr_types.h"

#include <string.h>

#include "mir_source_local_expr_binding_facts.h"
#include "mir_source_local_expr_call_facts.h"
#include "../parser/ast_api.h"

const char *
mir_source_local_expr_type_name(const MIRProgram *program,
                                const MIRRoutine *routine,
                                MIRSourceLocalTypeScratch *scratch,
                                ASTNode *expr)
{
    if (expr == NULL)
        return NULL;
    switch (expr->type) {
    case AST_NUMBER:
        if (ast_number_is_long(expr))
            return "Long";
        return ast_number_is_float(expr) ? "Float" : "Int";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_ARRAY_LITERAL:
        if (ast_array_literal_count(expr) > 0
            && ast_array_literal_element(expr, 0) != NULL) {
            const char *inner = mir_source_local_expr_type_name(program,
                routine, scratch, ast_array_literal_element(expr, 0));
            return mir_source_local_type_scratch_format(scratch, "Array",
                inner);
        }
        return NULL;
    case AST_IDENTIFIER:
        return mir_source_local_identifier_type_name(program, routine,
            ast_identifier_name(expr));
    case AST_ARRAY_ACCESS: {
        const char *collection_type = mir_source_local_expr_type_name(
            program, routine, scratch, ast_array_access_array(expr));
        char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        char *buffer;

        if (!mir_source_local_unwrap_array_or_slice_type(collection_type,
                inner, sizeof(inner))) {
            return NULL;
        }
        buffer = mir_source_local_type_scratch_next(scratch);
        if (buffer == NULL)
            return NULL;
        memcpy(buffer, inner, strlen(inner) + 1);
        return buffer;
    }
    case AST_CHANNEL_RECV: {
        const char *channel_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_channel_recv_channel(expr));
        char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        char *buffer;

        if (!mir_source_local_unwrap_channel_type(channel_type, inner,
                sizeof(inner))) {
            return NULL;
        }
        buffer = mir_source_local_type_scratch_next(scratch);
        if (buffer == NULL)
            return NULL;
        memcpy(buffer, inner, strlen(inner) + 1);
        return buffer;
    }
    case AST_AWAIT_EXPR: {
        const char *future_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_await_expression(expr));
        char inner[MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE];
        bool is_remote = false;
        char *buffer;

        if (!mir_source_local_unwrap_future_type(future_type, inner,
                sizeof(inner), &is_remote)) {
            return NULL;
        }
        if (is_remote)
            return mir_source_local_type_scratch_format(scratch, "Result",
                inner);
        buffer = mir_source_local_type_scratch_next(scratch);
        if (buffer == NULL)
            return NULL;
        memcpy(buffer, inner, strlen(inner) + 1);
        return buffer;
    }
    case AST_SPAWN_EXPR: {
        const char *inner = mir_source_local_expr_type_name(program,
            routine, scratch, ast_spawn_function(expr));
        return mir_source_local_type_scratch_format(scratch, "Future", inner);
    }
    case AST_PARALLEL_BLOCK: {
        /* The checker-sealed give fact owns expression-form join typing. */
        const char *give = ast_parallel_join_give_type(expr);
        if (give == NULL || give[0] == '\0')
            return NULL;
        if (ast_parallel_join_reduce_op(expr) != NULL
            || ast_parallel_join_is_any(expr)) {
            return give;
        }
        return mir_source_local_type_scratch_format(scratch, "Array", give);
    }
    case AST_UNARY:
        if (ast_unary_operator(expr).type == TOKEN_NOT)
            return "Bool";
        return mir_source_local_expr_type_name(program, routine,
            scratch, ast_unary_operand(expr));
    case AST_BINARY:
        switch (ast_binary_operator(expr).type) {
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_AND:
        case TOKEN_OR:
            return "Bool";
        default:
            return mir_source_local_expr_type_name(program, routine,
                scratch, ast_binary_left(expr));
        }
    case AST_MEMBER_ACCESS: {
        const char *owner_type = mir_source_local_expr_type_name(program,
            routine, scratch, ast_member_object(expr));
        return mir_source_local_member_field_type_name(program, owner_type,
            ast_member_name(expr));
    }
    case AST_CALL:
        return mir_source_local_call_expr_type_name(program, routine, scratch,
            expr);
    default:
        return NULL;
    }
}

const char *
mir_source_local_for_loop_variable_type_name(const MIRProgram *program,
                                             const MIRRoutine *routine,
                                             MIRSourceLocalTypeScratch *scratch,
                                             ASTNode *node)
{
    ASTNode *iterable;
    const char *iterable_type;
    char *inner;

    if (node == NULL || node->type != AST_FOR_LOOP
        || ast_for_variable(node) == NULL) {
        return NULL;
    }

    iterable = ast_for_iterable(node);
    if (iterable == NULL)
        return "Int";

    iterable_type = mir_source_local_expr_type_name(program, routine,
        scratch, iterable);
    inner = mir_source_local_type_scratch_next(scratch);
    if (inner == NULL
        || !mir_source_local_unwrap_iterable_type(iterable_type, inner,
            MIR_SOURCE_LOCAL_TYPE_SCRATCH_SIZE)) {
        return NULL;
    }
    return inner;
}
