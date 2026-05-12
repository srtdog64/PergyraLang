#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_stmt_type_infer_helpers.h"

#include <stdarg.h>
static bool
llvm_stmt_type_reasonf(char *out, size_t out_size, const char *fmt, ...)
{
    va_list ap;
    int written;

    if (out == NULL || out_size == 0 || fmt == NULL)
        return false;
    va_start(ap, fmt);
    written = vsnprintf(out, out_size, fmt, ap);
    va_end(ap);
    return written >= 0 && (size_t)written < out_size;
}

static LLVMTypeRef
llvm_stmt_expected_array_elem_type(LLVMGenCtx *ctx)
{
    const char *inner;

    if (ctx == NULL || ctx->expected_type_name == NULL)
        return NULL;
    if (strncmp(ctx->expected_type_name, "Array<", 6) != 0
        && strncmp(ctx->expected_type_name, "Slice<", 6) != 0)
        return NULL;
    inner = llvm_constructed_arg_name_at(ctx->expected_type_name, 0);
    if (inner == NULL || inner[0] == '\0'
        || strcmp(inner, "Unknown") == 0)
        return NULL;
    return pergyra_type_to_llvm(ctx, inner);
}

LLVMClassTypeEntry *
llvm_stmt_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    if (ctx == NULL || type == NULL)
        return NULL;

    for (int i = 0; i < ctx->class_type_count; i++) {
        if (ctx->class_types[i].struct_type == type)
            return &ctx->class_types[i];
    }
    return NULL;
}

static LLVMTypeRef
llvm_stmt_unknown_expr_type(LLVMGenCtx *ctx, ASTNode *expr, const char *reason)
{
    if (ctx == NULL)
        return NULL;
    if (!ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, expr,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM expression type inference requires a concrete type: %s",
            reason != NULL ? reason : "unknown expression");
    }
    return ctx->type_i32;
}

LLVMTypeRef
llvm_stmt_infer_expr_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    const char *nominal_name;
    LLVMClassTypeEntry *nominal_cls;

    if (ctx == NULL)
        return NULL;
    if (expr == NULL)
        return llvm_stmt_unknown_expr_type(ctx, expr, "missing expression");

    nominal_name = llvm_stmt_infer_nominal_name_from_init(ctx, expr);
    nominal_cls = nominal_name != NULL ? llvm_lookup_class(ctx, nominal_name) : NULL;
    if (nominal_cls != NULL)
        return nominal_cls->struct_type;

    switch (expr->type) {
    case AST_STRING:
        return ctx->type_i8ptr;
    case AST_BOOLEAN:
        return ctx->type_i1;
    case AST_NUMBER: {
        double val = expr->data.number.value;
        if (expr->data.number.is_long)
            return ctx->type_i64;
        if (val == (int64_t)val
            && val >= -2147483648.0
            && val <= 2147483647.0) {
            return ctx->type_i32;
        }
        if (val == (double)(int64_t)val
            && val >= -9.2233720368547758e+18
            && val <=  9.2233720368547758e+18) {
            return ctx->type_i64;
        }
        return ctx->type_f64;
    }
    case AST_ARRAY_LITERAL: {
        LLVMTypeRef elem_type = NULL;
        const char *suffix = NULL;
        if (expr->data.array_literal.count > 0
            && expr->data.array_literal.elements != NULL
            && expr->data.array_literal.elements[0] != NULL) {
            elem_type = llvm_stmt_infer_expr_type(ctx,
                expr->data.array_literal.elements[0]);
            suffix = llvm_type_to_suffix(ctx, elem_type);
            if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "array literal element type is unresolved");
        } else if (ctx->expected_type_name != NULL
                   && strncmp(ctx->expected_type_name, "Array<", 6) == 0) {
            suffix = llvm_constructed_arg_name_at(ctx->expected_type_name, 0);
        }
        if (suffix == NULL || suffix[0] == '\0'
            || strcmp(suffix, "Unknown") == 0)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "empty array literal requires an explicit Array<T> context");
        return llvm_array_struct_type(ctx, suffix);
    }
    case AST_IDENTIFIER: {
        LLVMVarEntry *var = llvm_scope_lookup(ctx, expr->data.identifier.name);
        if (var != NULL)
            return var->type;
        /* MIR local allocation can ask for loop induction variables before
         * the value inventory has registered them. Keep this as poison i32
         * until loop locals are typed directly from MIR facts. */
        return ctx->type_i32;
    }
    case AST_ASSIGNMENT:
        if (expr->data.assignment.target != NULL
            && expr->data.assignment.target->type == AST_IDENTIFIER) {
            LLVMVarEntry *var = llvm_scope_lookup(ctx,
                expr->data.assignment.target->data.identifier.name);
            if (var != NULL)
                return var->type;
        }
        if (expr->data.assignment.value != NULL)
            return llvm_stmt_infer_expr_type(ctx, expr->data.assignment.value);
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "assignment is missing a value expression");
    case AST_CHANNEL_RECV:
        if (expr->data.channel_recv.channel != NULL
            && expr->data.channel_recv.channel->type == AST_IDENTIFIER) {
            const char *name =
                expr->data.channel_recv.channel->data.identifier.name;
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (inner != NULL)
                return pergyra_type_to_llvm(ctx, inner);
            if (name != NULL && llvm_scope_lookup(ctx, name) != NULL) {
                char reason[256];
                if (!llvm_stmt_type_reasonf(reason, sizeof(reason),
                        "channel receive '%s' has no registered Channel<T> metadata",
                        name)) {
                    return llvm_stmt_unknown_expr_type(ctx, expr,
                        "channel receive has no registered Channel<T> metadata");
                }
                return llvm_stmt_unknown_expr_type(ctx, expr, reason);
            }
        }
        /* Select lowering can allocate receive temporaries before channel
         * inner metadata is registered. If the enclosing let/return already
         * supplied a concrete expected value type, use that instead of
         * inventing poison i32. */
        if (ctx->expected_type_name != NULL
            && strncmp(ctx->expected_type_name, "Channel<", 8) != 0) {
            LLVMTypeRef expected = pergyra_type_to_llvm(
                ctx, ctx->expected_type_name);
            if (expected != NULL)
                return expected;
        }
        return ctx->type_i32;
    case AST_MEMBER_ACCESS: {
        const char *base_name = llvm_stmt_infer_nominal_name_from_init(
            ctx, expr->data.member.object);
        LLVMClassTypeEntry *base_cls = base_name != NULL
            ? llvm_lookup_class(ctx, base_name) : NULL;
        if (base_cls != NULL) {
            int field_idx = llvm_class_field_index(base_cls, expr->data.member.name);
            if (field_idx >= 0)
                return base_cls->fields[field_idx].field_type;
        }
        {
            char reason[256];
            const char *field_name = expr->data.member.name != NULL
                ? expr->data.member.name : "<field>";
            const char *base_expr_name =
                (expr->data.member.object != NULL
                 && expr->data.member.object->type == AST_IDENTIFIER
                 && expr->data.member.object->data.identifier.name != NULL)
                    ? expr->data.member.object->data.identifier.name
                    : NULL;
            if (!llvm_stmt_type_reasonf(reason, sizeof(reason),
                    "member access '%s:%s.%s' did not resolve to a known field",
                    base_expr_name != NULL ? base_expr_name : "<expr>",
                    base_name != NULL ? base_name : "<unknown>",
                    field_name)) {
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "member access did not resolve to a known field");
            }
            return llvm_stmt_unknown_expr_type(ctx, expr, reason);
        }
    }
    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_MEMBER_ACCESS
            && expr->data.call.callee->data.member.name != NULL
            && expr->data.call.callee->data.member.object != NULL) {
            ASTNode *receiver = expr->data.call.callee->data.member.object;
            const char *method_name = expr->data.call.callee->data.member.name;
            const char *receiver_name = receiver->type == AST_IDENTIFIER
                ? receiver->data.identifier.name : NULL;
            const char *inner = llvm_stmt_lookup_slot_or_view_inner(
                ctx, receiver_name);
            if (inner != NULL && llvm_stmt_slot_call_returns_value(method_name))
                return pergyra_type_to_llvm(ctx, inner);
            if (inner != NULL
                && llvm_stmt_call_is_slot_builtin(method_name)) {
                return ctx->type_void;
            }
            if (strcmp(method_name, "Slice") == 0 && receiver_name != NULL) {
                LLVMArrayVarEntry *entry = llvm_lookup_array_var(ctx, receiver_name);
                if (entry != NULL) {
                    const char *suffix = llvm_type_to_suffix(ctx, entry->elem_type);
                    if (suffix != NULL && strcmp(suffix, "Unknown") != 0)
                        return llvm_slice_struct_type(ctx, suffix);
                }
            }
            if (strcmp(method_name, "Slice") == 0
                && receiver->type == AST_CALL
                && receiver->data.call.callee != NULL
                && receiver->data.call.callee->type == AST_IDENTIFIER
                && receiver->data.call.callee->data.identifier.name != NULL) {
                ASTNode *decl = llvm_stmt_find_function_decl_by_name(
                    ctx, receiver->data.call.callee->data.identifier.name);
                if (decl != NULL
                    && decl->type == AST_FUNC_DECL
                    && decl->data.func_decl.return_type != NULL
                    && decl->data.func_decl.return_type->type == AST_TYPE
                    && decl->data.func_decl.return_type->data.type.name != NULL
                    && (strcmp(decl->data.func_decl.return_type->data.type.name, "Array") == 0
                        || strcmp(decl->data.func_decl.return_type->data.type.name, "Slice") == 0)
                    && decl->data.func_decl.return_type->data.type.generic_args != NULL
                    && decl->data.func_decl.return_type->data.type.generic_args->count >= 1
                    && decl->data.func_decl.return_type->data.type.generic_args->params[0] != NULL) {
                    char *elem_name = llvm_stmt_render_type_arg_scratch(
                        decl->data.func_decl.return_type->data.type.generic_args->params[0],
                        &ctx->scratch);
                    if (elem_name == NULL) {
                        return llvm_stmt_unknown_expr_type(ctx, expr,
                            "Slice() receiver return type is missing its element type");
                    }
                    LLVMTypeRef slice_ty = llvm_slice_struct_type(ctx, elem_name);
                    return slice_ty;
                }
            }
        }
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER
            && expr->data.call.callee->data.identifier.name != NULL) {
            const char *callee = expr->data.call.callee->data.identifier.name;
            if (llvm_stmt_call_is_slot_builtin(callee)
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *receiver_name =
                    expr->data.call.arguments[0]->data.identifier.name;
                const char *inner = llvm_stmt_lookup_slot_or_view_inner(
                    ctx, receiver_name);
                if (inner != NULL && llvm_stmt_slot_call_returns_value(callee))
                    return pergyra_type_to_llvm(ctx, inner);
                if (inner != NULL)
                    return ctx->type_void;
            }
            LLVMFuncEntry *fn = llvm_stmt_lookup_visible_function(ctx, callee);
            if (fn != NULL)
                return fn->ret_type;
            {
                LLVMTypeRef declared_type =
                    llvm_stmt_lookup_declared_call_return_type(ctx, callee);
                if (declared_type != NULL)
                    return declared_type;
            }
            {
                LLVMTypeRef builtin_type =
                    llvm_stmt_infer_scalar_builtin_type(ctx, callee);
                if (builtin_type != NULL)
                    return builtin_type;
            }
            if (llvm_stmt_call_returns_collection_value(callee)
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *inner = llvm_stmt_lookup_collection_get_inner(
                    ctx, callee,
                    expr->data.call.arguments[0]->data.identifier.name);
                if (inner != NULL)
                    return pergyra_type_to_llvm(ctx, inner);
            }
            if (llvm_stmt_call_returns_collection_size(callee))
                return ctx->type_i32;
            if (llvm_stmt_call_returns_collection_bool(callee))
                return ctx->type_i1;
            if (llvm_stmt_call_returns_domain_bool(callee))
                return ctx->type_i1;
        }
        /* Domain helper calls can be emitted before their final lowered helper
         * entry is visible in the LLVM function inventory. Prefer the
         * enclosing concrete let/return context when it exists; otherwise keep
         * poison i32 until call result facts are carried directly by MIR. */
        if (ctx->expected_type_name != NULL) {
            LLVMTypeRef expected = pergyra_type_to_llvm(
                ctx, ctx->expected_type_name);
            if (expected != NULL)
                return expected;
        }
        return ctx->type_i32;
    case AST_BINARY: {
        PgyTokenType op = expr->data.binary.op.type;
        LLVMTypeRef left_ty = NULL;
        LLVMTypeRef right_ty = NULL;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return ctx->type_i1;
        }
        if (op == TOKEN_PLUS) {
            left_ty = llvm_stmt_infer_expr_type(ctx, expr->data.binary.left);
            right_ty = llvm_stmt_infer_expr_type(ctx, expr->data.binary.right);
            if (left_ty == ctx->type_i8ptr || right_ty == ctx->type_i8ptr)
                return ctx->type_i8ptr;
            return llvm_stmt_promote_numeric_type(ctx, left_ty, right_ty);
        }
        if (op == TOKEN_MINUS || op == TOKEN_STAR || op == TOKEN_SLASH
            || op == TOKEN_PERCENT) {
            left_ty = llvm_stmt_infer_expr_type(ctx, expr->data.binary.left);
            right_ty = llvm_stmt_infer_expr_type(ctx, expr->data.binary.right);
            return llvm_stmt_promote_numeric_type(ctx, left_ty, right_ty);
        }
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "unsupported binary operator has no inferred LLVM type");
    }
    case AST_SPAWN_EXPR:
        return ctx->type_task_handle;
    case AST_AWAIT_EXPR:
        return llvm_stmt_infer_await_expr_type(ctx, expr);
    case AST_ASYNC_BLOCK:
        return ctx->type_task_handle;
    case AST_TASK_GROUP:
        return ctx->type_void;
    case AST_SELECT_STMT:
    case AST_CHANNEL_SEND:
    case AST_RETURN:
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_BLOCK:
    case AST_IF_STMT:
    case AST_WHILE_LOOP:
    case AST_FOR_LOOP:
    case AST_PARALLEL_BLOCK:
    case AST_DEFER_STMT:
        return ctx->type_void;
    default:
        /* MIR local allocation still reaches a few statement-shaped AST nodes
         * before typed MIR result facts are complete. Keep poison i32 here;
         * concrete expression gaps above should use llvm_stmt_unknown_expr_type. */
        return ctx->type_i32;
    }
}

LLVMTypeRef
llvm_stmt_resolve_array_elem_type(LLVMGenCtx *ctx, ASTNode *expr,
                                  LLVMValueRef data_ptr)
{
    LLVMTypeRef elem_type = llvm_stmt_expected_array_elem_type(ctx);
    (void)data_ptr;

    if (elem_type == NULL)
        elem_type = ctx->type_i32;

    if (expr == NULL)
        return elem_type;

    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        LLVMArrayVarEntry *entry = llvm_lookup_array_var(
            ctx, expr->data.identifier.name);
        if (entry != NULL && entry->elem_type != NULL)
            return entry->elem_type;
    }

    if (expr->type == AST_ARRAY_LITERAL
        && expr->data.array_literal.count > 0
        && expr->data.array_literal.elements != NULL
        && expr->data.array_literal.elements[0] != NULL) {
        LLVMTypeRef inferred = llvm_stmt_infer_expr_type(
            ctx, expr->data.array_literal.elements[0]);
        if (inferred != NULL)
            return inferred;
    }

    if (expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_MEMBER_ACCESS
        && expr->data.call.callee->data.member.name != NULL
        && strcmp(expr->data.call.callee->data.member.name, "Slice") == 0
        && expr->data.call.callee->data.member.object != NULL) {
        ASTNode *receiver = expr->data.call.callee->data.member.object;
        if (receiver->type == AST_IDENTIFIER && receiver->data.identifier.name != NULL) {
            LLVMArrayVarEntry *entry = llvm_lookup_array_var(
                ctx, receiver->data.identifier.name);
            if (entry != NULL && entry->elem_type != NULL)
                return entry->elem_type;
        }
        if (receiver->type == AST_CALL
            && receiver->data.call.callee != NULL
            && receiver->data.call.callee->type == AST_IDENTIFIER
            && receiver->data.call.callee->data.identifier.name != NULL) {
            ASTNode *decl = llvm_stmt_find_function_decl_by_name(
                ctx, receiver->data.call.callee->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && decl->data.func_decl.return_type != NULL
                && decl->data.func_decl.return_type->type == AST_TYPE) {
                ASTNode *ret = decl->data.func_decl.return_type;
                if (ret->data.type.name != NULL
                    && (strcmp(ret->data.type.name, "Array") == 0
                        || strcmp(ret->data.type.name, "Slice") == 0)
                    && ret->data.type.generic_args != NULL
                    && ret->data.type.generic_args->count >= 1
                    && ret->data.type.generic_args->params[0] != NULL) {
                    char *elem_name = llvm_stmt_render_type_arg_scratch(
                        ret->data.type.generic_args->params[0],
                        &ctx->scratch);
                    if (elem_name == NULL || elem_name[0] == '\0')
                        return llvm_stmt_unknown_expr_type(ctx, receiver,
                            "array return element type is unresolved");
                    LLVMTypeRef declared = pergyra_type_to_llvm(ctx, elem_name);
                    if (declared != NULL)
                        return declared;
                }
            }
        }
        return llvm_stmt_resolve_array_elem_type(
            ctx, receiver, NULL);
    }

    if (expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL) {
        ASTNode *decl = llvm_stmt_find_function_decl_by_name(
            ctx, expr->data.call.callee->data.identifier.name);
        if (decl != NULL
            && decl->type == AST_FUNC_DECL
            && decl->data.func_decl.return_type != NULL
            && decl->data.func_decl.return_type->type == AST_TYPE) {
            ASTNode *ret = decl->data.func_decl.return_type;
            if (ret->data.type.name != NULL
                && (strcmp(ret->data.type.name, "Array") == 0
                    || strcmp(ret->data.type.name, "Slice") == 0)
                && ret->data.type.generic_args != NULL
                && ret->data.type.generic_args->count >= 1
                && ret->data.type.generic_args->params[0] != NULL) {
                GenericParam *gp = ret->data.type.generic_args->params[0];
                /* Prefer the simple type name ("String", "Int"); fall back
                 * to the explicit constraint node if given. */
                if (gp->name != NULL) {
                    LLVMTypeRef declared = pergyra_type_to_llvm(ctx, gp->name);
                    if (declared != NULL)
                        return declared;
                }
                if (gp->constraint != NULL) {
                    LLVMTypeRef declared = ast_type_to_llvm(ctx, gp->constraint);
                    if (declared != NULL)
                        return declared;
                }
            }
        }
    }

    return elem_type;
}


#endif /* PGY_LLVM_ENABLED */
