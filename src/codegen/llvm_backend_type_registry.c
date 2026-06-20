/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend variable type registry lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"

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
    LLVM_REGISTRY_TYPE_SLOT,
    LLVM_REGISTRY_TYPE_SECURE_SLOT,
    LLVM_REGISTRY_TYPE_DEVICE_SLOT,
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
        { "DeviceSlot", LLVM_REGISTRY_TYPE_DEVICE_SLOT },
        { "Future", LLVM_REGISTRY_TYPE_FUTURE },
        { "HashMap", LLVM_REGISTRY_TYPE_HASHMAP },
        { "List", LLVM_REGISTRY_TYPE_LIST },
        { "Queue", LLVM_REGISTRY_TYPE_QUEUE },
        { "Rc", LLVM_REGISTRY_TYPE_RC },
        { "RemoteFuture", LLVM_REGISTRY_TYPE_REMOTE_FUTURE },
        { "SecureSlot", LLVM_REGISTRY_TYPE_SECURE_SLOT },
        { "Set", LLVM_REGISTRY_TYPE_SET },
        { "Slice", LLVM_REGISTRY_TYPE_SLICE },
        { "Weak", LLVM_REGISTRY_TYPE_WEAK },
    };
    const LLVMRegistryTypeSpec *spec;
    PgyTypeKind pgy_kind;

    if (type_name == NULL)
        return LLVM_REGISTRY_TYPE_UNKNOWN;

    pgy_kind = pgy_classify_type(type_name);
    if (pgy_kind == PGY_TK_RC)
        return LLVM_REGISTRY_TYPE_RC;
    if (pgy_kind == PGY_TK_WEAK)
        return LLVM_REGISTRY_TYPE_WEAK;
    if (pgy_kind == PGY_TK_SLOT)
        return LLVM_REGISTRY_TYPE_SLOT;
    if (pgy_kind == PGY_TK_SECURE_SLOT)
        return LLVM_REGISTRY_TYPE_SECURE_SLOT;
    if (pgy_kind == PGY_TK_DEVICE_SLOT)
        return LLVM_REGISTRY_TYPE_DEVICE_SLOT;
    if (pgy_kind == PGY_TK_ARRAY)
        return LLVM_REGISTRY_TYPE_ARRAY;
    if (pgy_kind == PGY_TK_SLICE)
        return LLVM_REGISTRY_TYPE_SLICE;
    if (pgy_kind == PGY_TK_HASHMAP)
        return LLVM_REGISTRY_TYPE_HASHMAP;
    if (pgy_kind == PGY_TK_LIST)
        return LLVM_REGISTRY_TYPE_LIST;
    if (pgy_kind == PGY_TK_QUEUE)
        return LLVM_REGISTRY_TYPE_QUEUE;
    if (pgy_kind == PGY_TK_SET)
        return LLVM_REGISTRY_TYPE_SET;

    spec = (const LLVMRegistryTypeSpec *)bsearch(&type_name,
        specs,
        sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]),
        llvm_registry_type_spec_compare);
    return spec != NULL ? spec->kind : LLVM_REGISTRY_TYPE_UNKNOWN;
}

static bool
llvm_registry_required_arg_name(LLVMGenCtx *ctx,
                                ASTNode *owner,
                                const char *type_name,
                                int arg_index,
                                const char *container_name,
                                char *out,
                                size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (llvm_constructed_arg_name_copy(type_name, arg_index, out, out_size)
        && out[0] != '\0')
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, owner,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s registry requires concrete type metadata",
            container_name != NULL ? container_name : "typed variable");
    }
    return false;
}

static LLVMValueRef
llvm_registry_active_binding(LLVMGenCtx *ctx, const char *var_name)
{
    LLVMVarEntry entry;

    if (ctx == NULL || var_name == NULL)
        return NULL;
    return llvm_scope_lookup_snapshot(ctx, var_name, &entry)
        ? entry.alloca : NULL;
}

void
llvm_register_typed_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                LLVMValueRef binding,
                                ASTNode *type_node)
{
    const char *type_name;
    const char *base_type_name;
    char *rendered_type_name;
    LLVMRegistryTypeKind type_kind;
    char arg0_name[256];
    char arg1_name[256];

    if (ctx == NULL || var_name == NULL || type_node == NULL)
        return;

    if (type_node->type == AST_EVENT_HANDLER_TYPE) {
        llvm_register_callable_var(ctx, var_name, type_node);
        return;
    }

    if (ast_type_name(type_node) == NULL)
        return;

    base_type_name = ast_type_name(type_node);
    rendered_type_name = llvm_render_type_name_scratch_in_ctx(ctx, type_node,
        &ctx->scratch);
    type_name = (rendered_type_name != NULL
        && rendered_type_name[0] != '\0'
        && strcmp(rendered_type_name, "Unknown") != 0)
            ? rendered_type_name
            : base_type_name;
    type_kind = llvm_registry_type_kind(type_name);

    if ((type_kind == LLVM_REGISTRY_TYPE_ARRAY
         || type_kind == LLVM_REGISTRY_TYPE_SLICE)) {
        LLVMTypeRef elem_type;
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                "Array/Slice", arg0_name, sizeof(arg0_name)))
            return;
        elem_type = pergyra_type_to_llvm(ctx, arg0_name);
        if (ctx->has_error || elem_type == NULL)
            return;
        llvm_register_array_var_binding(ctx, var_name, binding, elem_type, -1);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_LIST) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                "List<T>", arg0_name, sizeof(arg0_name)))
            return;
        llvm_register_list_var_binding(ctx, var_name, binding, arg0_name);
        return;
    }

    if ((type_kind == LLVM_REGISTRY_TYPE_SLOT
         || type_kind == LLVM_REGISTRY_TYPE_SECURE_SLOT)) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                type_kind == LLVM_REGISTRY_TYPE_SECURE_SLOT
                    ? "SecureSlot<T>" : "Slot<T>",
                arg0_name, sizeof(arg0_name)))
            return;
        llvm_register_slot_var_binding(ctx, var_name, binding, arg0_name,
            type_kind == LLVM_REGISTRY_TYPE_SECURE_SLOT);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_DEVICE_SLOT) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                "DeviceSlot<T>", arg0_name, sizeof(arg0_name)))
            return;
        llvm_register_device_slot_var_binding(ctx, var_name, binding,
            arg0_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_SET) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                "Set<T>", arg0_name, sizeof(arg0_name)))
            return;
        llvm_register_set_var_binding(ctx, var_name, binding, arg0_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_QUEUE) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                "Queue<T>", arg0_name, sizeof(arg0_name)))
            return;
        llvm_register_queue_var_binding(ctx, var_name, binding, arg0_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_HASHMAP) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                "HashMap<K, V> key", arg0_name, sizeof(arg0_name))
            || !llvm_registry_required_arg_name(ctx, type_node, type_name, 1,
                "HashMap<K, V> value", arg1_name, sizeof(arg1_name)))
            return;
        llvm_register_map_var_binding(ctx, var_name, binding, arg0_name,
            arg1_name);
        return;
    }

    if ((type_kind == LLVM_REGISTRY_TYPE_FUTURE
         || type_kind == LLVM_REGISTRY_TYPE_REMOTE_FUTURE)) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                "Future<T>", arg0_name, sizeof(arg0_name)))
            return;
        llvm_register_future_var_binding(ctx, var_name, binding, arg0_name,
            type_kind == LLVM_REGISTRY_TYPE_REMOTE_FUTURE);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_CHANNEL) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                "Channel<T>", arg0_name, sizeof(arg0_name)))
            return;
        llvm_register_channel_var_binding(ctx, var_name, binding, arg0_name);
        return;
    }

    if (type_kind == LLVM_REGISTRY_TYPE_RC
        || type_kind == LLVM_REGISTRY_TYPE_WEAK) {
        if (!llvm_registry_required_arg_name(ctx, type_node, type_name, 0,
                type_name, arg0_name, sizeof(arg0_name)))
            return;
        if (type_kind == LLVM_REGISTRY_TYPE_RC)
            llvm_register_rc_var_binding(ctx, var_name, binding, arg0_name);
        else
            llvm_register_weak_var_binding(ctx, var_name, binding, arg0_name);
        return;
    }

    /* Prefer the specialized generic instantiation name (Pair<Int>) over
     * the type-erased base (Pair), so member calls resolve to the
     * monomorphized method (Pair<Int>_GetFirst) rather than the base. */
    if (type_name != NULL && strchr(type_name, '<') != NULL
        && llvm_lookup_class(ctx, type_name) != NULL) {
        llvm_register_var_class(ctx, var_name, type_name);
        return;
    }
    if (llvm_lookup_class(ctx, base_type_name) != NULL
        || llvm_decl_exists_in_context(ctx, AST_ENUM_DECL, base_type_name))
        llvm_register_var_class(ctx, var_name, base_type_name);
}

void
llvm_register_typed_var_abi_binding(LLVMGenCtx *ctx,
                                    const char *var_name,
                                    LLVMValueRef binding,
                                    const char *abi_type_name)
{
    LLVMRegistryTypeKind type_kind;
    char arg0_name[256];
    char arg1_name[256];

    if (ctx == NULL || var_name == NULL || abi_type_name == NULL)
        return;

    type_kind = llvm_registry_type_kind(abi_type_name);
    switch (type_kind) {
    case LLVM_REGISTRY_TYPE_ARRAY:
    case LLVM_REGISTRY_TYPE_CHANNEL:
    case LLVM_REGISTRY_TYPE_DEVICE_SLOT:
    case LLVM_REGISTRY_TYPE_FUTURE:
    case LLVM_REGISTRY_TYPE_HASHMAP:
    case LLVM_REGISTRY_TYPE_LIST:
    case LLVM_REGISTRY_TYPE_QUEUE:
    case LLVM_REGISTRY_TYPE_RC:
    case LLVM_REGISTRY_TYPE_REMOTE_FUTURE:
    case LLVM_REGISTRY_TYPE_SECURE_SLOT:
    case LLVM_REGISTRY_TYPE_SET:
    case LLVM_REGISTRY_TYPE_SLICE:
    case LLVM_REGISTRY_TYPE_SLOT:
    case LLVM_REGISTRY_TYPE_WEAK:
        break;
    default:
        return;
    }

    if (!llvm_constructed_arg_name_copy(abi_type_name, 0, arg0_name,
            sizeof(arg0_name))
        || arg0_name[0] == '\0') {
        if (!ctx->has_error)
            llvm_set_error(ctx,
                "LLVM ABI type registry requires concrete type metadata");
        return;
    }

    switch (type_kind) {
    case LLVM_REGISTRY_TYPE_ARRAY:
    case LLVM_REGISTRY_TYPE_SLICE: {
        LLVMTypeRef elem_type = pergyra_type_to_llvm(ctx, arg0_name);
        if (ctx->has_error || elem_type == NULL)
            return;
        llvm_register_array_var_binding(ctx, var_name, binding, elem_type, -1);
        return;
    }
    case LLVM_REGISTRY_TYPE_LIST:
        llvm_register_list_var_binding(ctx, var_name, binding, arg0_name);
        return;
    case LLVM_REGISTRY_TYPE_SET:
        llvm_register_set_var_binding(ctx, var_name, binding, arg0_name);
        return;
    case LLVM_REGISTRY_TYPE_QUEUE:
        llvm_register_queue_var_binding(ctx, var_name, binding, arg0_name);
        return;
    case LLVM_REGISTRY_TYPE_HASHMAP:
        if (!llvm_constructed_arg_name_copy(abi_type_name, 1, arg1_name,
                sizeof(arg1_name))
            || arg1_name[0] == '\0') {
            if (!ctx->has_error)
                llvm_set_error(ctx,
                    "LLVM ABI HashMap registry requires concrete value metadata");
            return;
        }
        llvm_register_map_var_binding(ctx, var_name, binding, arg0_name,
            arg1_name);
        return;
    case LLVM_REGISTRY_TYPE_SLOT:
    case LLVM_REGISTRY_TYPE_SECURE_SLOT:
        llvm_register_slot_var_binding(ctx, var_name, binding, arg0_name,
            type_kind == LLVM_REGISTRY_TYPE_SECURE_SLOT);
        return;
    case LLVM_REGISTRY_TYPE_DEVICE_SLOT:
        llvm_register_device_slot_var_binding(ctx, var_name, binding,
            arg0_name);
        return;
    case LLVM_REGISTRY_TYPE_FUTURE:
    case LLVM_REGISTRY_TYPE_REMOTE_FUTURE:
        llvm_register_future_var_binding(ctx, var_name, binding, arg0_name,
            type_kind == LLVM_REGISTRY_TYPE_REMOTE_FUTURE);
        return;
    case LLVM_REGISTRY_TYPE_CHANNEL:
        llvm_register_channel_var_binding(ctx, var_name, binding, arg0_name);
        return;
    case LLVM_REGISTRY_TYPE_RC:
        llvm_register_rc_var_binding(ctx, var_name, binding, arg0_name);
        return;
    case LLVM_REGISTRY_TYPE_WEAK:
        llvm_register_weak_var_binding(ctx, var_name, binding, arg0_name);
        return;
    default:
        return;
    }
}

void
llvm_register_typed_var(LLVMGenCtx *ctx, const char *var_name,
                        ASTNode *type_node)
{
    llvm_register_typed_var_binding(ctx, var_name,
        llvm_registry_active_binding(ctx, var_name), type_node);
}

#endif /* PGY_LLVM_ENABLED */
