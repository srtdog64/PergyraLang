#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "codegen_slot_type_policy.h"
#include "parser/ast_api.h"

const char *
llvm_stmt_render_type_annotation_copy(LLVMGenCtx *ctx, ASTNode *type_ann)
{
    char buf[256];
    size_t offset;
    GenericParams *generic_args;

    if (ast_type_name(type_ann) == NULL)
        return NULL;
    generic_args = ast_type_generic_args(type_ann);
    size_t generic_count = ast_generic_param_count(generic_args);
    if (generic_count == 0) {
        if (ctx == NULL)
            return ast_type_name(type_ann);
        return pgy_arena_strdup(&ctx->scratch, ast_type_name(type_ann));
    }

    {
        int written = snprintf(buf, sizeof(buf), "%s<",
                               ast_type_name(type_ann));
        if (written < 0 || (size_t)written >= sizeof(buf))
            return NULL;
        offset = (size_t)written;
    }
    for (size_t i = 0; i < generic_count; i++) {
        char *arg = llvm_stmt_render_type_arg(
            ast_generic_param_at(generic_args, i));
        if (arg == NULL || arg[0] == '\0') {
            free(arg);
            return NULL;
        }
        int written = snprintf(buf + offset, sizeof(buf) - offset,
                               "%s%s", i == 0 ? "" : ", ",
                               arg);
        free(arg);
        if (written < 0)
            return NULL;
        if ((size_t)written >= sizeof(buf) - offset)
            return NULL;
        offset += (size_t)written;
    }
    if (offset + 1 < sizeof(buf)) {
        buf[offset++] = '>';
        buf[offset] = '\0';
    } else {
        return NULL;
    }
    if (ctx == NULL)
        return NULL;
    return pgy_arena_strdup(&ctx->scratch, buf);
}

static const char *
llvm_stmt_declared_return_type_name(LLVMGenCtx *ctx, const char *name)
{
    ASTNode *decl;

    if (ctx == NULL || name == NULL)
        return NULL;

    decl = llvm_stmt_find_function_decl_by_name(ctx, name);
    ASTNode *return_type = ast_func_return_type(decl);
    if (ast_type_name(return_type) == NULL) {
        return NULL;
    }

    return ast_type_name(return_type);
}

LLVMValueRef
llvm_stmt_create_slot_alloca(LLVMGenCtx *ctx, LLVMTypeRef type, const char *name)
{
    return llvm_create_entry_alloca(ctx, type, name);
}

LLVMTypeRef
llvm_stmt_lambda_signature_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    if (ctx == NULL || expr == NULL || expr->type != AST_LAMBDA_EXPR)
        return NULL;

    int pc = (int)ast_lambda_param_count(expr);
    LLVMTypeRef *params = NULL;
    LLVMTypeRef ret_type = ctx->type_i32;

    if (ast_lambda_return_type(expr) != NULL) {
        ret_type = ast_type_to_llvm(ctx, ast_lambda_return_type(expr));
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
            ASTNode *p = ast_lambda_param(expr, (size_t)i);
            ASTNode *p_type = p != NULL && p->type == AST_LET_DECL
                ? ast_let_type(p)
                : NULL;
            if (p_type != NULL) {
                params[i] = ast_type_to_llvm(ctx, p_type);
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
        LLVMVarEntry *entry = llvm_scope_lookup(ctx, ast_identifier_name(expr));
        if (entry != NULL)
            return llvm_type_to_suffix(ctx, entry->type);
        return NULL;
    }
    case AST_CALL:
        callee = ast_call_callee(expr);
        if (callee != NULL
            && callee->type == AST_MEMBER_ACCESS
            && ast_member_name(callee) != NULL) {
            receiver = ast_member_object(callee);
            method_name = ast_member_name(callee);
            if (receiver != NULL && receiver->type == AST_IDENTIFIER) {
                const char *name = ast_identifier_name(receiver);
                const char *inner = llvm_lookup_slot_inner(ctx, name);
                if (inner == NULL) {
                    LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, name);
                    if (view != NULL)
                        inner = view->inner_type;
                }
                if (inner == NULL)
                    inner = llvm_lookup_device_slot_inner(ctx, name);
                if (inner != NULL
                    && pgy_codegen_call_name_is_read(method_name))
                    return inner;
                if (inner != NULL
                    && (pgy_codegen_call_name_is_write(method_name)
                        || pgy_codegen_call_name_is_release(method_name))) {
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
                    ASTNode *method_return_type = ast_func_return_type(method_decl);
                    if (ast_type_name(method_return_type) != NULL) {
                        return ast_type_name(method_return_type);
                    }
                }
            }
        }
        if (callee != NULL
            && callee->type == AST_IDENTIFIER
            && ast_identifier_name(callee) != NULL) {
            const char *callee_name = ast_identifier_name(callee);
            if (ast_call_arg_count(expr) >= 1
                && ast_call_argument(expr, 0) != NULL
                && ast_call_argument(expr, 0)->type == AST_IDENTIFIER) {
                const char *name =
                    ast_identifier_name(ast_call_argument(expr, 0));
                const char *inner = NULL;
                if (pgy_codegen_call_name_is_slot_operation(callee_name)) {
                    inner = llvm_lookup_slot_inner(ctx, name);
                    if (inner == NULL) {
                        LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, name);
                        if (view != NULL)
                            inner = view->inner_type;
                    }
                    if (inner == NULL)
                        inner = llvm_lookup_device_slot_inner(ctx, name);
                    if (inner != NULL
                        && pgy_codegen_call_name_is_read(callee_name))
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
    ASTNode *target = ast_spawn_function(spawn_expr);
    ASTNode *call = NULL;
    ASTNode *callee = target;
    const char *callee_name = NULL;

    if (target != NULL && target->type == AST_CALL) {
        call = target;
        callee = ast_call_callee(target);
    }
    if (callee != NULL && callee->type == AST_IDENTIFIER)
        callee_name = ast_identifier_name(callee);
    if (callee_name == NULL)
        return NULL;

    ASTNode *decl = llvm_stmt_find_function_decl_by_name(ctx, callee_name);
    ASTNode *return_type = ast_func_return_type(decl);
    if (ast_type_name(return_type) == NULL) {
        return NULL;
    }

    const char *ret_name = ast_type_name(return_type);
    if (!(ret_name[0] >= 'A' && ret_name[0] <= 'Z' && ret_name[1] == '\0'))
        return ret_name;

    if (call == NULL)
        return NULL;

    size_t param_count = ast_func_param_count(decl);
    for (size_t i = 0; i < param_count && i < ast_call_arg_count(call); i++) {
        FuncParam *param = ast_func_param(decl, i);
        if (param == NULL || ast_type_name(param->type) == NULL)
            continue;
        if (strcmp(ast_type_name(param->type), ret_name) == 0) {
            const char *actual_type = llvm_simple_expr_type_name(ctx,
                ast_call_argument(call, i));
            if (actual_type == NULL || actual_type[0] == '\0')
                continue;
            return ctx != NULL
                ? pgy_arena_strdup(&ctx->scratch, actual_type)
                : actual_type;
        }
    }

    return NULL;
}

#endif /* PGY_LLVM_ENABLED */
