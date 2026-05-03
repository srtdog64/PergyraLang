#ifdef PGY_LLVM_ENABLED
#include "llvm_stmt_type_infer_helpers.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_builtin_type_table.h"

#define PGY_ARRAY_COUNT(items) (sizeof(items) / sizeof((items)[0]))

typedef const char *(*LLVMCollectionInnerLookup)(LLVMGenCtx *ctx,
                                                 const char *collection);

typedef struct
{
    const char *name;
    LLVMCollectionInnerLookup lookup;
} LLVMCollectionGetSpec;

static bool
llvm_stmt_name_in_table(const char *name, const char *const *items,
                        size_t item_count)
{
    if (name == NULL)
        return false;
    for (size_t i = 0; i < item_count; i++) {
        if (strcmp(name, items[i]) == 0)
            return true;
    }
    return false;
}

static bool
llvm_stmt_type_name_is_simple_builtin_return(const char *type_name)
{
    static const char *const return_types[] = {
        "Int",
        "Bool",
        "Float",
        "String",
        "Void",
    };
    return llvm_stmt_name_in_table(type_name, return_types,
        PGY_ARRAY_COUNT(return_types));
}

LLVMTypeRef
llvm_stmt_infer_scalar_builtin_type(LLVMGenCtx *ctx, const char *callee)
{
    const char *type_name;

    if (ctx == NULL || callee == NULL)
        return NULL;

    type_name = pgy_builtin_simple_return_type(callee);
    if (type_name == NULL)
        return NULL;
    if (!llvm_stmt_type_name_is_simple_builtin_return(type_name))
        return NULL;
    return pergyra_type_to_llvm(ctx, type_name);
}

bool
llvm_stmt_call_is_slot_builtin(const char *callee)
{
    return pgy_codegen_call_name_is_slot_operation(callee);
}

bool
llvm_stmt_slot_call_returns_value(const char *callee)
{
    return pgy_codegen_call_name_is_read(callee);
}

bool
llvm_stmt_call_returns_collection_size(const char *callee)
{
    static const char *const calls[] = {
        "ListSize",
        "QueueSize",
        "MapSize",
    };
    return llvm_stmt_name_in_table(callee, calls, PGY_ARRAY_COUNT(calls));
}

bool
llvm_stmt_call_returns_collection_bool(const char *callee)
{
    static const char *const calls[] = {
        "QueueEmpty",
        "MapHas",
    };
    return llvm_stmt_name_in_table(callee, calls, PGY_ARRAY_COUNT(calls));
}

bool
llvm_stmt_call_returns_collection_value(const char *callee)
{
    static const char *const calls[] = {
        "ListGet",
        "QueuePop",
        "MapGet",
    };
    return llvm_stmt_name_in_table(callee, calls, PGY_ARRAY_COUNT(calls));
}

bool
llvm_stmt_call_returns_domain_bool(const char *callee)
{
    static const char *const calls[] = {
        "HasZone",
        "HasState",
        "HasLayer",
        "HasProjection",
    };
    return llvm_stmt_name_in_table(callee, calls, PGY_ARRAY_COUNT(calls));
}

const char *
llvm_stmt_lookup_collection_get_inner(LLVMGenCtx *ctx, const char *callee,
                                      const char *collection)
{
    if (ctx == NULL || callee == NULL || collection == NULL)
        return NULL;
    static const LLVMCollectionGetSpec specs[] = {
        { "ListGet", llvm_lookup_list_inner },
        { "QueuePop", llvm_lookup_queue_inner },
        { "MapGet", llvm_lookup_map_value },
    };
    for (size_t i = 0; i < PGY_ARRAY_COUNT(specs); i++) {
        if (strcmp(callee, specs[i].name) == 0)
            return specs[i].lookup(ctx, collection);
    }
    return NULL;
}

const char *
llvm_stmt_lookup_slot_or_view_inner(LLVMGenCtx *ctx, const char *receiver_name)
{
    const char *inner;
    LLVMViewVarEntry *view;

    if (ctx == NULL || receiver_name == NULL)
        return NULL;

    inner = llvm_lookup_slot_inner(ctx, receiver_name);
    if (inner != NULL)
        return inner;

    view = llvm_lookup_view_var(ctx, receiver_name);
    if (view != NULL)
        return view->inner_type;

    return llvm_lookup_device_slot_inner(ctx, receiver_name);
}

#undef PGY_ARRAY_COUNT

#endif /* PGY_LLVM_ENABLED */
