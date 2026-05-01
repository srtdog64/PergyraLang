/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend variable type registry lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"

#include <stdlib.h>
#include <string.h>

static char *
llvm_copy_first_constructed_arg_name(LLVMGenCtx *ctx, const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return NULL;

    const char *lt = strchr(type_name, '<');
    const char *gt = strrchr(type_name, '>');
    if (lt == NULL || gt == NULL || gt <= lt + 1)
        return NULL;

    size_t len = (size_t)(gt - lt - 1);
    char *copy = pgy_arena_alloc(&ctx->persistent, len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, lt + 1, len);
    copy[len] = '\0';
    return copy;
}

void
llvm_register_typed_var(LLVMGenCtx *ctx, const char *var_name,
                        ASTNode *type_node)
{
    const char *type_name;

    if (ctx == NULL || var_name == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        llvm_register_callable_var(ctx, var_name, type_node);
        return;
    }

    if (type_node->type != AST_TYPE || type_node->data.type.name == NULL)
        return;

    type_name = type_node->data.type.name;

    if ((strcmp(type_name, "Array") == 0 || strcmp(type_name, "Slice") == 0)
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *elem_name = llvm_render_type_name_scratch(
            type_node->data.type.generic_args->params[0]->constraint,
            &ctx->scratch);
        if (elem_name == NULL || elem_name[0] == '\0') {
            llvm_set_error_at_with_hints(ctx, type_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Array/Slice registry requires concrete element metadata");
            return;
        }
        LLVMTypeRef elem_type = pergyra_type_to_llvm(ctx, elem_name);
        llvm_register_array_var(ctx, var_name, elem_type, -1);
    }

    if (strcmp(type_name, "List") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_list_var(ctx, var_name, inner_name);
        free(inner_name);
        return;
    }

    if (strcmp(type_name, "Set") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_set_var(ctx, var_name, inner_name);
        free(inner_name);
        return;
    }

    if (strcmp(type_name, "Queue") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_queue_var(ctx, var_name, inner_name);
        free(inner_name);
        return;
    }

    if (strcmp(type_name, "HashMap") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 1
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL
        && type_node->data.type.generic_args->params[1] != NULL
        && type_node->data.type.generic_args->params[1]->constraint != NULL) {
        char *key_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        char *value_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[1]->constraint);
        llvm_register_map_var(ctx, var_name, key_name, value_name);
        free(key_name);
        free(value_name);
        return;
    }

    if ((strcmp(type_name, "Future") == 0 || strcmp(type_name, "RemoteFuture") == 0)
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_future_var(ctx, var_name, inner_name,
            strcmp(type_name, "RemoteFuture") == 0);
        return;
    }

    if (strcmp(type_name, "Channel") == 0
        && type_node->data.type.generic_args != NULL
        && type_node->data.type.generic_args->count > 0
        && type_node->data.type.generic_args->params[0] != NULL
        && type_node->data.type.generic_args->params[0]->constraint != NULL) {
        char *inner_name = llvm_render_type_name(
            type_node->data.type.generic_args->params[0]->constraint);
        llvm_register_channel_var(ctx, var_name, inner_name);
        return;
    }

    if (strcmp(type_name, "Rc") == 0 || strcmp(type_name, "Weak") == 0
        || strncmp(type_name, "Rc<", 3) == 0
        || strncmp(type_name, "Weak<", 5) == 0) {
        char *inner_name = NULL;
        if (type_node->data.type.generic_args != NULL
            && type_node->data.type.generic_args->count > 0
            && type_node->data.type.generic_args->params[0] != NULL
            && type_node->data.type.generic_args->params[0]->constraint != NULL) {
            inner_name = llvm_render_type_name(
                type_node->data.type.generic_args->params[0]->constraint);
        } else {
            inner_name = llvm_copy_first_constructed_arg_name(ctx, type_name);
        }
        if (inner_name == NULL)
            return;
        if (strcmp(type_name, "Rc") == 0 || strncmp(type_name, "Rc<", 3) == 0)
            llvm_register_rc_var(ctx, var_name, inner_name);
        else
            llvm_register_weak_var(ctx, var_name, inner_name);
        return;
    }

    if (llvm_lookup_class(ctx, type_name) != NULL
        || llvm_find_enum_decl(ctx, type_name) != NULL)
        llvm_register_var_class(ctx, var_name, type_name);
}

#endif /* PGY_LLVM_ENABLED */
