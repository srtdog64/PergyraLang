#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "../common/match_variant_policy.h"

static Type *
expr_call_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static void
expr_call_report_unknown_member(SemanticContext *ctx, ASTNode *site,
                                const Type *object_type,
                                const char *method_name)
{
    const char *type_name = type_name_or_unknown(object_type);
    const char *member_name = method_name != NULL ? method_name : "<method>";

    semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL,
        PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, site,
        "Unknown method '%s.%s'.\n"
        "Reason:\n"
        "- the receiver type is known, but no callable member with this name is declared\n"
        "- field access and method calls are separate beta-stable surfaces\n"
        "Fix:\n"
        "- declare method '%s' on '%s'\n"
        "- or remove the call parentheses if this was meant to read a field",
        type_name,
        member_name,
        member_name,
        type_name);
}

static void
expr_call_report_unsupported_callee(SemanticContext *ctx, ASTNode *site,
                                    const char *callee_shape)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_CALL_NOT_CALLABLE, PGY_FIX_USE_CALLABLE_DECLARATION, site,
        "Unsupported call target '%s'.\n"
        "Reason:\n"
        "- beta-stable calls require a named function, callable binding, static member, or declared receiver method\n"
        "- anonymous/computed callees do not yet carry stable provenance into semantic, AIR, MIR, and diagnostics\n"
        "Fix:\n"
        "- bind the callable to a named function or variable before calling it\n"
        "- or use an explicit receiver method call with a declared method",
        callee_shape != NULL ? callee_shape : "<callee>");
}

Type *
type_check_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee = ast_call_callee(expr);
    size_t arg_count = ast_call_arg_count(expr);

    if (callee == NULL) {
        expr_call_report_unsupported_callee(ctx, expr, "<missing>");
        return TYPE_UNKNOWN;
    }

    bool callee_is_nominal_ctor = false;
    if (callee->type == AST_IDENTIFIER) {
        const char *ctor_name = ast_identifier_name(callee);
        callee_is_nominal_ctor = ctor_name != NULL
            && (semantic_find_class_decl_by_name(ctx, ctor_name) != NULL
                || semantic_find_party_decl_by_name(ctx, ctor_name) != NULL);
    }

    for (size_t i = 0; i < arg_count; i++) {
        const char *arg_name = ast_call_argument_name(expr, i);
        if (arg_name != NULL && !callee_is_nominal_ctor) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                expr,
                "Named call argument '%s:' is reserved but not implemented yet.\n"
                "Reason:\n"
                "- named/default argument dispatch needs a frozen call ABI, overload policy, and default-value interaction\n"
                "- beta-stable calls currently use positional arguments only\n"
                "Fix:\n"
                "- pass this argument positionally for now\n"
                "- keep named arguments behind the reserved syntax until call dispatch is implemented end-to-end",
                arg_name);
            return TYPE_UNKNOWN;
        }
    }

    if (callee->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(callee);
        bool user_class_overrides_builtin =
            semantic_find_class_decl_by_name(ctx, name) != NULL;
        BuiltinKind bk   = user_class_overrides_builtin
            ? BUILTIN_NOT_BUILTIN
            : builtin_resolve(name);
        if (bk != BUILTIN_NOT_BUILTIN)
            return type_check_builtin_call(expr, bk, ctx);

        {
            ASTNode *host_method = expr_current_host_method_decl(ctx, name);
            if (host_method != NULL)
                return expr_type_check_host_method_call(expr, host_method, ctx);
        }

        if (strcmp(name, "Channel") == 0)
            return TYPE_UNKNOWN;

        if (pgy_match_variant_is_result(pgy_match_variant_lookup(name)))
            return TYPE_UNKNOWN;

        if (!user_class_overrides_builtin) {
            Type *stdlib_type = type_check_stdlib_call(expr, name, ctx);
            if (stdlib_type != NULL)
                return stdlib_type;
        }

        Symbol *sym = scope_lookup(ctx->scope, name);
        return type_check_function_symbol_call(expr, sym, name, ctx);
    }

    if (callee->type == AST_MEMBER_ACCESS) {
        ASTNode *object = ast_member_object(callee);
        const char *method_name = ast_member_name(callee);

        if (object != NULL && method_name != NULL
            && (strcmp(method_name, "Write") == 0
                || strcmp(method_name, "Read") == 0
                || strcmp(method_name, "Release") == 0)) {
            Type *slot_type = expr_call_normalize_type(
                type_check_expression(object, ctx));
            if (slot_type->kind == TYPE_KIND_SLOT) {
                size_t orig_argc = arg_count;
                bool inject_token = false;
                char token_name_buf[256];
                const char *token_name = NULL;
                ASTNode *token_arg_node = NULL;
                ASTNode *synthetic_args[4];
                ASTNode fake_call;
                size_t new_argc = 1 + orig_argc;

                memset(&fake_call, 0, sizeof(fake_call));

                if (type_slot_is_secure(slot_type)
                    && object->type == AST_IDENTIFIER
                    && ast_identifier_name(object) != NULL) {
                    const char *object_name = ast_identifier_name(object);
                    Symbol *slot_sym =
                        scope_lookup(ctx->scope, object_name);
                    if (slot_sym != NULL
                        && slot_sym->kind == SYMBOL_SLOT
                        && slot_sym->slot_info.paired_token_name != NULL) {
                        token_name = slot_sym->slot_info.paired_token_name;
                    } else {
                        if (!semantic_format_secure_token_name(
                                token_name_buf, sizeof(token_name_buf),
                                object_name, expr, ctx))
                            return TYPE_UNKNOWN;
                        token_name = token_name_buf;
                    }
                    if ((strcmp(method_name, "Write") == 0 && orig_argc < 2)
                        || ((strcmp(method_name, "Read") == 0
                             || strcmp(method_name, "Release") == 0)
                            && orig_argc < 1)) {
                        inject_token = true;
                        new_argc++;
                        token_arg_node = ast_create_identifier(token_name);
                        if (token_arg_node == NULL)
                            return TYPE_UNKNOWN;
                    }
                }

                if (new_argc <= sizeof(synthetic_args) / sizeof(synthetic_args[0])) {
                    synthetic_args[0] = object;
                    for (size_t i = 0; i < orig_argc; i++)
                        synthetic_args[i + 1] = ast_call_argument(expr, i);
                    if (inject_token)
                        synthetic_args[new_argc - 1] = token_arg_node;

                    ast_init_call_borrowed_view(&fake_call, callee,
                        synthetic_args, new_argc);
                    fake_call.line = expr->line;
                    fake_call.column = expr->column;

                    if (strcmp(method_name, "Write") == 0) {
                        (void)type_check_write_slot(&fake_call, ctx);
                        ast_destroy(token_arg_node);
                        return TYPE_VOID;
                    }
                    if (strcmp(method_name, "Read") == 0) {
                        Type *read_type = type_check_read_slot(&fake_call, ctx);
                        ast_destroy(token_arg_node);
                        return read_type;
                    }

                    (void)type_check_release_slot(&fake_call, ctx);
                    ast_destroy(token_arg_node);
                    return TYPE_VOID;
                }
                ast_destroy(token_arg_node);
            }
        }

        if (expr_member_is_static_access(callee)) {
            char *flat_name = flatten_static_member_access(callee, '_');
            char *display_name = flatten_static_member_access(callee, '.');
            Symbol *sym = flat_name != NULL
                ? scope_lookup(ctx->scope, flat_name)
                : NULL;
            if (sym == NULL && method_name != NULL)
                sym = scope_lookup(ctx->scope, method_name);
            Type *result = type_check_function_symbol_call(
                expr, sym, display_name != NULL ? display_name : "<member>", ctx);
            free(flat_name);
            free(display_name);
            return result;
        }

        if (!(object != NULL
              && object->type == AST_IDENTIFIER
              && ast_identifier_name(object) != NULL
              && ast_identifier_name(object)[0] >= 'A'
              && ast_identifier_name(object)[0] <= 'Z')) {
            reject_if_embedded_world_zone_mutation(ctx, expr, object,
                                                   "hosted func/action call");
            Type *object_type = expr_call_normalize_type(
                type_check_expression(object, ctx));
            if (method_name != NULL
                && strcmp(method_name, "Slice") == 0
                && (type_is_constructed_named(object_type, "Array")
                    || type_is_constructed_named(object_type, "Slice"))) {
                ASTNode *start_arg = ast_call_argument(expr, 0);
                ASTNode *len_arg = ast_call_argument(expr, 1);
                if (arg_count != 2) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                        "%s.%s(start, len) requires exactly two Int arguments",
                        type_is_constructed_named(object_type, "Array")
                            ? "Array<T>" : "Slice<T>",
                        method_name);
                    return TYPE_UNKNOWN;
                }

                require_assignable(
                    expr_call_normalize_type(
                        type_check_expression(start_arg, ctx)),
                    TYPE_INT, start_arg, ctx);
                require_assignable(
                    expr_call_normalize_type(
                        type_check_expression(len_arg, ctx)),
                    TYPE_INT, len_arg, ctx);

                if (type_constructed_arg_count(object_type) >= 1
                    && type_constructed_arg(object_type, 0) != NULL) {
                    Type *slice_args[1] = {
                        type_constructed_arg(object_type, 0)
                    };
                    return type_create_constructed(TYPE_SLICE, slice_args, 1);
                }
                return TYPE_UNKNOWN;
            }
            if (object_type->kind == TYPE_KIND_SLOT
                && method_name != NULL) {
                Symbol *sym = NULL;
                Symbol *owner = NULL;
                if (object->type == AST_IDENTIFIER)
                    sym = scope_lookup(ctx->scope, ast_identifier_name(object));

                if (strcmp(method_name, "Write") == 0) {
                    ASTNode *value_arg = ast_call_argument(expr, 0);
                    if (arg_count < 1) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                            "slot.Write(value) requires a value argument");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_read_view(object_type)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_VIEW_KIND_MISMATCH, PGY_CAUSE_VIEW_KIND_OP_MISMATCH, PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT, object,
                            "Cannot write through ReadView<T>; create a WriteView(slot) or keep the owning Slot<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_move_token(object_type)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, object,
                            "Cannot write through MoveToken<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (type_slot_is_secure(object_type))
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    if (sym != NULL && sym->kind == SYMBOL_SLOT) {
                        if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_LIFECYCLE_WRITE_AFTER_RELEASE,
                                PGY_FIX_RECLAIM_BEFORE_USE,
                                object,
                                "Cannot write to released slot '%s'",
                                sym->name != NULL ? sym->name : "<slot>");
                            return TYPE_UNKNOWN;
                        }
                        sym->slot_flow_access_mask |= PGY_SLOT_FLOW_ACCESS_WRITE;
                    } else if (sym != NULL && type_is_write_view(sym->type)
                               && sym->slot_info.paired_slot_name != NULL) {
                        owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
                        if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_VIEW_WRITE_THROUGH_RELEASED_OWNER,
                                PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW,
                                object,
                                "Cannot write through WriteView '%s' because source slot '%s' was released",
                                sym->name != NULL ? sym->name : "<view>",
                                owner->name != NULL ? owner->name : "<slot>");
                            return TYPE_UNKNOWN;
                        }
                        if (owner != NULL && owner->slot_info.is_secure)
                            semantic_record_effect(ctx, EFFECT_SECURE);
                        if (owner != NULL)
                            owner->slot_flow_access_mask |= PGY_SLOT_FLOW_ACCESS_WRITE;
                    }
                    require_assignable(
                        expr_call_normalize_type(
                            type_check_expression(value_arg, ctx)),
                        type_slot_inner_type(object_type),
                        value_arg, ctx);
                    return TYPE_VOID;
                }

                if (strcmp(method_name, "Read") == 0) {
                    if (type_is_write_view(object_type)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_VIEW_KIND_MISMATCH, PGY_CAUSE_VIEW_KIND_OP_MISMATCH, PGY_FIX_ACQUIRE_MATCHING_VIEW_OR_USE_SLOT, object,
                            "Cannot read through WriteView<T>; create a ReadView(slot) or keep the owning Slot<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (type_is_move_token(object_type)) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_TOKEN_MISUSE, PGY_CAUSE_MOVE_TOKEN_DIRECT_ACCESS, PGY_FIX_MATERIALIZE_TOKEN_TO_SLOT, object,
                            "Cannot read through MoveToken<T>");
                        return TYPE_UNKNOWN;
                    }
                    if (type_slot_is_secure(object_type))
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    if (sym != NULL && sym->kind == SYMBOL_SLOT) {
                        if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_LIFECYCLE_READ_AFTER_RELEASE,
                                PGY_FIX_RECLAIM_BEFORE_USE,
                                object,
                                "Cannot read from released slot '%s'",
                                sym->name != NULL ? sym->name : "<slot>");
                            return TYPE_UNKNOWN;
                        }
                        sym->slot_flow_access_mask |= PGY_SLOT_FLOW_ACCESS_READ;
                    } else if (sym != NULL && type_is_read_view(sym->type)
                               && sym->slot_info.paired_slot_name != NULL) {
                        owner = scope_lookup(ctx->scope, sym->slot_info.paired_slot_name);
                        if (owner != NULL && owner->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_VIEW_READ_THROUGH_RELEASED_OWNER,
                                PGY_FIX_RECLAIM_SOURCE_OR_DROP_VIEW,
                                object,
                                "Cannot read through ReadView '%s' because source slot '%s' was released",
                                sym->name != NULL ? sym->name : "<view>",
                                owner->name != NULL ? owner->name : "<slot>");
                            return TYPE_UNKNOWN;
                        }
                        if (owner != NULL && owner->slot_info.is_secure)
                            semantic_record_effect(ctx, EFFECT_SECURE);
                        if (owner != NULL)
                            owner->slot_flow_access_mask |= PGY_SLOT_FLOW_ACCESS_READ;
                    }
                    return expr_call_normalize_type(type_slot_inner_type(object_type));
                }

                if (strcmp(method_name, "Release") == 0) {
                    if (object->type != AST_IDENTIFIER || sym == NULL || sym->kind != SYMBOL_SLOT) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_RELEASE_REQUIRES_OWNER, PGY_CAUSE_RELEASE_NON_OWNING_RECEIVER, PGY_FIX_RELEASE_OWNING_SLOT_NOT_VIEW, object,
                            "slot.Release() requires an owning slot identifier");
                        return TYPE_UNKNOWN;
                    }
                    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_DOUBLE_RELEASE, PGY_CAUSE_RELEASE_DOUBLE, PGY_FIX_REMOVE_REDUNDANT_RELEASE, object,
                            "Slot '%s' has already been released",
                            sym->name != NULL ? sym->name : "<slot>");
                        return TYPE_UNKNOWN;
                    }
                    if (sym->slot_info.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    sym->slot_flow_access_mask |= PGY_SLOT_FLOW_ACCESS_RELEASE;
                    if (sym->name != NULL)
                        scope_release_slot(ctx->scope, sym->name);
                    return TYPE_VOID;
                }

                expr_call_report_unknown_member(ctx, expr, object_type,
                    method_name);
                return TYPE_UNKNOWN;
            }
            if (object_type->name != NULL && method_name != NULL) {
                ASTNode *host_decl =
                    semantic_host_decl_for_type(ctx, object_type);
                if (host_decl != NULL) {
                    size_t method_count = 0;
                    ASTNode **methods =
                        semantic_host_decl_methods(host_decl, &method_count);
                    for (size_t i = 0; i < method_count; i++) {
                        ASTNode *method = methods[i];
                        const char *candidate_name = ast_declaration_name(method);
                        if (method == NULL || method->type != AST_FUNC_DECL
                            || candidate_name == NULL)
                            continue;
                        if (strcmp(candidate_name, method_name) == 0) {
                            if (!explicit_member_access_allowed(host_decl,
                                    object_type,
                                    ast_func_access(method),
                                    ast_func_has_explicit_access(method),
                                    ctx)) {
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY, PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER, expr,
                                    "Member '%s.%s' is not accessible across the current visibility boundary",
                                    object_type->name,
                                    method_name);
                                return TYPE_UNKNOWN;
                            }
                            return expr_type_check_host_method_call_on_host(
                                expr, host_decl, method, object_type, ctx);
                        }
                    }
                    expr_call_report_unknown_member(ctx, expr, object_type,
                        method_name);
                    return TYPE_UNKNOWN;
                }
                expr_call_report_unknown_member(ctx, expr, object_type,
                    method_name);
                return TYPE_UNKNOWN;
            }
        }
        expr_call_report_unsupported_callee(ctx, expr, "member access");
        return TYPE_UNKNOWN;
    }

    expr_call_report_unsupported_callee(ctx, expr, "computed expression");
    return TYPE_UNKNOWN;
}
