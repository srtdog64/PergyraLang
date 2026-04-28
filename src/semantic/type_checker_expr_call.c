#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"

static Type *
expr_call_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_type_ref_or_materialize(ctx, type_ref);
}

static const char *
expr_root_identifier_name(ASTNode *expr)
{
    if (expr == NULL)
        return NULL;

    switch (expr->type) {
    case AST_IDENTIFIER:
        return expr->data.identifier.name;
    case AST_MEMBER_ACCESS:
        return expr_root_identifier_name(expr->data.member.object);
    case AST_ARRAY_ACCESS:
        return expr_root_identifier_name(expr->data.array_access.array);
    default:
        return NULL;
    }
}

static size_t
expr_embedded_world_zone_index(SemanticContext *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return (size_t)-1;

    for (size_t i = 0; i < ctx->embedded_world_zone_count; i++) {
        if (ctx->embedded_world_zone_names[i] != NULL
            && strcmp(ctx->embedded_world_zone_names[i], name) == 0) {
            return i;
        }
    }
    return (size_t)-1;
}

static bool
expr_has_embedded_world_zone_name(SemanticContext *ctx, const char *name)
{
    return expr_embedded_world_zone_index(ctx, name) != (size_t)-1;
}

static const char *
expr_embedded_world_zone_world_name(SemanticContext *ctx, const char *name)
{
    size_t index = expr_embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_world_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_world_names[index];
}

static const char *
expr_embedded_world_zone_slot_name(SemanticContext *ctx, const char *name)
{
    size_t index = expr_embedded_world_zone_index(ctx, name);
    if (index == (size_t)-1 || ctx->embedded_world_zone_slot_names == NULL)
        return NULL;
    return ctx->embedded_world_zone_slot_names[index];
}

static void
expr_reject_embedded_world_zone_mutation(SemanticContext *ctx, ASTNode *site,
                                         ASTNode *target, const char *op_name)
{
    const char *root_name;
    const char *owner_world;
    const char *owner_slot;
    Symbol *root_sym;

    if (ctx == NULL || site == NULL || target == NULL || op_name == NULL)
        return;
    if (ctx->current_world != NULL)
        return;

    root_name = expr_root_identifier_name(target);
    if (root_name == NULL)
        return;

    root_sym = scope_lookup(ctx->scope, root_name);
    if ((root_sym == NULL || !root_sym->embedded_in_world)
        && !expr_has_embedded_world_zone_name(ctx, root_name)) {
        return;
    }

    owner_world = expr_embedded_world_zone_world_name(ctx, root_name);
    owner_slot = expr_embedded_world_zone_slot_name(ctx, root_name);

    semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID,
        PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
        "Zone '%s' cannot be mutated via %s after it was embedded into world '%s' slot '%s'.\n"
        "Reason:\n"
        "- origin binding is '%s'\n"
        "Contract source:\n"
        "- world '%s' zone slot '%s'\n"
        "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
        "- derived embedding provenance points to world '%s' slot '%s'\n"
        "- ownership/authority now flows through the world-owned slot rather than the old local binding\n"
        "- owned embedding hands authority-bearing visibility to the world-owned slot\n"
        "- mutating the old binding would diverge from the world-owned handoff destination\n"
        "Fix:\n"
        "- finish configuring '%s' before constructing world '%s'\n"
        "- or mutate it through world '%s' slot '%s' after embedding",
        root_name, op_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>",
        root_name,
        owner_world != NULL ? owner_world : "<world>",
        owner_world != NULL ? owner_world : "<world>",
        owner_slot != NULL ? owner_slot : "<slot>");
}

Type *
type_check_call(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee = expr->data.call.callee;

    if (callee->type == AST_IDENTIFIER) {
        const char *name = callee->data.identifier.name;
        BuiltinKind bk   = builtin_resolve(name);
        if (bk != BUILTIN_NOT_BUILTIN)
            return type_check_builtin_call(expr, bk, ctx);

        {
            ASTNode *host_method = expr_current_host_method_decl(ctx, name);
            if (host_method != NULL)
                return expr_type_check_host_method_call(expr, host_method, ctx);
        }

        if (strcmp(name, "Channel") == 0)
            return TYPE_UNKNOWN;

        if (strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0)
            return TYPE_UNKNOWN;

        {
            Type *stdlib_type = type_check_stdlib_call(expr, name, ctx);
            if (stdlib_type != NULL)
                return stdlib_type;
        }

        Symbol *sym = scope_lookup(ctx->scope, name);
        return type_check_function_symbol_call(expr, sym, name, ctx);
    }

    if (callee->type == AST_MEMBER_ACCESS) {
        ASTNode *object = callee->data.member.object;
        const char *method_name = callee->data.member.name;

        if (object != NULL && method_name != NULL
            && (strcmp(method_name, "Write") == 0
                || strcmp(method_name, "Read") == 0
                || strcmp(method_name, "Release") == 0)) {
            Type *slot_type = type_check_expression(object, ctx);
            if (slot_type != NULL && slot_type->kind == TYPE_KIND_SLOT) {
                size_t orig_argc = expr->data.call.arg_count;
                bool inject_token = false;
                char token_name_buf[256];
                const char *token_name = NULL;
                ASTNode token_arg;
                ASTNode *synthetic_args[4];
                ASTNode fake_call;
                size_t new_argc = 1 + orig_argc;

                memset(&token_arg, 0, sizeof(token_arg));
                memset(&fake_call, 0, sizeof(fake_call));

                if (slot_type->data.slot.is_secure
                    && object->type == AST_IDENTIFIER
                    && object->data.identifier.name != NULL) {
                    Symbol *slot_sym =
                        scope_lookup(ctx->scope, object->data.identifier.name);
                    if (slot_sym != NULL
                        && slot_sym->kind == SYMBOL_SLOT
                        && slot_sym->slot_info.paired_token_name != NULL) {
                        token_name = slot_sym->slot_info.paired_token_name;
                    } else {
                        snprintf(token_name_buf, sizeof(token_name_buf), "%s_token",
                            object->data.identifier.name);
                        token_name = token_name_buf;
                    }
                    if ((strcmp(method_name, "Write") == 0 && orig_argc < 2)
                        || ((strcmp(method_name, "Read") == 0
                             || strcmp(method_name, "Release") == 0)
                            && orig_argc < 1)) {
                        inject_token = true;
                        new_argc++;
                        token_arg.type = AST_IDENTIFIER;
                        token_arg.data.identifier.name = (char *)token_name;
                    }
                }

                if (new_argc <= sizeof(synthetic_args) / sizeof(synthetic_args[0])) {
                    synthetic_args[0] = object;
                    for (size_t i = 0; i < orig_argc; i++)
                        synthetic_args[i + 1] = expr->data.call.arguments[i];
                    if (inject_token)
                        synthetic_args[new_argc - 1] = &token_arg;

                    fake_call.type = AST_CALL;
                    fake_call.line = expr->line;
                    fake_call.column = expr->column;
                    fake_call.data.call.callee = callee;
                    fake_call.data.call.arguments = synthetic_args;
                    fake_call.data.call.arg_count = new_argc;

                    if (strcmp(method_name, "Write") == 0) {
                        (void)type_check_write_slot(&fake_call, ctx);
                        return TYPE_VOID;
                    }
                    if (strcmp(method_name, "Read") == 0)
                        return type_check_read_slot(&fake_call, ctx);

                    (void)type_check_release_slot(&fake_call, ctx);
                    return TYPE_VOID;
                }
            }
        }

        if (expr_member_is_static_access(callee)) {
            char *flat_name = flatten_static_member_access(callee, '_');
            char *display_name = flatten_static_member_access(callee, '.');
            Symbol *sym = flat_name != NULL
                ? scope_lookup(ctx->scope, flat_name)
                : NULL;
            if (sym == NULL && callee->data.member.name != NULL)
                sym = scope_lookup(ctx->scope, callee->data.member.name);
            Type *result = type_check_function_symbol_call(
                expr, sym, display_name != NULL ? display_name : "<member>", ctx);
            free(flat_name);
            free(display_name);
            return result;
        }

        if (!(object != NULL
              && object->type == AST_IDENTIFIER
              && object->data.identifier.name != NULL
              && object->data.identifier.name[0] >= 'A'
              && object->data.identifier.name[0] <= 'Z')) {
            expr_reject_embedded_world_zone_mutation(ctx, expr, object,
                                                     "hosted func/action call");
            Type *object_type = type_check_expression(object, ctx);
            if (object_type != NULL
                && method_name != NULL
                && strcmp(method_name, "Slice") == 0
                && (type_is_constructed_named(object_type, "Array")
                    || type_is_constructed_named(object_type, "Slice"))) {
                if (expr->data.call.arg_count != 2) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
                        "%s.%s(start, len) requires exactly two Int arguments",
                        type_is_constructed_named(object_type, "Array")
                            ? "Array<T>" : "Slice<T>",
                        method_name);
                    return TYPE_UNKNOWN;
                }

                require_assignable(
                    type_check_expression(expr->data.call.arguments[0], ctx),
                    TYPE_INT, expr->data.call.arguments[0], ctx);
                require_assignable(
                    type_check_expression(expr->data.call.arguments[1], ctx),
                    TYPE_INT, expr->data.call.arguments[1], ctx);

                if (object_type->kind == TYPE_KIND_CONSTRUCTED
                    && object_type->data.constructed.arg_count >= 1
                    && object_type->data.constructed.args[0] != NULL) {
                    Type *slice_args[1] = {
                        object_type->data.constructed.args[0]
                    };
                    return type_create_constructed(TYPE_SLICE, slice_args, 1);
                }
                return TYPE_UNKNOWN;
            }
            if (object_type != NULL
                && object_type->kind == TYPE_KIND_SLOT
                && method_name != NULL) {
                Symbol *sym = NULL;
                Symbol *owner = NULL;
                if (object->type == AST_IDENTIFIER)
                    sym = scope_lookup(ctx->scope, object->data.identifier.name);

                if (strcmp(method_name, "Write") == 0) {
                    if (expr->data.call.arg_count < 1) {
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
                    if (object_type->data.slot.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    if (sym != NULL && sym->kind == SYMBOL_SLOT) {
                        if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_LIFECYCLE_WRITE_AFTER_RELEASE,
                                PGY_FIX_RECLAIM_BEFORE_USE,
                                object,
                                "Cannot write to released slot '%s'",
                                sym->name);
                            return TYPE_UNKNOWN;
                        }
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
                                sym->name, owner->name);
                            return TYPE_UNKNOWN;
                        }
                        if (owner != NULL && owner->slot_info.is_secure)
                            semantic_record_effect(ctx, EFFECT_SECURE);
                    }
                    require_assignable(
                        type_check_expression(expr->data.call.arguments[0], ctx),
                        object_type->data.slot.inner_type,
                        expr->data.call.arguments[0], ctx);
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
                    if (object_type->data.slot.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    if (sym != NULL && sym->kind == SYMBOL_SLOT) {
                        if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_LIFECYCLE_READ_AFTER_RELEASE,
                                PGY_FIX_RECLAIM_BEFORE_USE,
                                object,
                                "Cannot read from released slot '%s'",
                                sym->name);
                            return TYPE_UNKNOWN;
                        }
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
                                sym->name, owner->name);
                            return TYPE_UNKNOWN;
                        }
                        if (owner != NULL && owner->slot_info.is_secure)
                            semantic_record_effect(ctx, EFFECT_SECURE);
                    }
                    return object_type->data.slot.inner_type;
                }

                if (strcmp(method_name, "Release") == 0) {
                    if (object->type != AST_IDENTIFIER || sym == NULL || sym->kind != SYMBOL_SLOT) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_RELEASE_REQUIRES_OWNER, PGY_CAUSE_RELEASE_NON_OWNING_RECEIVER, PGY_FIX_RELEASE_OWNING_SLOT_NOT_VIEW, object,
                            "slot.Release() requires an owning slot identifier");
                        return TYPE_UNKNOWN;
                    }
                    if (sym->slot_info.state == SLOT_STATE_RELEASED) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_DOUBLE_RELEASE, PGY_CAUSE_RELEASE_DOUBLE, PGY_FIX_REMOVE_REDUNDANT_RELEASE, object,
                            "Slot '%s' has already been released", sym->name);
                        return TYPE_UNKNOWN;
                    }
                    if (sym->slot_info.is_secure)
                        semantic_record_effect(ctx, EFFECT_SECURE);
                    scope_release_slot(ctx->scope, sym->name);
                    return TYPE_VOID;
                }
            }
            ASTNode *class_decl;

            if (expr_type_is_nominal_host_type(object_type, ctx)
                && object_type->name != NULL
                && method_name != NULL) {
                class_decl = find_type_decl_by_name(ctx->program_root,
                    object_type->name);
                if (class_decl != NULL) {
                    ASTNode **methods = NULL;
                    size_t method_count = 0;
                    if (class_decl->type == AST_CLASS_DECL) {
                        methods = class_decl->data.class_decl.methods;
                        method_count = class_decl->data.class_decl.method_count;
                    } else if (class_decl->type == AST_ENUM_DECL) {
                        methods = class_decl->data.enum_decl.methods;
                        method_count = class_decl->data.enum_decl.method_count;
                    }
                    for (size_t i = 0; i < method_count; i++) {
                        ASTNode *method = methods[i];
                        if (method == NULL || method->type != AST_FUNC_DECL
                            || method->data.func_decl.name == NULL)
                            continue;
                        if (strcmp(method->data.func_decl.name, method_name) == 0) {
                            uint32_t method_effects =
                                declared_effects_from_function_node(method, ctx, NULL);
                            if (!explicit_member_access_allowed(class_decl,
                                    object_type,
                                    method->data.func_decl.access,
                                    method->data.func_decl.has_explicit_access,
                                    ctx)) {
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY, PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER, expr,
                                    "Member '%s.%s' is not accessible across the current visibility boundary",
                                    object_type->name,
                                    method_name);
                                return TYPE_UNKNOWN;
                            }
                            semantic_record_effect(ctx, method_effects);
                            semantic_record_callable_decl_summary(
                                ctx, method, method_effects);
                            if (ctx->in_parallel
                                && type_effect_mask_has(method_effects, EFFECT_SECURE)) {
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
                                    "Parallel context does not permit calling secure-effect method '%s.%s'; serialize authority-bearing operations outside the parallel block",
                                    object_type->name,
                                    method_name);
                                return TYPE_UNKNOWN;
                            }
                            if (method->data.func_decl.return_type != NULL)
                                return expr_call_resolve_type_ref(
                                    method->data.func_decl.return_type, ctx);
                            return TYPE_VOID;
                        }
                    }
                }
            }
        }
        return TYPE_UNKNOWN;
    }

    return TYPE_UNKNOWN;
}
