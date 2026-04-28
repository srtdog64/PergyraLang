/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker — late / call-path helpers.
 * Owns the large call-typing and borrowed-boundary argument validator logic
 * as a focused translation unit. See docs/101_semantic_split_template.md.
 */

#include "type_checker_internal.h"
#include "type_checker_visibility.h"
#include "type_checker_generic_diag_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "slot_analyzer.h"
#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

uint32_t
declared_effects_from_function_node(ASTNode *node, SemanticContext *ctx,
                                    bool *has_contract_out);

static Type *
late_helper_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_or_materialize(ctx, type_ref);
}

bool
semantic_find_active_slot_view(Scope *scope,
                               const char **view_name_out,
                               const char **view_kind_out,
                               const char **source_slot_out)
{
    if (view_name_out != NULL)
        *view_name_out = NULL;
    if (view_kind_out != NULL)
        *view_kind_out = NULL;
    if (source_slot_out != NULL)
        *source_slot_out = NULL;

    for (Scope *cur = scope; cur != NULL; cur = cur->parent) {
        for (size_t i = 0; i < cur->symbol_count; i++) {
            Symbol *sym = cur->symbols[i];
            bool is_read;
            bool is_write;

            if (sym == NULL || sym->type == NULL)
                continue;
            is_read = type_is_read_view(sym->type);
            is_write = type_is_write_view(sym->type);
            if (!is_read && !is_write)
                continue;

            if (view_name_out != NULL)
                *view_name_out = sym->name;
            if (view_kind_out != NULL)
                *view_kind_out = is_write ? "WriteView" : "ReadView";
            if (source_slot_out != NULL)
                *source_slot_out = sym->slot_info.paired_slot_name;
            return true;
        }
    }

    return false;
}

bool
semantic_find_active_slot_view_for_source(Scope *scope,
                                          const char *source_slot,
                                          const char **view_name_out,
                                          const char **view_kind_out,
                                          bool *is_write_view_out)
{
    if (view_name_out != NULL)
        *view_name_out = NULL;
    if (view_kind_out != NULL)
        *view_kind_out = NULL;
    if (is_write_view_out != NULL)
        *is_write_view_out = false;
    if (source_slot == NULL)
        return false;

    for (Scope *cur = scope; cur != NULL; cur = cur->parent) {
        for (size_t i = 0; i < cur->symbol_count; i++) {
            Symbol *sym = cur->symbols[i];
            bool is_read;
            bool is_write;

            if (sym == NULL || sym->type == NULL
                || sym->slot_info.paired_slot_name == NULL
                || strcmp(sym->slot_info.paired_slot_name, source_slot) != 0) {
                continue;
            }

            is_read = type_is_read_view(sym->type);
            is_write = type_is_write_view(sym->type);
            if (!is_read && !is_write)
                continue;

            if (view_name_out != NULL)
                *view_name_out = sym->name;
            if (view_kind_out != NULL)
                *view_kind_out = is_write ? "WriteView" : "ReadView";
            if (is_write_view_out != NULL)
                *is_write_view_out = is_write;
            return true;
        }
    }

    return false;
}

bool
semantic_reject_active_slot_owner_escape(ASTNode *site,
                                         SemanticContext *ctx,
                                         const char *escape_kind,
                                         const char *escape_name)
{
    const char *slot_name;
    Symbol *sym;
    const char *active_view_name = NULL;
    const char *active_view_kind = NULL;

    if (site == NULL || ctx == NULL || site->type != AST_IDENTIFIER
        || site->data.identifier.name == NULL) {
        return false;
    }

    slot_name = site->data.identifier.name;
    sym = scope_lookup(ctx->scope, slot_name);
    if (sym == NULL || sym->kind != SYMBOL_SLOT || sym->type == NULL
        || !type_is_owned_slot_handle(sym->type)) {
        return false;
    }

    if (!semantic_find_active_slot_view_for_source(ctx->scope, sym->name,
            &active_view_name, &active_view_kind, NULL)) {
        return false;
    }

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
        PGY_CAUSE_PIN_PARALLEL_CONFLICT,
        PGY_FIX_SERIALIZE_PIN_ACCESS,
        site,
        "Cannot store or forward slot '%s' through %s '%s' while %s '%s' is live.\n"
        "Reason:\n"
        "- pinned views are scoped capability leases over the source slot\n"
        "- escaping the owner handle would let code outside the view scope read, write, release, or forward the source\n"
        "Fix:\n"
        "- store a copied value read through the active view when that is intended\n"
        "- or end the pin/view scope before passing '%s' through %s",
        sym->name,
        escape_kind != NULL ? escape_kind : "boundary",
        escape_name != NULL ? escape_name : "<unknown>",
        active_view_kind != NULL ? active_view_kind : "view",
        active_view_name != NULL ? active_view_name : "<view>",
        sym->name,
        escape_kind != NULL ? escape_kind : "that boundary");
    return true;
}

static ASTNode *
lookup_function_param_contract_local(SemanticContext *ctx,
                                     const char *display_name,
                                     size_t arg_index,
                                     ParamMode *mode_out)
{
    if (mode_out != NULL)
        *mode_out = PARAM_MODE_DEFAULT;

    if (ctx == NULL || ctx->program_root == NULL || display_name == NULL)
        return NULL;

    ASTNode *prog = ctx->program_root;
    for (size_t si = 0; si < prog->data.program.count; si++) {
        ASTNode *stmt = prog->data.program.statements[si];
        if (stmt == NULL || stmt->type != AST_FUNC_DECL
            || stmt->data.func_decl.name == NULL
            || strcmp(stmt->data.func_decl.name, display_name) != 0
            || arg_index >= stmt->data.func_decl.param_count) {
            continue;
        }
        if (mode_out != NULL && stmt->data.func_decl.params[arg_index] != NULL)
            *mode_out = stmt->data.func_decl.params[arg_index]->mode;
        return stmt;
    }

    return NULL;
}

static unsigned
callable_param_escape_summary_local(ASTNode *callee_decl,
                                    size_t arg_index,
                                    SemanticContext *ctx)
{
    if (callee_decl == NULL
        || callee_decl->type != AST_FUNC_DECL
        || callee_decl->data.func_decl.body == NULL
        || arg_index >= callee_decl->data.func_decl.param_count) {
        return 0u;
    }

    return slot_analyze_param_summary_in_program(
        callee_decl->data.func_decl.body,
        callee_decl->data.func_decl.params[arg_index] != NULL
            ? callee_decl->data.func_decl.params[arg_index]->name
            : NULL,
        ctx != NULL ? ctx->program_root : NULL);
}

Type *
type_check_function_symbol_call(ASTNode *expr, Symbol *sym,
                                const char *display_name,
                                SemanticContext *ctx)
{
    ASTNode *callable_decl = NULL;

    if (sym == NULL) {
        if (name_looks_qualified(display_name)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                "Undefined function '%s' (expected a callable symbol). "
                "Check namespace spelling/export visibility or define/import a callable declaration before use.",
                display_name);
        } else {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_UNDEFINED_SYMBOL, PGY_CAUSE_SYMBOL_UNDEFINED, PGY_FIX_IMPORT_OR_DECLARE_SYMBOL, expr,
                "Undefined function '%s' (expected a callable symbol in current scope). "
                "Declare '%s' first, or use a qualified name for a namespaced declaration.",
                display_name, display_name);
        }
        return TYPE_UNKNOWN;
    }

    {
        Type *constructor_type = NULL;
        if (type_check_constructor_symbol_call(expr, sym, display_name, ctx,
                &constructor_type)) {
            return constructor_type != NULL ? constructor_type : TYPE_UNKNOWN;
        }
    }

    if (sym->type->kind != TYPE_KIND_FUNCTION) {
        char sig_buf[256];
        semantic_format_function_signature(sym->type, sig_buf, sizeof(sig_buf));
        if (sig_buf[0] == '\0')
            snprintf(sig_buf, sizeof(sig_buf), "fn(<signature unavailable>)");
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_CALL_NOT_CALLABLE, PGY_FIX_USE_CALLABLE_DECLARATION,
            expr,
            "'%s' resolves to a %s '%s' of type '%s', but function calls require a callable declaration. "
            "Expected a function signature matching %s. Use a function name, a callable variable, "
            "or adjust this symbol declaration before calling.",
            display_name,
            semantic_symbol_kind_label(sym->kind),
            display_name,
            type_name_or_unknown(sym->type),
            sig_buf);
        return TYPE_UNKNOWN;
    }
    if (ctx->program_root != NULL
        && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_INTENT)) {
        callable_decl = find_callable_decl_by_name(ctx->program_root, display_name);
        if (callable_decl != NULL && !explicit_type_reference_allowed(callable_decl, expr, ctx)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_VISIBILITY_BOUNDARY,
                PGY_CAUSE_VISIBILITY_BOUNDARY_CROSS, PGY_FIX_WIDEN_VISIBILITY_OR_MOVE_CALLER,
                expr,
                "Callable '%s' is not accessible across the current visibility boundary",
                display_name);
            return sym->type->data.function.return_type;
        }
    }
    sym->is_used = true;
    semantic_record_effect(ctx, type_function_effects(sym->type));
    semantic_record_callee_body_summary(ctx, sym->type);
    semantic_record_callable_decl_summary(ctx, callable_decl,
        type_function_effects(sym->type));
    if (ctx->in_parallel
        && type_effect_mask_has(type_function_effects(sym->type), EFFECT_SECURE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
            "Parallel context does not permit calling secure-effect function '%s'; serialize authority-bearing operations outside the parallel block",
            display_name);
        return sym->type->data.function.return_type;
    }

    size_t expected = sym->type->data.function.param_count;
    size_t provided = expr->data.call.arg_count;
    GenericParams *callable_generic_params = NULL;
    Type **effective_generic_types = NULL;
    if (provided != expected) {
        char sig_buf[256];
        semantic_format_function_signature(sym->type, sig_buf, sizeof(sig_buf));
        if (sig_buf[0] == '\0')
            snprintf(sig_buf, sizeof(sig_buf), "fn(<signature unavailable>)");
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID, PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr,
            "'%s' expects %llu argument(s), got %llu. Declared signature: %s. "
            "Adjust caller arguments to satisfy the declaration.",
            display_name, (unsigned long long) expected, (unsigned long long) provided, sig_buf);
        return sym->type->data.function.return_type;
    }

    Type **call_arg_types = calloc(provided > 0 ? provided : 1, sizeof(Type *));
    if (ctx->program_root != NULL
        && (sym->kind == SYMBOL_FUNCTION || sym->kind == SYMBOL_INTENT)) {
        callable_decl = find_callable_decl_by_name(ctx->program_root, display_name);
        if (callable_decl != NULL
            && callable_decl->type == AST_FUNC_DECL
            && callable_decl->data.func_decl.generic_params != NULL
            && callable_decl->data.func_decl.generic_params->count > 0) {
            callable_generic_params = callable_decl->data.func_decl.generic_params;
            effective_generic_types = calloc(
                callable_generic_params->count > 0
                    ? callable_generic_params->count : 1,
                sizeof(Type *));
            if (effective_generic_types != NULL) {
                for (size_t gi = 0; gi < callable_generic_params->count; gi++) {
                    GenericParam *gp = callable_generic_params->params[gi];
                    if (gp != NULL && gp->default_type != NULL)
                        effective_generic_types[gi] =
                            late_helper_resolve_type_ref(gp->default_type, ctx);
                }
            }
        }
    }

    for (size_t i = 0; i < provided; i++) {
        Type *param_type = sym->type->data.function.param_types[i];
        OwnershipTypeClass declared_param_ownership =
            semantic_classify_ownership_type(param_type, ctx);
        Type *arg_type = declared_param_ownership == OWNERSHIP_TYPE_MOVE_ONLY
            ? type_check_qubit_use(expr->data.call.arguments[i], ctx)
            : type_check_expression(expr->data.call.arguments[i], ctx);
        if (effective_generic_types != NULL
            && callable_decl != NULL
            && i < callable_decl->data.func_decl.param_count) {
            FuncParam *fp = callable_decl->data.func_decl.params[i];
            int param_index = -1;
            if (fp != NULL && fp->type != NULL
                && fp->type->type == AST_TYPE
                && fp->type->data.type.name != NULL) {
                param_index = find_generic_param_index(
                    callable_generic_params,
                    fp->type->data.type.name);
            }
            if (param_index >= 0
                && (size_t)param_index < callable_generic_params->count) {
                effective_generic_types[param_index] = arg_type;
                if (arg_type != NULL) {
                    param_type = arg_type;
                }
            }
        }
        if (call_arg_types != NULL)
            call_arg_types[i] = arg_type;

        OwnershipTypeClass param_ownership =
            semantic_classify_ownership_type(param_type, ctx);
        OwnershipTypeClass arg_ownership =
            semantic_classify_ownership_type(arg_type, ctx);

        if (param_ownership == OWNERSHIP_TYPE_MOVE_ONLY
            || arg_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (arg_ownership != OWNERSHIP_TYPE_MOVE_ONLY
                || param_ownership != OWNERSHIP_TYPE_MOVE_ONLY) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_TYPE_MOVABLE_HANDLE_REQUIRED, PGY_FIX_PROVIDE_MOVABLE_HANDLE,
                    expr->data.call.arguments[i],
                    "Movable resource argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            if (expr->data.call.arguments[i]->type != AST_IDENTIFIER
                && semantic_borrowed_boundary_root_name(
                       expr->data.call.arguments[i], ctx) == NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_MOVE_SOURCE_NOT_NAMED, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
                    expr->data.call.arguments[i],
                    "%s arguments must be moved from a named variable; bind the value first, then pass that variable",
                    resource_handle_display_name(param_type));
                continue;
            }
            callee_decl = lookup_function_param_contract_local(
                ctx, display_name, i, &pmode);
            if (pmode == PARAM_MODE_REF
                && callee_decl != NULL
                && callee_decl->data.func_decl.body != NULL) {
                const char *borrowed_name =
                    semantic_borrowed_boundary_root_name(
                        expr->data.call.arguments[i], ctx);
                unsigned callee_mask = callable_param_escape_summary_local(
                    callee_decl, i, ctx);
                if (borrowed_name != NULL
                    && (callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                                       | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                                       | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
                    semantic_validate_borrowed_escape(
                        expr->data.call.arguments[i],
                        expr->data.call.arguments[i],
                        ctx,
                        arg_type,
                        borrowed_name,
                        OWNERSHIP_CONSUMER_HELPER_CALL,
                        NULL,
                        display_name != NULL ? display_name : "<callee>",
                        NULL,
                        true,
                        NULL,
                        "slot handle (movable)");
                    continue;
                }
            }
            if (pmode != PARAM_MODE_REF) {
                const char *borrowed_name =
                    semantic_borrowed_boundary_root_name(
                        expr->data.call.arguments[i], ctx);
                if (borrowed_name != NULL) {
                    semantic_validate_borrowed_escape(
                        expr->data.call.arguments[i],
                        expr->data.call.arguments[i],
                        ctx,
                        arg_type,
                        borrowed_name,
                        OWNERSHIP_CONSUMER_HELPER_CALL,
                        NULL,
                        display_name != NULL ? display_name : "<callee>",
                        NULL,
                        false,
                        pmode == PARAM_MODE_OWN ? "own" : "default",
                        "slot handle (movable)");
                    continue;
                }
            }
            if (pmode == PARAM_MODE_REF)
                continue;
            consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            continue;
        }

        if (param_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (arg_ownership != OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_TYPE_SUBJECT_ARG_MISMATCH, PGY_FIX_ALIGN_SUBJECT_ARG_TYPE,
                    expr->data.call.arguments[i],
                    "Subject argument type mismatch: expected '%s', got '%s'.\n"
                    "Reason:\n"
                    "- subject boundary passing requires the caller value and callee contract to agree on the same subject type\n"
                    "- ownership/borrow provenance cannot be preserved across mismatched subject types\n"
                    "Fix:\n"
                    "- pass a value of type '%s'\n"
                    "- or change the callee contract to accept '%s'",
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type),
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type));
                continue;
            }
            callee_decl = lookup_function_param_contract_local(
                ctx, display_name, i, &pmode);
            if (!semantic_validate_borrowed_boundary_call_argument(
                    expr->data.call.arguments[i],
                    ctx,
                    callee_decl,
                    display_name,
                    i,
                    pmode,
                    arg_type,
                    "subject",
                    true)) {
                continue;
            }
            if (pmode == PARAM_MODE_OWN) {
                consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            }
            continue;
        }

        if (param_ownership == OWNERSHIP_TYPE_BORROW_TRACKED) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            bool track_boundary_borrow = true;
            if (!type_is_general_boundary_type(arg_type, ctx)
                || arg_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
                || arg_ownership == OWNERSHIP_TYPE_MOVE_ONLY
                || !type_is_assignable(arg_type, param_type)
                || !type_is_assignable(param_type, arg_type)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_TYPE_BOUNDARY_ARG_MISMATCH, PGY_FIX_ALIGN_BOUNDARY_ARG_TYPE,
                    expr->data.call.arguments[i],
                    "Boundary value argument type mismatch: expected '%s', got '%s'.\n"
                    "Reason:\n"
                    "- own/ref value boundaries require the caller value and callee contract to agree on the same boundary value type\n"
                    "- provenance cannot be preserved across mismatched boundary value types\n"
                    "Fix:\n"
                    "- pass a value of type '%s'\n"
                    "- or change the callee contract to accept '%s'",
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type),
                    type_name_or_unknown(param_type),
                    type_name_or_unknown(arg_type));
                continue;
            }
            callee_decl = lookup_function_param_contract_local(
                ctx, display_name, i, &pmode);
            if (!semantic_validate_borrowed_boundary_call_argument(
                    expr->data.call.arguments[i],
                    ctx,
                    callee_decl,
                    display_name,
                    i,
                    pmode,
                    arg_type,
                    "value",
                    track_boundary_borrow)) {
                continue;
            }
            if (pmode == PARAM_MODE_OWN) {
                const char *borrowed_name =
                    semantic_borrowed_boundary_root_name(
                        expr->data.call.arguments[i], ctx);
                if (track_boundary_borrow || borrowed_name == NULL)
                    consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            }
            continue;
        }

        if (param_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE
            || arg_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
            if (arg_ownership != OWNERSHIP_TYPE_ANCHORED_HANDLE
                || param_ownership != OWNERSHIP_TYPE_ANCHORED_HANDLE) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_TYPE_RESOURCE_HANDLE_ARG_MISMATCH, PGY_FIX_ALIGN_RESOURCE_HANDLE_ARG,
                    expr->data.call.arguments[i],
                    "Resource handle argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            {
                ParamMode pmode = PARAM_MODE_DEFAULT;
                ASTNode *callee_decl = lookup_function_param_contract_local(
                    ctx, display_name, i, &pmode);
                if (pmode == PARAM_MODE_DEFAULT) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                        PGY_CAUSE_SLOT_PARAM_QUALIFIER_MISSING,
                        PGY_FIX_ANNOTATE_SLOT_PARAM_QUALIFIER,
                        expr->data.call.arguments[i],
                        "Slot-handle (anchored) arguments require 'own' or 'ref' in the callee signature.\n"
                        "Reason:\n"
                        "- slot-handle (anchored) boundaries must declare whether the call borrows or transfers ownership\n"
                        "- implicit handle passing would hide that boundary contract\n"
                        "Fix:\n"
                        "- use 'own' in the callee signature to transfer the handle\n"
                        "- or use 'ref' to borrow it");
                    continue;
                }
                if (expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                    const char *src = expr->data.call.arguments[i]->data.identifier.name;
                    Symbol *src_sym = scope_lookup(ctx->scope, src);
                    const char *active_view_name = NULL;
                    const char *active_view_kind = NULL;
                    if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT
                        && semantic_find_active_slot_view_for_source(
                            ctx->scope,
                            src_sym->name,
                            &active_view_name,
                            &active_view_kind,
                            NULL)) {
                        semantic_error_with_hints(ctx,
                            PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                            PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                            PGY_FIX_SERIALIZE_PIN_ACCESS,
                            expr->data.call.arguments[i],
                            "Cannot pass slot '%s' to '%s' while %s '%s' is live.\n"
                            "Reason:\n"
                            "- slot helper calls may read, write, release, or forward the owner handle\n"
                            "- passing the owner while a view is live would bypass the view's aliasing contract\n"
                            "Fix:\n"
                            "- pass the active view to a view-typed helper\n"
                            "- or end the pin/view scope before calling '%s'",
                            src_sym->name,
                            display_name != NULL ? display_name : "<callee>",
                            active_view_kind != NULL ? active_view_kind : "view",
                            active_view_name != NULL ? active_view_name : "<view>",
                            display_name != NULL ? display_name : "<callee>");
                        continue;
                    }
                }
                if (!semantic_validate_borrowed_boundary_call_argument(
                        expr->data.call.arguments[i],
                        ctx,
                        callee_decl,
                        display_name,
                        i,
                        pmode,
                        arg_type,
                        "slot handle (anchored)",
                        true)) {
                    continue;
                }
                if (expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                    const char *src = expr->data.call.arguments[i]->data.identifier.name;
                    Symbol *src_sym = scope_lookup(ctx->scope, src);
                    if (pmode == PARAM_MODE_OWN) {
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
                            if (src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                                semantic_error_with_hints(ctx, PGY_CODE_SEM_MOVE_FROM_RELEASED, PGY_CAUSE_MOVE_FROM_RELEASED, PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE, expr->data.call.arguments[i],
                                    "Cannot move from released slot '%s'", src);
                            } else {
                                src_sym->slot_info.state = SLOT_STATE_RELEASED;
                            }
                        } else if (src_sym != NULL) {
                            src_sym->is_consumed = true;
                        }
                    } else if (pmode == PARAM_MODE_REF) {
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT
                            && src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error_with_hints(ctx, PGY_CODE_SEM_SLOT_RELEASED,
                                PGY_CAUSE_SLOT_BORROW_RELEASED,
                                PGY_FIX_RECLAIM_SOURCE_OR_TRACE_EARLIER_RELEASE,
                                expr->data.call.arguments[i],
                                "Cannot borrow released slot '%s'.\n"
                                "Reason:\n"
                                "- slot '%s' was already released or invalidated earlier in this scope\n"
                                "- a 'ref' borrow requires a live source handle\n"
                                "Fix:\n"
                                "- reacquire the slot before borrowing it\n"
                                "- or remove the earlier release/invalidating transfer",
                                src,
                                src);
                        }
                    }
                }
                continue;
            }
        }

        require_assignable(arg_type, param_type, expr->data.call.arguments[i], ctx);
    }

    /* Validate generic function where-clause constraints */
    if (ctx->program_root != NULL) {
        ASTNode *prog = ctx->program_root;
        for (size_t si = 0; si < prog->data.program.count; si++) {
            ASTNode *stmt = prog->data.program.statements[si];
            if (stmt != NULL && stmt->type == AST_FUNC_DECL
                && stmt->data.func_decl.name != NULL
                && strcmp(stmt->data.func_decl.name, display_name) == 0
                && stmt->data.func_decl.generic_params != NULL
                && stmt->data.func_decl.generic_params->count > 0
                && stmt->data.func_decl.where_clause != NULL) {
                /* Generic function with where clause — validate constraints */
                GenericParams *decl_gp = stmt->data.func_decl.generic_params;
                WhereClause *wc = stmt->data.func_decl.where_clause;
                char *expected_sig = format_generic_subject_signature(display_name, decl_gp);
                Type **effective_generic_types =
                    calloc(decl_gp->count > 0 ? decl_gp->count : 1, sizeof(Type *));
                if (effective_generic_types == NULL) {
                    free(expected_sig);
                    break;
                }

                for (size_t gi = 0; gi < decl_gp->count; gi++) {
                    GenericParam *gp = decl_gp->params[gi];
                    if (gp != NULL && gp->default_type != NULL)
                        effective_generic_types[gi] =
                            late_helper_resolve_type_ref(gp->default_type, ctx);
                }
                for (size_t ai = 0; ai < provided; ai++) {
                    FuncParam *fp = (ai < stmt->data.func_decl.param_count)
                        ? stmt->data.func_decl.params[ai] : NULL;
                    int param_index;
                    if (fp == NULL || fp->type == NULL
                        || fp->type->type != AST_TYPE
                        || fp->type->data.type.name == NULL) {
                        continue;
                    }
                    param_index = find_generic_param_index(
                        decl_gp, fp->type->data.type.name);
                    if (param_index < 0)
                        continue;
                    effective_generic_types[param_index] =
                        (call_arg_types != NULL) ? call_arg_types[ai] : NULL;
                }

                for (size_t ci = 0; ci < wc->count; ci++) {
                    TypeConstraint *tc = wc->constraints[ci];
                    int param_index;
                    const char *param_name;
                    Type *concrete_type;
                    const char *actual_sig =
                        format_effective_generic_type_list_scratch(
                            ctx, display_name, effective_generic_types, decl_gp->count);

                    if (tc == NULL || tc->type_param == NULL)
                    {
                        continue;
                    }

                    param_name = tc->type_param;
                    param_index = find_generic_param_index(decl_gp, param_name);
                    if (param_index < 0
                        || (size_t)param_index >= decl_gp->count) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr,
                            "Call site of '%s' could not validate generic parameter '%s'.\n"
                            "Reason:\n"
                            "- function '%s' where-clause references '%s'\n"
                            "- that parameter does not exist in the function generic parameter list\n"
                            "Fix:\n"
                            "- change the function where-clause to reference an existing generic parameter\n"
                            "- or add generic parameter '%s' to function '%s'",
                            display_name,
                            param_name != NULL ? param_name : "<param>",
                            display_name,
                            param_name != NULL ? param_name : "<param>",
                            param_name != NULL ? param_name : "<param>",
                            display_name);
                        continue;
                    }
                    concrete_type = effective_generic_types[param_index];
                    if (concrete_type == NULL) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr,
                            "Call site of '%s' could not materialize generic argument for '%s'.\n"
                            "Reason:\n"
                            "- function '%s' where-clause validation depends on an effective type argument for '%s'\n"
                            "- neither explicit call-site evidence nor declaration defaults produced a concrete type\n"
                            "Fix:\n"
                            "- pass arguments that make '%s' concrete\n"
                            "- or add/fix a default type argument for '%s'",
                            display_name,
                            param_name != NULL ? param_name : "<param>",
                            display_name,
                            param_name != NULL ? param_name : "<param>",
                            param_name != NULL ? param_name : "<param>",
                            param_name != NULL ? param_name : "<param>");
                        continue;
                    }
                    /* Check each bound */
                    for (size_t bi = 0; bi < tc->bound_count; bi++) {
                        char *bounds_text = format_type_constraint_bounds(tc);
                        const char *bound_name =
                            (tc->bounds[bi] != NULL
                             && tc->bounds[bi]->type == AST_TYPE
                             && tc->bounds[bi]->data.type.name != NULL)
                                ? tc->bounds[bi]->data.type.name
                                : NULL;
                        bool satisfies = concrete_type_satisfies_bound(
                            concrete_type, tc->bounds[bi], ctx);
                        if (!satisfies) {
                            semantic_report_function_generic_bound_failure(
                                ctx,
                                expr,
                                display_name,
                                param_name,
                                bound_name != NULL ? bound_name : "<constraint>",
                                bounds_text,
                                expected_sig != NULL ? expected_sig : display_name,
                                actual_sig != NULL ? actual_sig : display_name,
                                concrete_type->name != NULL ? concrete_type->name : "<type>");
                        }
                        free(bounds_text);
                    }
                }
                free(expected_sig);
                free(effective_generic_types);
                break;
            }
        }
    }

    Type *return_type = sym->type->data.function.return_type;
    if (effective_generic_types != NULL
        && callable_decl != NULL
        && callable_decl->type == AST_FUNC_DECL
        && callable_decl->data.func_decl.return_type != NULL
        && callable_decl->data.func_decl.return_type->type == AST_TYPE
        && callable_decl->data.func_decl.return_type->data.type.name != NULL) {
        int return_index = find_generic_param_index(
            callable_generic_params,
            callable_decl->data.func_decl.return_type->data.type.name);
        if (return_index >= 0
            && (size_t)return_index < callable_generic_params->count
            && effective_generic_types[return_index] != NULL) {
            return_type = effective_generic_types[return_index];
        }
    }

    free(effective_generic_types);
    free(call_arg_types);
    return return_type;
}
