#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "../semantic/slot_summary.h"

const char *
llvm_stmt_render_type_annotation_static(ASTNode *type_ann)
{
    static char buf[256];
    size_t offset;

    if (type_ann == NULL || type_ann->type != AST_TYPE
        || type_ann->data.type.name == NULL)
        return NULL;
    if (type_ann->data.type.generic_args == NULL
        || type_ann->data.type.generic_args->count == 0)
        return type_ann->data.type.name;

    offset = (size_t)snprintf(buf, sizeof(buf), "%s<",
                              type_ann->data.type.name);
    if (offset >= sizeof(buf))
        offset = sizeof(buf) - 1;
    for (size_t i = 0; i < type_ann->data.type.generic_args->count; i++) {
        char *arg = llvm_stmt_render_type_arg(
            type_ann->data.type.generic_args->params[i]);
        if (arg == NULL || arg[0] == '\0') {
            free(arg);
            return NULL;
        }
        int written = snprintf(buf + offset, sizeof(buf) - offset,
                               "%s%s", i == 0 ? "" : ", ",
                               arg);
        free(arg);
        if (written < 0)
            break;
        offset += (size_t)written;
        if (offset >= sizeof(buf)) {
            offset = sizeof(buf) - 1;
            break;
        }
    }
    if (offset + 1 < sizeof(buf)) {
        buf[offset++] = '>';
        buf[offset] = '\0';
    } else {
        buf[sizeof(buf) - 2] = '>';
        buf[sizeof(buf) - 1] = '\0';
    }
    return buf;
}

static const char *
llvm_stmt_declared_return_type_name(LLVMGenCtx *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;

    decl = llvm_stmt_find_function_decl_by_name(ctx, name);
    if (decl == NULL
        || decl->data.func_decl.return_type == NULL
        || decl->data.func_decl.return_type->type != AST_TYPE
        || decl->data.func_decl.return_type->data.type.name == NULL) {
        return NULL;
    }

    return decl->data.func_decl.return_type->data.type.name;
}

static bool
llvm_stmt_slot_can_sink_locally(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL || ctx->current_func_decl == NULL)
        return false;
    if (ctx->current_func_decl->type != AST_FUNC_DECL
        || ctx->current_func_decl->data.func_decl.body == NULL)
        return false;
    return (slot_analyze_param_summary_in_program(
                ctx->current_func_decl->data.func_decl.body, name, NULL)
            & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
               | SLOT_PARAM_SUMMARY_CALL_ESCAPE
               | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE))
        == 0;
}

LLVMValueRef
llvm_stmt_create_slot_alloca(LLVMGenCtx *ctx, LLVMTypeRef type, const char *name)
{
    if (llvm_stmt_slot_can_sink_locally(ctx, name))
        return LLVMBuildAlloca(ctx->builder, type, name);
    return llvm_create_entry_alloca(ctx, type, name);
}

LLVMTypeRef
llvm_stmt_lambda_signature_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    if (ctx == NULL || expr == NULL || expr->type != AST_LAMBDA_EXPR)
        return NULL;

    int pc = (int)expr->data.lambda_expr.param_count;
    LLVMTypeRef *params = NULL;
    LLVMTypeRef ret_type = ctx->type_i32;

    if (expr->data.lambda_expr.return_type != NULL) {
        ret_type = ast_type_to_llvm(ctx, expr->data.lambda_expr.return_type);
        if (ctx->has_error || ret_type == NULL)
            return NULL;
    }

    if (pc > 0) {
        params = pgy_arena_calloc(&ctx->scratch,
            (size_t)pc * sizeof(LLVMTypeRef));
        if (params == NULL) {
            llvm_set_error_at_with_hints(ctx, expr,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM lambda signature parameter allocation failed");
            return NULL;
        }
        for (int i = 0; i < pc; i++) {
            ASTNode *p = expr->data.lambda_expr.params[i];
            if (p != NULL && p->type == AST_LET_DECL
                && p->data.let_decl.type != NULL) {
                params[i] = ast_type_to_llvm(ctx, p->data.let_decl.type);
                if (ctx->has_error || params[i] == NULL)
                    return NULL;
            } else {
                params[i] = ctx->type_i32;
            }
        }
    }

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, params, (unsigned)pc, 0);
    return LLVMPointerType(fn_type, 0);
}

static const char *
llvm_simple_expr_type_name(LLVMGenCtx *ctx, ASTNode *expr)
{
    ASTNode *callee;
    ASTNode *receiver;
    const char *method_name;

    if (expr == NULL)
        return NULL;

    switch (expr->type) {
    case AST_NUMBER: return "Int";
    case AST_STRING: return "String";
    case AST_BOOLEAN: return "Bool";
    case AST_IDENTIFIER: {
        LLVMVarEntry *entry = llvm_scope_lookup(ctx, expr->data.identifier.name);
        if (entry != NULL)
            return llvm_type_to_suffix(ctx, entry->type);
        return NULL;
    }
    case AST_CALL:
        callee = expr->data.call.callee;
        if (callee != NULL
            && callee->type == AST_MEMBER_ACCESS
            && callee->data.member.name != NULL) {
            receiver = callee->data.member.object;
            method_name = callee->data.member.name;
            if (receiver != NULL && receiver->type == AST_IDENTIFIER) {
                const char *name = receiver->data.identifier.name;
                const char *inner = llvm_lookup_slot_inner(ctx, name);
                if (inner == NULL) {
                    LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, name);
                    if (view != NULL)
                        inner = view->inner_type;
                }
                if (inner == NULL)
                    inner = llvm_lookup_device_slot_inner(ctx, name);
                if (inner != NULL && strcmp(method_name, "Read") == 0)
                    return inner;
                if (inner != NULL
                    && (strcmp(method_name, "Write") == 0
                        || strcmp(method_name, "Release") == 0)) {
                    return "Void";
                }
                const char *receiver_type = NULL;
                ASTNode *method_decl = NULL;
                if (strcmp(name, "self") == 0) {
                    ASTNode *host_decl = llvm_current_host_decl(ctx);
                    receiver_type = llvm_decl_node_name(host_decl);
                }
                if (receiver_type == NULL)
                    receiver_type = llvm_lookup_var_class(ctx, name);
                if (receiver_type != NULL) {
                    method_decl = llvm_find_host_method_decl_in_context(
                        ctx, receiver_type, method_name);
                    if (method_decl != NULL
                        && method_decl->data.func_decl.return_type != NULL
                        && method_decl->data.func_decl.return_type->type == AST_TYPE
                        && method_decl->data.func_decl.return_type->data.type.name != NULL) {
                        return method_decl->data.func_decl.return_type->data.type.name;
                    }
                }
            }
        }
        if (callee != NULL
            && callee->type == AST_IDENTIFIER
            && callee->data.identifier.name != NULL) {
            const char *callee_name = callee->data.identifier.name;
            if (expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *name = expr->data.call.arguments[0]->data.identifier.name;
                const char *inner = NULL;
                if (strcmp(callee_name, "Read") == 0
                    || strcmp(callee_name, "Write") == 0
                    || strcmp(callee_name, "Release") == 0) {
                    inner = llvm_lookup_slot_inner(ctx, name);
                    if (inner == NULL) {
                        LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, name);
                        if (view != NULL)
                            inner = view->inner_type;
                    }
                    if (inner == NULL)
                        inner = llvm_lookup_device_slot_inner(ctx, name);
                    if (inner != NULL && strcmp(callee_name, "Read") == 0)
                        return inner;
                    if (inner != NULL)
                        return "Void";
                }
            }
            const char *declared_ret = llvm_stmt_declared_return_type_name(ctx,
                callee_name);
            if (declared_ret != NULL)
                return declared_ret;
        }
        return NULL;
    default:
        return NULL;
    }
}

const char *
llvm_infer_spawn_future_inner(LLVMGenCtx *ctx, ASTNode *spawn_expr)
{
    ASTNode *target = spawn_expr != NULL ? spawn_expr->data.spawn_expr.function : NULL;
    ASTNode *call = NULL;
    ASTNode *callee = target;
    const char *callee_name = NULL;
    static char buf[128];

    if (target != NULL && target->type == AST_CALL) {
        call = target;
        callee = target->data.call.callee;
    }
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = callee->data.identifier.name;
    if (callee_name == NULL)
        return NULL;

    ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx, callee_name);
    if (decl == NULL || decl->data.func_decl.return_type == NULL
        || decl->data.func_decl.return_type->type != AST_TYPE
        || decl->data.func_decl.return_type->data.type.name == NULL) {
        return NULL;
    }

    const char *ret_name = decl->data.func_decl.return_type->data.type.name;
    if (!(ret_name[0] >= 'A' && ret_name[0] <= 'Z' && ret_name[1] == '\0'))
        return ret_name;

    if (call == NULL)
        return NULL;

    for (size_t i = 0; i < decl->data.func_decl.param_count
         && i < call->data.call.arg_count; i++) {
        FuncParam *param = decl->data.func_decl.params[i];
        if (param == NULL || param->type == NULL || param->type->type != AST_TYPE
            || param->type->data.type.name == NULL)
            continue;
        if (strcmp(param->type->data.type.name, ret_name) == 0) {
            const char *actual_type = llvm_simple_expr_type_name(ctx,
                call->data.call.arguments[i]);
            if (actual_type == NULL || actual_type[0] == '\0')
                continue;
            snprintf(buf, sizeof(buf), "%s", actual_type);
            return buf;
        }
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
