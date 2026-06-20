#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_internal_api.h"
#include "llvm_inventory_host_methods.h"
#include "llvm_stmt_source_local_fallback.h"
#include "llvm_stmt_type_infer_helpers.h"
#include "codegen_match_variant_policy.h"
#include "../parser/ast_api.h"

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

LLVMClassTypeEntry *
llvm_stmt_lookup_class_by_type(LLVMGenCtx *ctx, LLVMTypeRef type)
{
    return llvm_lookup_class_by_struct_type(ctx, type);
}

LLVMTypeRef
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
    return NULL;
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
    case AST_LET_DECL: {
        ASTNode *type_ann = ast_let_type(expr);
        ASTNode *initializer = ast_let_initializer(expr);
        LLVMTypeRef var_type = NULL;
        const char *name = ast_let_name(expr);
        LLVMClassTypeEntry *cls = NULL;
        const char *nominal_name = NULL;

        if (type_ann != NULL) {
            var_type = ast_type_to_llvm(ctx, type_ann);
        } else if (initializer != NULL) {
            var_type = llvm_stmt_infer_expr_type(ctx, initializer);
        }

        if (var_type == NULL) {
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "let declaration requires an annotation or initializer type");
        }

        if (name != NULL) {
            llvm_scope_declare(ctx, name, NULL, var_type);
            if (type_ann != NULL && type_ann->type == AST_TYPE && ast_type_name(type_ann) != NULL) {
                const char *ann_name = ast_type_name(type_ann);
                if (llvm_lookup_class(ctx, ann_name) != NULL) {
                    llvm_register_var_class(ctx, name, ann_name);
                }
            } else {
                cls = llvm_stmt_lookup_class_by_type(ctx, var_type);
                if (cls != NULL && cls->class_name != NULL) {
                    llvm_register_var_class(ctx, name, cls->class_name);
                } else if (initializer != NULL) {
                    nominal_name = llvm_stmt_infer_nominal_name_from_init(ctx, initializer);
                    if (nominal_name != NULL) {
                        llvm_register_var_class(ctx, name, nominal_name);
                    }
                }
            }
        }
        return var_type;
    }
    case AST_STRING:
        return ctx->type_i8ptr;
    case AST_BOOLEAN:
        return ctx->type_i1;
    case AST_TYPE_TEST:
        return ctx->type_i1;
    case AST_NUMBER: {
        double val = ast_number_value(expr);
        if (ast_number_is_long(expr))
            return ctx->type_i64;
        if (ast_number_is_float(expr))
            return ctx->type_f32;
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
    case AST_CAST: {
        const char *target = ast_cast_target_type(expr);
        LLVMTypeRef target_type = target != NULL
            ? pergyra_type_to_llvm(ctx, target)
            : NULL;
        if (target_type != NULL)
            return target_type;
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "cast expression requires a concrete target type");
    }
    case AST_ARRAY_LITERAL: {
        LLVMTypeRef elem_type = NULL;
        const char *suffix = NULL;
        char suffix_buf[256];
        if (ast_array_literal_count(expr) > 0
            && ast_array_literal_element(expr, 0) != NULL) {
            elem_type = llvm_stmt_infer_expr_type(ctx,
                ast_array_literal_element(expr, 0));
            suffix = llvm_type_to_suffix(ctx, elem_type);
            if (suffix == NULL || strcmp(suffix, "Unknown") == 0)
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "array literal element type is unresolved");
        } else if (ctx->expected_type_name != NULL
                   && pgy_classify_type(ctx->expected_type_name)
                        == PGY_TK_ARRAY) {
            if (llvm_constructed_arg_name_copy(ctx->expected_type_name, 0,
                    suffix_buf, sizeof(suffix_buf))) {
                suffix = suffix_buf;
            }
        }
        if (suffix == NULL || suffix[0] == '\0'
            || strcmp(suffix, "Unknown") == 0)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "empty array literal requires an explicit Array<T> context");
        if (strlen(suffix) >= sizeof(suffix_buf))
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "array literal element type name is too long");
        memcpy(suffix_buf, suffix, strlen(suffix) + 1);
        suffix = suffix_buf;
        return llvm_array_struct_type(ctx, suffix);
    }
    case AST_MAP_LITERAL:
        if (ctx->expected_type_name != NULL
            && strncmp(ctx->expected_type_name, "HashMap<", 8) == 0) {
            LLVMTypeRef expected = pergyra_type_to_llvm(
                ctx, ctx->expected_type_name);
            if (expected != NULL)
                return expected;
        }
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "map literal requires an explicit HashMap<K,V> context");
    case AST_SET_LITERAL:
        if (ctx->expected_type_name != NULL
            && strncmp(ctx->expected_type_name, "Set<", 4) == 0) {
            LLVMTypeRef expected = pergyra_type_to_llvm(
                ctx, ctx->expected_type_name);
            if (expected != NULL)
                return expected;
        }
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "set literal requires an explicit Set<T> context");
    case AST_TUPLE_LITERAL: {
        size_t count = ast_tuple_literal_count(expr);
        LLVMTypeRef *fields;

        if (count < 2)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "tuple literal requires at least two elements");
        if (count > UINT_MAX)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "tuple literal exceeds LLVM struct field limit");
        if (count > SIZE_MAX / sizeof(*fields))
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "tuple literal field allocation would overflow");
        fields = pgy_arena_calloc(&ctx->scratch, count * sizeof(*fields));
        if (fields == NULL)
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "tuple literal field allocation failed");
        for (size_t i = 0; i < count; i++) {
            fields[i] = llvm_stmt_infer_expr_type(ctx,
                ast_tuple_literal_element(expr, i));
            if (ctx->has_error || fields[i] == NULL)
                return NULL;
        }
        return LLVMStructTypeInContext(ctx->context, fields,
            (unsigned)count, 0);
    }
    case AST_LAMBDA_EXPR: {
        LLVMTypeRef lambda_type = llvm_stmt_lambda_signature_type(ctx, expr);
        if (ctx->has_error || lambda_type == NULL)
            return NULL;
        return lambda_type;
    }
    case AST_ARRAY_ACCESS: {
        ASTNode *array = ast_array_access_array(expr);
        LLVMTypeRef elem_type =
            llvm_stmt_resolve_array_elem_type(ctx, array, NULL);
        if (elem_type != NULL)
            return elem_type;
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "indexed collection access requires registered Array<T>/Slice<T> metadata");
    }
    case AST_IDENTIFIER: {
        const char *name = ast_identifier_name(expr);
        LLVMVarEntry var;
        bool has_var = llvm_scope_lookup_snapshot(ctx, name, &var);
        if (pgy_codegen_match_variant_lookup(name)
                == PGY_MATCH_VARIANT_NONE_CTOR) {
            if (ctx->expected_type_name != NULL
                && pgy_classify_type(ctx->expected_type_name)
                    == PGY_TK_OPTION) {
                LLVMTypeRef expected = pergyra_type_to_llvm(
                    ctx, ctx->expected_type_name);
                if (expected != NULL)
                    return expected;
            }
            if (ctx->current_ret_type != NULL
                && LLVMGetTypeKind(ctx->current_ret_type)
                    == LLVMStructTypeKind
                && LLVMCountStructElementTypes(ctx->current_ret_type) == 2
                && LLVMStructGetTypeAtIndex(ctx->current_ret_type, 0)
                    == ctx->type_i32) {
                return ctx->current_ret_type;
            }
        }
        if (has_var)
            return var.type;
        if (name != NULL && strcmp(name, "self") != 0
            && llvm_current_host_class_name(ctx) != NULL) {
            LLVMClassTypeEntry *host_cls = llvm_lookup_class(
                ctx, llvm_current_host_class_name(ctx));
            int field_idx = host_cls != NULL
                ? llvm_class_field_index(host_cls, name)
                : -1;
        if (field_idx >= 0)
                return llvm_class_field_type_at_index(host_cls, field_idx);
        }
        {
            const char *custom_type = llvm_expr_custom_type_name(expr, ctx);
            if (custom_type != NULL) {
                LLVMTypeRef ty = pergyra_type_to_llvm(ctx, custom_type);
                if (ty != NULL)
                    return ty;
            }
        }
        {
            LLVMTypeRef source_local_ty =
                llvm_stmt_source_local_type(ctx, name);
            if (source_local_ty != NULL || ctx->has_error)
                return source_local_ty;
        }
        /* Unannotated local (`let x = <call>`) with no scope alloca yet in the
         * pre-pass: recover its type by inferring the let initializer. */
        {
            static int s_local_init_depth;
            ASTNode *local_init = llvm_stmt_source_local_let_init(ctx, name);
            if (local_init != NULL && s_local_init_depth < 8) {
                bool prev_err = ctx->has_error;
                LLVMTypeRef ty;
                s_local_init_depth++;
                ty = llvm_stmt_infer_expr_type(ctx, local_init);
                s_local_init_depth--;
                if (ctx->has_error && !prev_err)
                    ctx->has_error = false;
                else if (ty != NULL)
                    return ty;
            }
        }
        {
            char reason[160];
            if (!llvm_stmt_type_reasonf(reason, sizeof(reason),
                    "identifier '%s' requires registered LLVM local metadata",
                    name != NULL ? name : "<null>"))
                return llvm_stmt_unknown_expr_type(ctx, expr,
                    "identifier requires registered LLVM local metadata");
            return llvm_stmt_unknown_expr_type(ctx, expr, reason);
        }
    }
    case AST_ASSIGNMENT:
        if (ast_assignment_target(expr) != NULL
            && ast_assignment_target(expr)->type == AST_IDENTIFIER) {
            LLVMVarEntry var;
            const char *target_name =
                ast_identifier_name(ast_assignment_target(expr));
            if (llvm_scope_lookup_snapshot(ctx, target_name, &var))
                return var.type;
        }
        if (ast_assignment_value(expr) != NULL)
            return llvm_stmt_infer_expr_type(ctx, ast_assignment_value(expr));
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "assignment is missing a value expression");
    case AST_UNARY: {
        PgyTokenType op = ast_unary_operator(expr).type;
        if (op == TOKEN_NOT)
            return ctx->type_i1;
        if (op == TOKEN_MINUS)
            return llvm_stmt_infer_expr_type(ctx, ast_unary_operand(expr));
        if (op == TOKEN_QUESTION) {
            LLVMTypeRef operand_type =
                llvm_stmt_infer_expr_type(ctx, ast_unary_operand(expr));
            if (operand_type != NULL
                && LLVMGetTypeKind(operand_type) == LLVMStructTypeKind
                && LLVMCountStructElementTypes(operand_type) >= 2) {
                LLVMTypeRef fields[8];
                LLVMGetStructElementTypes(operand_type, fields);
                return fields[1];
            }
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "try operator requires Result-like aggregate metadata");
        }
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "unsupported unary operator has no inferred LLVM type");
    }
    case AST_CHANNEL_RECV:
        if (ast_channel_recv_channel(expr) != NULL
            && ast_channel_recv_channel(expr)->type == AST_IDENTIFIER) {
            ASTNode *channel = ast_channel_recv_channel(expr);
            const char *name =
                ast_identifier_name(channel);
            const char *inner = llvm_resolve_channel_target_inner(ctx, expr,
                channel, "channel receive expression");
            if (inner != NULL)
                return pergyra_type_to_llvm(ctx, inner);
            if (name != NULL && llvm_scope_contains(ctx, name)) {
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
        /* Missing channel metadata is a source-of-truth gap. */
        if (ctx->expected_type_name != NULL
            && pgy_classify_type(ctx->expected_type_name) != PGY_TK_CHANNEL) {
            LLVMTypeRef expected = pergyra_type_to_llvm(
                ctx, ctx->expected_type_name);
            if (expected != NULL)
                return expected;
        }
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "channel receive requires registered Channel<T> metadata");
    case AST_MEMBER_ACCESS: {
        ASTNode *obj_node = ast_member_object(expr);
        const char *field_name = ast_member_name(expr);
        /* Enum-qualified variants use the enum carrier type. */
        if (obj_node != NULL && field_name != NULL
            && llvm_is_upper_ident(obj_node)
            && llvm_lookup_enum_variant_qualified(ctx,
                ast_identifier_name(obj_node), field_name) != NULL) {
            return ctx->type_i32;
        }
        const char *base_name = llvm_stmt_infer_nominal_name_from_init(
            ctx, obj_node);
        LLVMClassTypeEntry *base_cls = base_name != NULL
            ? llvm_lookup_class(ctx, base_name) : NULL;
        /* P0 #4 fallback split into llvm_stmt_source_local_fallback.c. */
        if (base_cls == NULL) {
            base_cls = llvm_stmt_source_local_class(ctx, obj_node);
            if (base_cls != NULL)
                base_name = base_cls->class_name;
        }
        if (base_cls != NULL) {
            int field_idx = llvm_class_field_index(base_cls, ast_member_name(expr));
            if (field_idx >= 0)
                return llvm_class_field_type_at_index(base_cls, field_idx);
        }
        {
            char reason[256];
            ASTNode *member_object = ast_member_object(expr);
            const char *field_name = ast_member_name(expr) != NULL
                ? ast_member_name(expr) : "<field>";
            const char *base_expr_name =
                (member_object != NULL
                 && member_object->type == AST_IDENTIFIER
                 && ast_identifier_name(member_object) != NULL)
                    ? ast_identifier_name(member_object)
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
        return llvm_stmt_infer_call_expr_type(ctx, expr);
    case AST_BINARY: {
        PgyTokenType op = ast_binary_operator(expr).type;
        LLVMTypeRef left_ty = NULL;
        LLVMTypeRef right_ty = NULL;
        if (op == TOKEN_COALESCE) {
            LLVMTypeRef fields[2];
            left_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_left(expr));
            if (ctx->has_error || left_ty == NULL)
                return NULL;
            if (LLVMGetTypeKind(left_ty) == LLVMStructTypeKind
                && LLVMCountStructElementTypes(left_ty) == 2) {
                LLVMGetStructElementTypes(left_ty, fields);
                return fields[1];
            }
            return llvm_stmt_unknown_expr_type(ctx, expr,
                "coalesce operator requires concrete Option<T> left operand");
        }
        if (op == TOKEN_EQUAL || op == TOKEN_NOT_EQUAL
            || op == TOKEN_LESS || op == TOKEN_LESS_EQUAL
            || op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL
            || op == TOKEN_AND || op == TOKEN_OR) {
            return ctx->type_i1;
        }
        if (op == TOKEN_PLUS) {
            left_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_left(expr));
            right_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_right(expr));
            if (left_ty == ctx->type_i8ptr || right_ty == ctx->type_i8ptr)
                return ctx->type_i8ptr;
            return llvm_stmt_promote_numeric_type(ctx, left_ty, right_ty);
        }
        if (op == TOKEN_MINUS || op == TOKEN_STAR || op == TOKEN_SLASH
            || op == TOKEN_PERCENT) {
            left_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_left(expr));
            right_ty = llvm_stmt_infer_expr_type(ctx, ast_binary_right(expr));
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
        return llvm_stmt_unknown_expr_type(ctx, expr,
            "expression requires typed MIR result facts");
    }
}

#endif /* PGY_LLVM_ENABLED */
