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
    result = result != NULL ? pergyra_strdup(result) : NULL;
    pgy_arena_destroy(&arena);
    return result;
}

char *
llvm_stmt_render_type_arg_scratch(GenericParam *param, PgyArena *arena)
{
    ASTNode *type = NULL;

    if (param == NULL)
        return NULL;

    type = ast_generic_param_constraint(param);
    if (ast_type_name(type) != NULL) {
        GenericParams *generic_args = ast_type_generic_args(type);
        size_t generic_count = ast_generic_param_count(generic_args);
        if (generic_count == 0)
            return pgy_arena_strdup(arena, ast_type_name(type));

        char *result = pgy_arena_strdup(arena, ast_type_name(type));
        if (result == NULL)
            return NULL;
        for (size_t i = 0; i < generic_count; i++) {
            char *arg = llvm_stmt_render_type_arg_scratch(
                ast_generic_param_at(generic_args, i), arena);
            if (arg == NULL || arg[0] == '\0')
                return NULL;
            size_t cur_len = strlen(result);
            size_t arg_len = strlen(arg);
            size_t need;
            if (arg_len > ((size_t)-1) - cur_len - 4)
                return NULL;
            need = cur_len + arg_len + 4;
            char *grown = pgy_arena_alloc(arena, need);
            if (grown == NULL)
                return NULL;
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
            if (cur_len > ((size_t)-1) - 2)
                return NULL;
            char *grown = pgy_arena_alloc(arena, cur_len + 2);
            if (grown == NULL)
                return NULL;
            memcpy(grown, result, cur_len + 1);
            result = grown;
            result[cur_len] = '>';
            result[cur_len + 1] = '\0';
        }
        return result;
    }

    if (ast_generic_param_name(param) != NULL)
        return pgy_arena_strdup(arena, ast_generic_param_name(param));
    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
