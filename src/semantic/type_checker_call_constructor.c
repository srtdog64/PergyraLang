/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Constructor-like symbol call validation for subjects, overlays, worlds, and
 * zones.  Keeping this outside the late function-call owner prevents regular
 * callable argument validation from owning domain constructor diagnostics.
 */

#include "type_checker_internal.h"
#include "type_checker_call_constructor_internal.h"
#include "callable_capability_inference.h"
#include "string_window_extent.h"
#include "array_storage_release_owner.h"
#include "type_checker_visibility.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "function_param_publication_owner.h"

#include <stdlib.h>
#include <string.h>

static Type *
constructor_call_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
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
            SemanticNominalResultSnapshot *snapshot = NULL;
            bool local_ok = false, metadata_ok = true;
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
                field_count = projection_source_field_count(decl);
                decl_is_generic =
                    (ast_generic_param_count(class_generics) > 0);
            } else if (decl != NULL
                       && (decl->type == AST_RELATION_DECL
                           || decl->type == AST_EFFECT_DECL
                           || decl->type == AST_PARTY_DECL
                           || decl->type == AST_ROSTER_DECL
                           || decl->type == AST_WORLD_DECL
                           || decl->type == AST_ZONE_DECL)) {
                if (domain_constructor_has_role_filtered_inputs(decl))
                    (void)domain_constructor_input(decl, SIZE_MAX, &field_count);
                else
                    field_count = overlay_field_count(decl);
            }
            if (decl != NULL) {
                size_t provided = ast_call_arg_count(expr);
                const char *channel_field_name = NULL;
                semantic_string_window_extent_record_constructor(ctx, expr, decl);
                if (constructor_decl_has_channel_field(
                        decl, ctx, &channel_field_name)) {
                    constructor_reject_channel_field_store(ctx,
                        expr, display_name, channel_field_name);
                }
                /* A struct is a plain value record, so every field comes
                 * from the call, as the self-host checker requires; the
                 * missing ones used to be zero-filled. Domain nominals
                 * may still omit trailing fields on both front ends. */
                bool exact_fields = decl->type == AST_CLASS_DECL
                    && ast_class_is_struct(decl);
                local_ok = exact_fields && !decl_is_generic && provided == field_count &&
                    ast_class_field_destructure_count(decl) == 0 && channel_field_name == NULL &&
                    constructor_payload_type_ready(sym->type) && ast_node_stable_id(decl) != 0;
                if (local_ok && semantic_param_publication_pending(ctx)) {
                    snapshot = calloc(1, sizeof(*snapshot));
                    if (snapshot != NULL) {
                        snapshot->decl = decl;
                        snapshot->decl_id = ast_node_stable_id(decl);
                        snapshot->field_count = field_count;
                        snapshot->decl_name = strdup(ast_class_name(decl));
                        snapshot->result_type_name = strdup(sym->type->name);
                        snapshot->fields = calloc(field_count, sizeof(*snapshot->fields));
                    }
                    metadata_ok = snapshot != NULL && snapshot->decl_name != NULL &&
                        snapshot->result_type_name != NULL && (field_count == 0 || snapshot->fields != NULL);
                }
                /* Skip field-type validation for generic classes — the
                 * generic params (T, U) aren't in scope at the call site.
                 * Type safety is handled by the let-annotation type. */
                if (exact_fields && provided < field_count) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr,
                        "Constructor '%s' takes exactly %llu field argument(s), got %llu; a struct has no default fields",
                        display_name, (unsigned long long) field_count, (unsigned long long) provided);
                } else if (provided > field_count) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr,
                        "Constructor '%s' accepts at most %llu positional field argument(s), got %llu",
                        display_name, (unsigned long long) field_count, (unsigned long long) provided);
                } else if (decl_is_generic) {
                    for (size_t i = 0; i < provided; i++) {
                        ASTNode *arg = ast_call_argument(expr, i);
                        Type *arg_type = constructor_call_normalize_type(
                            type_check_expression(arg, ctx));
                        callable_capability_record_retained_value(ctx, expr, arg, arg_type);
                        if (semantic_reject_affine_aggregate_storage(
                                arg, arg_type, ctx,
                                "generic nominal constructor field"))
                            continue;
                        if (type_equals(arg_type, TYPE_VOID)) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_TYPE_MISMATCH,
                                PGY_CAUSE_ASSIGNABILITY_CHECK,
                                PGY_FIX_ALIGN_OPERAND_TYPE,
                                arg,
                                "Void expression cannot initialize constructor field; split the side effect before constructing '%s'",
                                display_name != NULL
                                    ? display_name : "<constructor>");
                        }
                    }
                } else {
                    for (size_t i = 0; i < provided; i++) {
                        size_t selected = i;
                        const char *field_name = NULL;
                        ASTNode *field_type_node = NULL;
                        ASTNode *arg = ast_call_argument(expr, i);
                        Type *arg_type = NULL;
                        Type *field_type = NULL;
                        const char *arg_nm =
                            ast_call_argument_name(expr, i);
                        if (arg_nm != NULL) {
                            field_type_node =
                                constructor_decl_field_type_by_name(
                                    decl, arg_nm, field_count, &field_name, &selected);
                            if (field_type_node == NULL) {
                                local_ok = false;
                                (void)type_check_expression(arg, ctx);
                                semantic_error_with_hints(ctx,
                                    PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
                                    PGY_CAUSE_CLASS_CONTRACT,
                                    PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN,
                                    arg,
                                    "Constructor '%s' has no field named '%s'",
                                    display_name, arg_nm);
                                continue;
                            }
                            field_type = semantic_host_resolve_type_ref(field_type_node, ctx);
                        } else {
                            field_type = constructor_decl_field_resolved_type_at(
                                decl, i, &field_name, ctx);
                        }
                        /* The field type is the argument's storage site. */
                        arg_type = constructor_call_normalize_type(
                            type_check_expression_at_typed_site(
                                arg, field_type, ctx));
                        callable_capability_record_retained_value(ctx, expr, arg, arg_type);
                        if (field_type == NULL)
                        {
                            local_ok = false;
                            continue;
                        }
                        if (type_equals(arg_type, TYPE_VOID)) {
                            semantic_error_with_hints(ctx,
                                PGY_CODE_SEM_TYPE_MISMATCH,
                                PGY_CAUSE_ASSIGNABILITY_CHECK,
                                PGY_FIX_ALIGN_OPERAND_TYPE,
                                arg,
                                "Void expression cannot initialize constructor field '%s.%s'; split the side effect before construction",
                                display_name != NULL
                                    ? display_name : "<constructor>",
                                field_name != NULL ? field_name : "<field>");
                            arg_type = TYPE_UNKNOWN;
                        }
                        if (!type_is_assignable(arg_type, field_type)) {
                            local_ok = false;
                            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, arg,
                                "Constructor '%s' argument %llu initializes field '%s' of type '%s', got '%s'",
                                display_name, (unsigned long long) (i + 1),
                                field_name != NULL ? field_name : "<field>",
                                field_type->name != NULL ? field_type->name : "<type>",
                                arg_type->name != NULL ? arg_type->name : "<type>");
                        }
                        if (arg != NULL) {
                            semantic_array_storage_escape(arg, arg_type, ctx);
                            const char *borrowed_name =
                                semantic_borrowed_boundary_root_name(
                                    arg, ctx);
                            const char *constructor_name =
                                display_name != NULL ? display_name : "<constructor>";
                            const char *constructor_field =
                                field_name != NULL ? field_name : "<field>";

                            if (borrowed_name != NULL) {
                                if (semantic_validate_borrowed_escape(
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
                                    NULL)) local_ok = false;
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
                        if (!constructor_payload_type_ready(arg_type) ||
                            !constructor_payload_type_ready(field_type)) local_ok = false;
                        if (local_ok) {
                            size_t count = 0;
                            ClassField **fields = ast_class_fields(decl, &count);
                            if (selected >= count || fields[selected]->default_value != NULL ||
                                fields[selected]->stable_id == 0 || ast_node_stable_id(fields[selected]->type) == 0)
                                local_ok = false;
                            for (size_t j = 0; j < i && local_ok; j++) {
                                size_t prior = j;
                                const char *prior_name = ast_call_argument_name(expr, j);
                                if (prior_name != NULL)
                                    (void)constructor_decl_field_type_by_name(decl, prior_name, count, NULL, &prior);
                                if (prior == selected) local_ok = false;
                            }
                            if (local_ok && metadata_ok && snapshot != NULL) {
                                SemanticNominalResultField *row = &snapshot->fields[i];
                                row->field_index = selected;
                                row->field = fields[selected];
                                row->field_id = row->field->stable_id;
                                row->type_node = row->field->type;
                                row->type_id = ast_node_stable_id(row->type_node);
                                row->field_name = strdup(row->field->name);
                                row->argument_name = arg_nm != NULL ? strdup(arg_nm) : NULL;
                                row->field_type_name = strdup(field_type->name);
                                row->argument_type_name = strdup(arg_type->name);
                                row->type_syntax_name = ast_capture_inline(row->type_node);
                                metadata_ok = row->field_name != NULL && row->field_type_name != NULL &&
                                    row->argument_type_name != NULL && row->type_syntax_name != NULL &&
                                    (arg_nm == NULL || row->argument_name != NULL);
                            }
                        }
                    }
                }
            }
            if (local_ok && semantic_param_publication_pending(ctx) && !ctx->has_error) {
                if (!metadata_ok || !ast_call_set_semantic_value_type_name_copy(expr, sym->type->name) ||
                    !semantic_param_publication_record_nominal_result(ctx, expr, snapshot))
                    semantic_error(ctx, expr, "Checked nominal result publication failed");
                else
                    snapshot = NULL;
            }
            semantic_nominal_result_snapshot_destroy(snapshot);
        }
        if (sym->kind == SYMBOL_ZONE && ctx != NULL) {
            ASTNode *zone_decl = semantic_constructor_decl_for_symbol_kind(ctx,
                sym->kind, display_name);
            if (zone_decl != NULL && zone_decl->type == AST_ZONE_DECL)
                semantic_reject_zone_subject_embedding(expr, zone_decl,
                    display_name, ctx);
        }
        if (sym->kind == SYMBOL_WORLD && ctx != NULL) {
            /* Nested constructor arguments (World(Zone(subject), ...)) are
             * not descended by the field loop above when the world slot has
             * no field metadata, so the zone-level boundary-fork check is
             * applied to each zone-constructor argument here. */
            for (size_t ai = 0; ai < ast_call_arg_count(expr); ai++) {
                ASTNode *warg = ast_call_argument(expr, ai);
                ASTNode *wcallee;
                const char *zname;
                ASTNode *zdecl;

                if (warg == NULL || warg->type != AST_CALL)
                    continue;
                wcallee = ast_call_callee(warg);
                if (wcallee == NULL || wcallee->type != AST_IDENTIFIER)
                    continue;
                zname = ast_identifier_name(wcallee);
                zdecl = semantic_find_zone_decl_by_name(ctx, zname);
                if (zdecl != NULL && zdecl->type == AST_ZONE_DECL)
                    semantic_reject_zone_subject_embedding(warg, zdecl,
                        zname, ctx);
            }
        }
        sym->is_used = true;
        if (type_out != NULL)
            *type_out = sym->type;
        return true;
    }


    return false;
}
