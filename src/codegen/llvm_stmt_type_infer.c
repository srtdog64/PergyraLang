#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

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

const char *
llvm_stmt_infer_nominal_name_from_init(LLVMGenCtx *ctx, ASTNode *init)
{
    const char *name;

    if (ctx == NULL || init == NULL)
        return NULL;

    if (init->type == AST_IDENTIFIER && init->data.identifier.name != NULL) {
        name = init->data.identifier.name;
        if (llvm_scope_lookup(ctx, name) != NULL) {
            const char *tracked = llvm_lookup_var_class(ctx, name);
            if (tracked != NULL)
                return tracked;
        }
        if (strcmp(name, "self") != 0) {
            ASTNode *host_decl = llvm_current_host_decl(ctx);
            const char *host_name = llvm_decl_node_name(host_decl);
            LLVMClassTypeEntry *host_cls =
                host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
            if (host_cls != NULL) {
                int field_idx = llvm_class_field_index(host_cls, name);
                if (field_idx >= 0) {
                    LLVMClassTypeEntry *field_cls = llvm_stmt_lookup_class_by_type(
                        ctx, host_cls->fields[field_idx].field_type);
                    if (field_cls != NULL)
                        return field_cls->class_name;
                }
            }
        }
        return NULL;
    }

    if (init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.callee->data.identifier.name != NULL) {
        name = init->data.call.callee->data.identifier.name;
        if ((strcmp(name, "ListGet") == 0 || strcmp(name, "QueuePop") == 0)
            && init->data.call.arg_count >= 1
            && init->data.call.arguments[0] != NULL
            && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
            const char *collection = init->data.call.arguments[0]->data.identifier.name;
            const char *inner = strcmp(name, "ListGet") == 0
                ? llvm_lookup_list_inner(ctx, collection)
                : llvm_lookup_queue_inner(ctx, collection);
            if (inner != NULL && llvm_lookup_class(ctx, inner) != NULL)
                return inner;
        }
        if (strcmp(name, "MapGet") == 0
            && init->data.call.arg_count >= 1
            && init->data.call.arguments[0] != NULL
            && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
            const char *collection = init->data.call.arguments[0]->data.identifier.name;
            const char *value = llvm_lookup_map_value(ctx, collection);
            if (value != NULL && llvm_lookup_class(ctx, value) != NULL)
                return value;
        }
        if (llvm_lookup_class(ctx, name) != NULL)
            return name;
        {
            LLVMFuncEntry *callee_fn = llvm_lookup_function(ctx, name);
            if (callee_fn == NULL) {
                ASTNode *host_decl = llvm_current_host_decl(ctx);
                const char *host_name = llvm_decl_node_name(host_decl);
                char full_name[256];
                if (host_name != NULL) {
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                        host_name, name);
                    callee_fn = llvm_lookup_function(ctx, full_name);
                }
            }
            LLVMClassTypeEntry *ret_cls = callee_fn != NULL
                ? llvm_stmt_lookup_class_by_type(ctx, callee_fn->ret_type)
                : NULL;
            if (ret_cls != NULL)
                return ret_cls->class_name;
        }
    }

    if (init->type == AST_MEMBER_ACCESS
        && init->data.member.object != NULL
        && init->data.member.name != NULL) {
        const char *base_name = llvm_stmt_infer_nominal_name_from_init(
            ctx, init->data.member.object);
        LLVMClassTypeEntry *base_cls = base_name != NULL
            ? llvm_lookup_class(ctx, base_name) : NULL;
        if (base_cls != NULL) {
            int field_idx = llvm_class_field_index(base_cls, init->data.member.name);
            if (field_idx >= 0) {
                LLVMClassTypeEntry *field_cls = llvm_stmt_lookup_class_by_type(
                    ctx, base_cls->fields[field_idx].field_type);
                if (field_cls != NULL)
                    return field_cls->class_name;
            }
        }
    }

    return NULL;
}

LLVMTypeRef
llvm_stmt_infer_expr_type(LLVMGenCtx *ctx, ASTNode *expr)
{
    const char *nominal_name;
    LLVMClassTypeEntry *nominal_cls;

    if (ctx == NULL || expr == NULL)
        return ctx->type_i32;

    nominal_name = llvm_stmt_infer_nominal_name_from_init(ctx, expr);
    nominal_cls = nominal_name != NULL ? llvm_lookup_class(ctx, nominal_name) : NULL;
    if (nominal_cls != NULL)
        return nominal_cls->struct_type;

    switch (expr->type) {
    case AST_STRING:
        return ctx->type_i8ptr;
    case AST_BOOLEAN:
        return ctx->type_i1;
    case AST_NUMBER:
        return ctx->type_i32;
    case AST_ARRAY_LITERAL: {
        LLVMTypeRef elem_type = ctx->type_i32;
        const char *suffix = "Int";
        if (expr->data.array_literal.count > 0
            && expr->data.array_literal.elements != NULL
            && expr->data.array_literal.elements[0] != NULL) {
            elem_type = llvm_stmt_infer_expr_type(ctx,
                expr->data.array_literal.elements[0]);
            suffix = llvm_type_to_suffix(ctx, elem_type);
            if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
                suffix = "Int";
        }
        return llvm_array_struct_type(ctx, suffix);
    }
    case AST_IDENTIFIER: {
        LLVMVarEntry *var = llvm_scope_lookup(ctx, expr->data.identifier.name);
        return var != NULL ? var->type : ctx->type_i32;
    }
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
        return ctx->type_i32;
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
            const char *inner = receiver_name != NULL
                ? llvm_lookup_slot_inner(ctx, receiver_name) : NULL;
            if (inner == NULL) {
                LLVMViewVarEntry *view = receiver_name != NULL
                    ? llvm_lookup_view_var(ctx, receiver_name) : NULL;
                if (view != NULL)
                    inner = view->inner_type;
            }
            if (inner == NULL)
                inner = receiver_name != NULL
                    ? llvm_lookup_device_slot_inner(ctx, receiver_name) : NULL;
            if (inner != NULL && strcmp(method_name, "Read") == 0)
                return pergyra_type_to_llvm(ctx, inner);
            if (inner != NULL
                && (strcmp(method_name, "Write") == 0
                    || strcmp(method_name, "Release") == 0)) {
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
                    LLVMTypeRef slice_ty = llvm_slice_struct_type(ctx,
                        elem_name != NULL ? elem_name : "Int");
                    return slice_ty;
                }
            }
        }
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER
            && expr->data.call.callee->data.identifier.name != NULL) {
            const char *callee = expr->data.call.callee->data.identifier.name;
            if ((strcmp(callee, "Read") == 0
                 || strcmp(callee, "Write") == 0
                 || strcmp(callee, "Release") == 0)
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *receiver_name =
                    expr->data.call.arguments[0]->data.identifier.name;
                const char *inner = llvm_lookup_slot_inner(ctx, receiver_name);
                if (inner == NULL) {
                    LLVMViewVarEntry *view = llvm_lookup_view_var(ctx, receiver_name);
                    if (view != NULL)
                        inner = view->inner_type;
                }
                if (inner == NULL)
                    inner = llvm_lookup_device_slot_inner(ctx, receiver_name);
                if (inner != NULL && strcmp(callee, "Read") == 0)
                    return pergyra_type_to_llvm(ctx, inner);
                if (inner != NULL)
                    return ctx->type_void;
            }
            LLVMFuncEntry *fn = llvm_lookup_function(ctx, callee);
            ASTNode *host_decl = llvm_current_host_decl(ctx);
            const char *host_name = llvm_decl_node_name(host_decl);
            if (fn == NULL && host_name != NULL) {
                char full_name[256];
                snprintf(full_name, sizeof(full_name), "%s_%s",
                    host_name, callee);
                fn = llvm_lookup_function(ctx, full_name);
            }
            if (fn != NULL)
                return fn->ret_type;
            if (strcmp(callee, "ToString") == 0
                || strcmp(callee, "ReadFile") == 0
                || strcmp(callee, "Input") == 0
                || strcmp(callee, "Upper") == 0
                || strcmp(callee, "ToUpper") == 0
                || strcmp(callee, "Lower") == 0
                || strcmp(callee, "ToLower") == 0
                || strcmp(callee, "Concat") == 0
                || strcmp(callee, "StringConcat") == 0) {
                return ctx->type_i8ptr;
            }
            if (strcmp(callee, "ListGet") == 0
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *inner = llvm_lookup_list_inner(
                    ctx, expr->data.call.arguments[0]->data.identifier.name);
                if (inner != NULL)
                    return pergyra_type_to_llvm(ctx, inner);
            }
            if (strcmp(callee, "QueuePop") == 0
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *inner = llvm_lookup_queue_inner(
                    ctx, expr->data.call.arguments[0]->data.identifier.name);
                if (inner != NULL)
                    return pergyra_type_to_llvm(ctx, inner);
            }
            if (strcmp(callee, "MapGet") == 0
                && expr->data.call.arg_count >= 1
                && expr->data.call.arguments[0] != NULL
                && expr->data.call.arguments[0]->type == AST_IDENTIFIER) {
                const char *value = llvm_lookup_map_value(
                    ctx, expr->data.call.arguments[0]->data.identifier.name);
                if (value != NULL)
                    return pergyra_type_to_llvm(ctx, value);
            }
            if (strcmp(callee, "ListSize") == 0
                || strcmp(callee, "QueueSize") == 0
                || strcmp(callee, "MapSize") == 0) {
                return ctx->type_i32;
            }
            if (strcmp(callee, "QueueEmpty") == 0
                || strcmp(callee, "MapHas") == 0) {
                return ctx->type_i1;
            }
            if (strcmp(callee, "HasZone") == 0
                || strcmp(callee, "HasState") == 0
                || strcmp(callee, "HasLayer") == 0
                || strcmp(callee, "HasProjection") == 0) {
                return ctx->type_i1;
            }
        }
        return ctx->type_i32;
    case AST_BINARY: {
        PgyTokenType op = expr->data.binary.op.type;
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return ctx->type_i1;
        }
        if (op == TOKEN_PLUS) {
            LLVMTypeRef left_ty = llvm_stmt_infer_expr_type(ctx, expr->data.binary.left);
            LLVMTypeRef right_ty = llvm_stmt_infer_expr_type(ctx, expr->data.binary.right);
            if (left_ty == ctx->type_i8ptr || right_ty == ctx->type_i8ptr)
                return ctx->type_i8ptr;
        }
        return ctx->type_i32;
    }
    default:
        return ctx->type_i32;
    }
}

LLVMTypeRef
llvm_stmt_resolve_array_elem_type(LLVMGenCtx *ctx, ASTNode *expr,
                                  LLVMValueRef data_ptr)
{
    LLVMTypeRef elem_type = ctx->type_i32;
    (void)data_ptr;

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
                    LLVMTypeRef declared = pergyra_type_to_llvm(
                        ctx, elem_name != NULL ? elem_name : "Int");
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
