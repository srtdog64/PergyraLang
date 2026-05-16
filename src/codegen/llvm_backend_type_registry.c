/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend variable type registry lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"

#include <stdlib.h>
#include <string.h>

typedef enum LLVMRegistryTypeKind {
    LLVM_REGISTRY_TYPE_UNKNOWN = 0,
    LLVM_REGISTRY_TYPE_ARRAY,
    LLVM_REGISTRY_TYPE_CHANNEL,
    LLVM_REGISTRY_TYPE_FUTURE,
    LLVM_REGISTRY_TYPE_HASHMAP,
    LLVM_REGISTRY_TYPE_LIST,
    LLVM_REGISTRY_TYPE_QUEUE,
    LLVM_REGISTRY_TYPE_RC,
    LLVM_REGISTRY_TYPE_REMOTE_FUTURE,
    LLVM_REGISTRY_TYPE_SET,
    LLVM_REGISTRY_TYPE_SLICE,
    LLVM_REGISTRY_TYPE_WEAK,
} LLVMRegistryTypeKind;

typedef struct LLVMRegistryTypeSpec {
    const char *name;
    LLVMRegistryTypeKind kind;
} LLVMRegistryTypeSpec;

static int
llvm_registry_type_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const LLVMRegistryTypeSpec *spec = (const LLVMRegistryTypeSpec *)entry;

    return strcmp(name, spec->name);
}

static LLVMRegistryTypeKind
llvm_registry_type_kind(const char *type_name)
{
    static const LLVMRegistryTypeSpec specs[] = {
        { "Array", LLVM_REGISTRY_TYPE_ARRAY },
        { "Channel", LLVM_REGISTRY_TYPE_CHANNEL },
        { "Future", LLVM_REGISTRY_TYPE_FUTURE },
        { "HashMap", LLVM_REGISTRY_TYPE_HASHMAP },
        { "List", LLVM_REGISTRY_TYPE_LIST },
        { "Queue", LLVM_REGISTRY_TYPE_QUEUE },
        { "Rc", LLVM_REGISTRY_TYPE_RC },
        { "RemoteFuture", LLVM_REGISTRY_TYPE_REMOTE_FUTURE },
        { "Set", LLVM_REGISTRY_TYPE_SET },
        { "Slice", LLVM_REGISTRY_TYPE_SLICE },
        { "Weak", LLVM_REGISTRY_TYPE_WEAK },
    };
    const LLVMRegistryTypeSpec *spec;

    if (type_name == NULL)
        return LLVM_REGISTRY_TYPE_UNKNOWN;
    if (strncmp(type_name, "Rc<", 3) == 0)
        return LLVM_REGISTRY_TYPE_RC;
    if (strncmp(type_name, "Weak<", 5) == 0)
        return LLVM_REGISTRY_TYPE_WEAK;

    spec = (const LLVMRegistryTypeSpec *)bsearch(&type_name,
        specs,
        sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]),
        llvm_registry_type_spec_compare);
    return spec != NULL ? spec->kind : LLVM_REGISTRY_TYPE_UNKNOWN;
}

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

static char *
llvm_registry_render_required_type_name(LLVMGenCtx *ctx,
                                        ASTNode *owner,
                                        ASTNode *type_node,
                                        const char *container_name)
{
    char *name = llvm_render_type_name(type_node);
    if (name != NULL && name[0] != '\0'
        && strcmp(name, "Unknown") != 0)
        return name;

    free(name);
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, owner,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s registry requires concrete type metadata",
            container_name != NULL ? container_name : "typed variable");
    }
    return NULL;
}

static ASTNode *
llvm_registry_generic_arg_type(GenericParams *generic_args, size_t index)
{
    GenericParam *param = ast_generic_param_at(generic_args, index);
    return ast_generic_param_constraint(param);
}

void
llvm_register_typed_var(LLVMGenCtx *ctx, const char *var_name,
                        ASTNode *type_node)
{
    const char *type_name;
    LLVMRegistryTypeKind type_kind;
    GenericParams *generic_args;
    ASTNode *arg0_type;
    ASTNode *arg1_type;

    if (ctx == NULL || var_name == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        llvm_register_callable_var(ctx, var_name, type_node);
        return;
    }

    if (ast_type_name(type_node) == NULL)
        return;

    type_name = ast_type_name(type_node);
    type_kind = llvm_registry_type_kind(type_name);
    generic_args = ast_type_generic_args(type_node);
    arg0_type = llvm_registry_generic_arg_type(generic_args, 0);
    arg1_type = llvm_registry_generic_arg_type(generic_args, 1);

    if ((type_kind == LLVM_REGISTRY_TYPE_ARRAY
         || type_kind == LLVM_REGISTRY_TYPE_SLICE)
        && arg0_type != NULL) {
        char *elem_name = llvm_render_type_name_scratch(
            arg0_type, &ctx->scratch);
        if (elem_name == NULL || elem_name[0] == '\0') {
            llvm_set_error_at_with_hints(ctx, type_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM Array/Slice registry requires concrete element metadata");
            return;
        }
        LLVMTypeRef elem_type = pergyra_type_to_llvm(ctx, elem_name);
        if (ctx->has_error || elem_type == NULL)
            return;
        llvm_register_array_var(ctx, var_name, elem_type, -1);
    }

    if (type_kind == LLVM_REGISTRY_TYPE_LIST
        && arg0_type != NULL) {
        char *inner_name = llvm_registry_render_required_type_name(ctx,
            type_node, arg0_type,
            "List<T>");
        if (inner_name == NULL)
            return;
        llvm_register_list_var(ctx, var_name, inner_name);
        free(inner_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_SET
        && arg0_type != NULL) {
        char *inner_name = llvm_registry_render_required_type_name(ctx,
            type_node, arg0_type,
            "Set<T>");
        if (inner_name == NULL)
            return;
        llvm_register_set_var(ctx, var_name, inner_name);
        free(inner_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_QUEUE
        && arg0_type != NULL) {
        char *inner_name = llvm_registry_render_required_type_name(ctx,
            type_node, arg0_type,
            "Queue<T>");
        if (inner_name == NULL)
            return;
        llvm_register_queue_var(ctx, var_name, inner_name);
        free(inner_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_HASHMAP
        && arg0_type != NULL
        && arg1_type != NULL) {
        char *key_name = llvm_registry_render_required_type_name(ctx,
            type_node, arg0_type,
            "HashMap<K, V> key");
        char *value_name = llvm_registry_render_required_type_name(ctx,
            type_node, arg1_type,
            "HashMap<K, V> value");
        if (key_name == NULL || value_name == NULL) {
            free(key_name);
            free(value_name);
            return;
        }
        llvm_register_map_var(ctx, var_name, key_name, value_name);
        free(key_name);
        free(value_name);
        return;
    }

    if ((type_kind == LLVM_REGISTRY_TYPE_FUTURE
         || type_kind == LLVM_REGISTRY_TYPE_REMOTE_FUTURE)
        && arg0_type != NULL) {
        char *inner_name = llvm_registry_render_required_type_name(ctx,
            type_node, arg0_type,
            "Future<T>");
        if (inner_name == NULL)
            return;
        llvm_register_future_var(ctx, var_name, inner_name,
            type_kind == LLVM_REGISTRY_TYPE_REMOTE_FUTURE);
        free(inner_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_CHANNEL
        && arg0_type != NULL) {
        char *inner_name = llvm_registry_render_required_type_name(ctx,
            type_node, arg0_type,
            "Channel<T>");
        if (inner_name == NULL)
            return;
        llvm_register_channel_var(ctx, var_name, inner_name);
        free(inner_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_RC
        || type_kind == LLVM_REGISTRY_TYPE_WEAK) {
        char *inner_name = NULL;
        bool free_inner_name = false;
        if (arg0_type != NULL) {
            inner_name = llvm_registry_render_required_type_name(ctx,
                type_node, arg0_type, type_name);
            free_inner_name = true;
        } else {
            inner_name = llvm_copy_first_constructed_arg_name(ctx, type_name);
        }
        if (inner_name == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, type_node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM %s registry requires concrete type metadata",
                    type_name);
            }
            return;
        }
        if (type_kind == LLVM_REGISTRY_TYPE_RC)
            llvm_register_rc_var(ctx, var_name, inner_name);
        else
            llvm_register_weak_var(ctx, var_name, inner_name);
        if (free_inner_name)
            free(inner_name);
        return;
    }

    if (llvm_lookup_class(ctx, type_name) != NULL
        || llvm_find_enum_decl(ctx, type_name) != NULL)
        llvm_register_var_class(ctx, var_name, type_name);
}

#endif /* PGY_LLVM_ENABLED */
