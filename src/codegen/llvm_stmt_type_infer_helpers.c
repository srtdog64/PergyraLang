#ifdef PGY_LLVM_ENABLED
#include "llvm_stmt_type_infer_helpers.h"
#include "transpiler_builtin_type_table.h"

static bool
llvm_stmt_type_name_is_simple_builtin_return(const char *type_name)
{
    if (type_name == NULL)
        return false;
    return strcmp(type_name, "Int") == 0
        || strcmp(type_name, "Bool") == 0
        || strcmp(type_name, "Float") == 0
        || strcmp(type_name, "String") == 0
        || strcmp(type_name, "Void") == 0;
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
    if (callee == NULL)
        return false;
    return strcmp(callee, "Read") == 0
        || strcmp(callee, "Write") == 0
        || strcmp(callee, "Release") == 0;
}

bool
llvm_stmt_slot_call_returns_value(const char *callee)
{
    return callee != NULL && strcmp(callee, "Read") == 0;
}

bool
llvm_stmt_call_returns_collection_size(const char *callee)
{
    if (callee == NULL)
        return false;
    return strcmp(callee, "ListSize") == 0
        || strcmp(callee, "QueueSize") == 0
        || strcmp(callee, "MapSize") == 0;
}

bool
llvm_stmt_call_returns_collection_bool(const char *callee)
{
    if (callee == NULL)
        return false;
    return strcmp(callee, "QueueEmpty") == 0
        || strcmp(callee, "MapHas") == 0;
}

bool
llvm_stmt_call_returns_collection_value(const char *callee)
{
    if (callee == NULL)
        return false;
    return strcmp(callee, "ListGet") == 0
        || strcmp(callee, "QueuePop") == 0
        || strcmp(callee, "MapGet") == 0;
}

bool
llvm_stmt_call_returns_domain_bool(const char *callee)
{
    if (callee == NULL)
        return false;
    return strcmp(callee, "HasZone") == 0
        || strcmp(callee, "HasState") == 0
        || strcmp(callee, "HasLayer") == 0
        || strcmp(callee, "HasProjection") == 0;
}

const char *
llvm_stmt_lookup_collection_get_inner(LLVMGenCtx *ctx, const char *callee,
                                      const char *collection)
{
    if (ctx == NULL || callee == NULL || collection == NULL)
        return NULL;
    if (strcmp(callee, "ListGet") == 0)
        return llvm_lookup_list_inner(ctx, collection);
    if (strcmp(callee, "QueuePop") == 0)
        return llvm_lookup_queue_inner(ctx, collection);
    if (strcmp(callee, "MapGet") == 0)
        return llvm_lookup_map_value(ctx, collection);
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

#endif /* PGY_LLVM_ENABLED */
