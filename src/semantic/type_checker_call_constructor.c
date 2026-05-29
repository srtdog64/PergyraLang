/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Constructor-like symbol call validation for subjects, overlays, worlds, and
 * zones.  Keeping this outside the late function-call owner prevents regular
 * callable argument validation from owning domain constructor diagnostics.
 */

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"

#include <string.h>

static Type *
constructor_call_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static bool
constructor_field_is_ordinary_channel(ASTNode *field_type_node,
                                      Type *field_type)
{
    return (field_type_node != NULL
            && field_type_node->type == AST_CHANNEL_TYPE)
        || type_is_constructed_named(field_type, "Channel");
}

static ASTNode *
constructor_decl_field_type_at(ASTNode *decl,
                               size_t index,
                               const char **field_name_out)
{
    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return NULL;
    if (decl->type == AST_CLASS_DECL) {
        ClassField *field = subject_host_field_at(decl, index);
        if (field_name_out != NULL && field != NULL)
            *field_name_out = field->name;
        return field != NULL ? field->type : NULL;
    }
    return overlay_field_decl_at(decl, index, field_name_out);
}

static bool
constructor_decl_has_channel_field(ASTNode *decl,
                                   SemanticContext *ctx,
                                   const char **field_name_out)
{
    size_t field_count;

    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return false;
    field_count = decl->type == AST_CLASS_DECL
        ? projection_source_field_count(decl)
        : overlay_field_count(decl);
    for (size_t i = 0; i < field_count; i++) {
        const char *field_name = NULL;
        ASTNode *field_type_node =
            constructor_decl_field_type_at(decl, i, &field_name);
        Type *field_type;
        if (field_type_node == NULL)
            continue;
        field_type = semantic_host_resolve_type_ref(field_type_node, ctx);
        if (!constructor_field_is_ordinary_channel(
                field_type_node, field_type)) {
            continue;
        }
        if (field_name_out != NULL)
            *field_name_out = field_name;
        return true;
    }
    return false;
}

static void
constructor_reject_channel_field_store(SemanticContext *ctx,
                                       ASTNode *site,
                                       const char *constructor_name,
                                       const char *field_name)
{
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
        PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
        PGY_FIX_PROVIDE_MOVABLE_HANDLE,
        site,
        "Constructor '%s' cannot aggregate-construct or default-initialize Channel<T> field '%s'.\n"
        "Reason:\n"
        "- ordinary Channel<T> currently lowers to runtime storage with synchronization state\n"
        "- copying that storage would duplicate mutex/condition-variable ownership\n"
        "- default-zeroing that storage would bypass channel runtime initialization\n"
        "- C and LLVM backends require a movable channel-handle lowering before this is safe\n"
        "Fix:\n"
        "- keep the channel in a local binding and pass values through send/recv\n"
        "- or wait for the movable Channel<T> handle ABI before storing channels in aggregate fields",
        constructor_name != NULL ? constructor_name : "<constructor>",
        field_name != NULL ? field_name : "<field>");
}

bool
type_check_constructor_symbol_call(ASTNode *expr,
                                   Symbol *sym,
                                   const char *display_name,
                                   SemanticContext *ctx,
                                   Type **type_out)
{
    if (type_out != NULL)
        *type_out = NULL;

    if (sym->kind == SYMBOL_CLASS || sym->kind == SYMBOL_PARTY
        || sym->kind == SYMBOL_RELATION
        || sym->kind == SYMBOL_EFFECT || sym->kind == SYMBOL_ROSTER
        || sym->kind == SYMBOL_WORLD || sym->kind == SYMBOL_ZONE) {
        if (ctx != NULL) {
            ASTNode *decl = semantic_constructor_decl_for_symbol_kind(ctx,
                sym->kind, display_name);
            size_t field_count = 0;
            bool decl_is_generic = false;
            if (decl != NULL
                && !explicit_type_reference_allowed(decl, expr, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr,
                    "Constructor '%s' is not accessible across the current visibility boundary",
                    display_name);
                if (type_out != NULL)
                    *type_out = TYPE_UNKNOWN;
                return true;
            }
            if (decl != NULL && decl->type == AST_CLASS_DECL) {
                GenericParams *class_generics = ast_class_generic_params(decl);
                (void) ast_class_fields(decl, &field_count);
                decl_is_generic =
                    (ast_generic_param_count(class_generics) > 0);
            } else if (decl != NULL
                       && (decl->type == AST_RELATION_DECL
                           || decl->type == AST_EFFECT_DECL
                           || decl->type == AST_PARTY_DECL
                           || decl->type == AST_ROSTER_DECL
                           || decl->type == AST_WORLD_DECL
                           || decl->type == AST_ZONE_DECL)) {
                field_count = overlay_field_count(decl);
            }
            if (decl != NULL) {
                size_t provided = ast_call_arg_count(expr);
                const char *channel_field_name = NULL;
                if (constructor_decl_has_channel_field(
                        decl, ctx, &channel_field_name)) {
                    constructor_reject_channel_field_store(ctx,
                        expr, display_name, channel_field_name);
                }
                /* Skip field-type validation for generic classes — the
                 * generic params (T, U) aren't in scope at the call site.
                 * Type safety is handled by the let-annotation type. */
                if (provided > field_count) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr,
                        "Constructor '%s' accepts at most %llu positional field argument(s), got %llu",
                        display_name, (unsigned long long) field_count, (unsigned long long) provided);
                } else if (decl_is_generic) {
                    for (size_t i = 0; i < provided; i++) {
                        type_check_expression(ast_call_argument(expr, i), ctx);
                    }
                } else {
                    for (size_t i = 0; i < provided; i++) {
                        const char *field_name = NULL;
                        ASTNode *field_type_node = NULL;
                        field_type_node = constructor_decl_field_type_at(
                            decl, i, &field_name);
                        if (field_type_node == NULL)
                            continue;
                        Type *field_type = semantic_host_resolve_type_ref(
                            field_type_node, ctx);
                        ASTNode *arg = ast_call_argument(expr, i);
                        Type *arg_type = constructor_call_normalize_type(
                            type_check_expression(arg, ctx));
                        if (field_type != NULL
                            && !type_is_assignable(arg_type, field_type)) {
                            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, arg,
                                "Constructor '%s' argument %llu initializes field '%s' of type '%s', got '%s'",
                                display_name, (unsigned long long) (i + 1),
                                field_name != NULL ? field_name : "<field>",
                                field_type->name != NULL ? field_type->name : "<type>",
                                arg_type->name != NULL ? arg_type->name : "<type>");
                        }
                        if (arg != NULL) {
                            const char *borrowed_name =
                                semantic_borrowed_boundary_root_name(
                                    arg, ctx);
                            const char *constructor_name =
                                display_name != NULL ? display_name : "<constructor>";
                            const char *constructor_field =
                                field_name != NULL ? field_name : "<field>";

                            if (borrowed_name != NULL) {
                                semantic_validate_borrowed_escape(
                                    arg,
                                    arg,
                                    ctx,
                                    arg_type,
                                    borrowed_name,
                                    OWNERSHIP_CONSUMER_CONSTRUCTOR_FIELD_STORE,
                                    NULL,
                                    constructor_name,
                                    constructor_field,
                                    false,
                                    NULL,
                                    NULL);
                            }
                        }
                        if (sym->kind == SYMBOL_WORLD
                            && decl->type == AST_WORLD_DECL
                            && arg != NULL
                            && arg->type == AST_IDENTIFIER) {
                            const char *arg_name = ast_identifier_name(arg);
                            Symbol *arg_sym = scope_lookup(ctx->scope,
                                arg_name);
                            if (arg_sym != NULL && arg_sym->kind == SYMBOL_VARIABLE) {
                                const char *arg_type_name = NULL;
                                if (arg_sym->type != NULL
                                    && arg_sym->type->kind == TYPE_KIND_CLASS
                                    && arg_sym->type->name != NULL) {
                                    arg_type_name = arg_sym->type->name;
                                } else if (field_type != NULL
                                           && field_type->kind == TYPE_KIND_CLASS
                                           && field_type->name != NULL) {
                                    arg_type_name = field_type->name;
                                }
                                if (arg_type_name == NULL)
                                    continue;
                                size_t zone_count = 0;
                                ASTNode **zones = ast_world_zones(decl,
                                    &zone_count);
                                for (size_t zi = 0; zi < zone_count; zi++) {
                                    ASTNode *zone_slot = zones[zi];
                                    const char *zone_type =
                                        ast_world_zone_type_name(zone_slot);
                                    const char *zone_slot_name =
                                        ast_world_zone_slot_name(zone_slot);
                                    if (zone_slot == NULL
                                        || zone_slot->type != AST_WORLD_ZONE
                                        || zone_type == NULL) {
                                        continue;
                                    }
                                    if (strcmp(arg_type_name, zone_type) == 0) {
                                        semantic_error_with_hints(ctx,
                                            PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
                                            PGY_CAUSE_ANCHORED_HANDLE_COPY_ATTEMPT,
                                            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                                            arg,
                                            "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                                            "Reason:\n"
                                            "- origin binding is '%s'\n"
                                            "Contract source:\n"
                                            "- world '%s' zone slot '%s'\n"
                                            "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                                            "- ownership/authority after construction belongs to the world-owned slot, not the origin binding\n"
                                            "- owned embedding hands authority-bearing visibility to the world-owned zone slot\n"
                                            "- mutating the original binding afterwards would diverge from the world-owned handoff destination\n"
                                            "Fix:\n"
                                            "- use Clone(%s) for an explicit copy\n"
                                            "- or construct the zone inline / mutate it through the owning world after embedding",
                                            display_name != NULL ? display_name : "<world>",
                                            arg_name != NULL ? arg_name : "<zone>",
                                            zone_slot_name != NULL
                                                ? zone_slot_name : "<slot>",
                                            arg_name != NULL ? arg_name : "<zone>",
                                            display_name != NULL ? display_name : "<world>",
                                            zone_slot_name != NULL
                                                ? zone_slot_name : "<slot>",
                                            arg_name != NULL ? arg_name : "<zone>",
                                            display_name != NULL ? display_name : "<world>",
                                            zone_slot_name != NULL
                                                ? zone_slot_name : "<slot>",
                                            arg_name != NULL ? arg_name : "<zone>");
                                        arg_sym->embedded_in_world = true;
                                        semantic_ctx_mark_embedded_world_zone_name(ctx,
                                            arg_name,
                                            display_name,
                                            zone_slot_name);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        sym->is_used = true;
        if (type_out != NULL)
            *type_out = sym->type;
        return true;
    }


    return false;
}
