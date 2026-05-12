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

static bool
llvm_stmt_format_host_method_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                                  const char *host_name, const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0 || host_name == NULL
        || method_name == NULL) {
        return false;
    }
    written = snprintf(out, out_size, "%s_%s", host_name, method_name);
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "LLVM host method lookup name is too long for '%s.%s'",
        host_name, method_name);
    return false;
}

LLVMFuncEntry *
llvm_stmt_lookup_visible_function(LLVMGenCtx *ctx, const char *callee)
{
    LLVMFuncEntry *entry;
    ASTNode *host_decl;
    const char *host_name;
    char full_name[256];

    if (ctx == NULL || callee == NULL)
        return NULL;

    entry = llvm_lookup_function(ctx, callee);
    if (entry != NULL)
        return entry;

    host_decl = llvm_current_host_decl(ctx);
    host_name = llvm_decl_node_name(host_decl);
    if (host_name == NULL)
        return NULL;
    if (!llvm_stmt_format_host_method_name(ctx, full_name, sizeof(full_name),
            host_name, callee))
        return NULL;
    return llvm_lookup_function(ctx, full_name);
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

LLVMTypeRef
llvm_stmt_lookup_declared_call_return_type(LLVMGenCtx *ctx, const char *callee)
{
    ASTNode *decl;

    if (ctx == NULL || callee == NULL)
        return NULL;
    decl = llvm_stmt_find_function_decl_by_name(ctx, callee);
    if (decl == NULL || decl->type != AST_FUNC_DECL
        || decl->data.func_decl.return_type == NULL)
        return NULL;
    return ast_type_to_llvm(ctx, decl->data.func_decl.return_type);
}

LLVMTypeRef
llvm_stmt_promote_numeric_type(LLVMGenCtx *ctx, LLVMTypeRef left_ty,
                               LLVMTypeRef right_ty)
{
    if (left_ty == ctx->type_f64 || right_ty == ctx->type_f64)
        return ctx->type_f64;
    if (left_ty == ctx->type_f32 || right_ty == ctx->type_f32)
        return ctx->type_f32;
    if (left_ty == ctx->type_i64 || right_ty == ctx->type_i64)
        return ctx->type_i64;
    return ctx->type_i32;
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
