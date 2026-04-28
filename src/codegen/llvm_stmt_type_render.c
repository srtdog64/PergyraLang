#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

char *
llvm_stmt_render_type_arg_scratch(GenericParam *param, PgyArena *arena);

char *
llvm_stmt_render_type_arg(GenericParam *param)
{
    PgyArena arena;
    char *result;

    pgy_arena_init(&arena, 0);
    result = llvm_stmt_render_type_arg_scratch(param, &arena);
    result = result != NULL ? pergyra_strdup(result) : pergyra_strdup("Int");
    pgy_arena_destroy(&arena);
    return result;
}

char *
llvm_stmt_render_type_arg_scratch(GenericParam *param, PgyArena *arena)
{
    ASTNode *type = NULL;

    if (param == NULL)
        return pgy_arena_strdup(arena, "Int");

    type = param->constraint;
    if (type != NULL && type->type == AST_TYPE && type->data.type.name != NULL) {
        if (type->data.type.generic_args == NULL || type->data.type.generic_args->count == 0)
            return pgy_arena_strdup(arena, type->data.type.name);

        char *result = pgy_arena_strdup(arena, type->data.type.name);
        for (size_t i = 0; i < type->data.type.generic_args->count; i++) {
            char *arg = llvm_stmt_render_type_arg_scratch(
                type->data.type.generic_args->params[i], arena);
            size_t cur_len = strlen(result);
            size_t arg_len = strlen(arg);
            size_t need = cur_len + arg_len + 4;
            char *grown = pgy_arena_alloc(arena, need);
            if (grown == NULL)
                return pgy_arena_strdup(arena, "Int");
            memcpy(grown, result, cur_len + 1);
            result = grown;
            size_t offset = cur_len;
            if (i == 0) {
                result[offset++] = '<';
            } else {
                result[offset++] = ',';
                result[offset++] = ' ';
            }
            memcpy(result + offset, arg, arg_len);
            offset += arg_len;
            result[offset] = '\0';
        }
        {
            size_t cur_len = strlen(result);
            char *grown = pgy_arena_alloc(arena, cur_len + 2);
            if (grown == NULL)
                return pgy_arena_strdup(arena, "Int");
            memcpy(grown, result, cur_len + 1);
            result = grown;
            result[cur_len] = '>';
            result[cur_len + 1] = '\0';
        }
        return result;
    }

    if (param->name != NULL)
        return pgy_arena_strdup(arena, param->name);
    return pgy_arena_strdup(arena, "Int");
}

#endif /* PGY_LLVM_ENABLED */
