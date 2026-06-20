#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "diag_codes.h"
#include "type_checker_module_contract_internal.h"
#include "type_checker_ownership_consumers_internal.h"

bool
type_check_func_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = ast_declaration_name(node);
    bool is_action = (!node->is_async_decl && ast_func_is_action(node));
    ASTNode *enclosing_nominal = ctx->current_nominal_decl;
    ASTNode *prev_function_decl = ctx->current_function_decl;
    uint32_t prev_effects = ctx->current_function_effects;
    uint32_t prev_body_summary = ctx->current_function_body_summary;
    bool prev_tracking = ctx->tracking_function_effects;
    bool prev_async = ctx->in_async_func;
    const char *prev_module_path = ctx->current_module_path;
    bool has_effect_contract = false;
    uint32_t declared_effects =
        declared_effects_from_function_node(node, ctx, &has_effect_contract);

    semantic_validate_action_func_contract(node, ctx, enclosing_nominal, name, is_action);

    /* subject now allows both func (private internal computation)
     * and action (public plot behavior with zone/effect/authority). */

    /* Register function generic parameters as opaque metadata-visible types for
     * parameter and return type resolution. */
    GenericParams *func_generics = ast_func_generic_params(node);
    WhereClause *func_where_clause = ast_func_where_clause(node);
    bool has_generics = (ast_generic_param_count(func_generics) > 0);
    if (has_generics) {
        validate_generic_param_defaults(func_generics, ctx, node, "function");
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        GenericParams *gp = func_generics;
        size_t generic_count = ast_generic_param_count(gp);
        for (size_t gi = 0; gi < generic_count; gi++) {
            GenericParam *param = ast_generic_param_at(gp, gi);
            const char *param_name = ast_generic_param_name(param);
            if (param_name == NULL)
                continue;
            Type *tp = type_create_generic(param_name);
            Symbol *s = symbol_create_variable(
                param_name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_TYPE_PARAM;
            scope_declare(ctx->scope, s);
        }
    }

    /* Build parameter types for the function type */
    size_t   param_count = ast_func_param_count(node);
    Type   **param_types = NULL;

    if (param_count > 0) {
        param_types = calloc(param_count, sizeof(Type *));
        if (param_types == NULL) {
            if (has_generics) scope_exit(&ctx->scope);
            return false;
        }
    }

    Type *return_type = type_check_func_resolve_return_type(node, ctx);
    /* No `-> Type` annotation: infer from the body. Use UNKNOWN as the working
     * return type during body checking (the missing-return and Void-return
     * checks both skip UNKNOWN), then finalize after the body. */
    bool infer_return = (ast_func_return_type(node) == NULL);
    if (infer_return)
        return_type = TYPE_UNKNOWN;
    if (type_is_class_object_type(return_type, ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
            PGY_CAUSE_ANCHORED_HANDLE_RETURN_BOUNDARY,
            PGY_FIX_RETURN_PROJECTION_OR_KEEP_LOCAL,
            ast_func_return_type(node),
            "Returning subjects by value is not supported yet.\n"
            "Reason:\n"
            "- return type '%s' is a subject, and subject values are zone/world slot handles (anchored)\n"
            "Fix:\n"
            "- return a struct/class/object/tobject projection instead\n"
            "- keep the subject local to its owning zone/world\n"
            "- or use Box<T>/another explicit handle layer",
            type_name_or_unknown(return_type));
    }

    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = ast_func_param(node, i);
        if (param == NULL) {
            param_types[i] = TYPE_UNKNOWN;
            continue;
        }
        /* Implicit 'self' type: if a parameter named "self" has no
         * type annotation and we're inside a class scope, infer the
         * enclosing class type. */
        if (param->type == NULL && param->name != NULL
            && strcmp(param->name, "self") == 0
            && ctx->scope != NULL
            && (ctx->scope->kind == SCOPE_CLASS
                || type_check_func_current_implicit_self_host_name(ctx) != NULL)) {
            Scope *parent = ctx->scope->parent;
            const char *nominal_name =
                type_check_func_current_implicit_self_host_name(ctx);

            if (parent != NULL && nominal_name != NULL) {
                Symbol *self_sym = scope_lookup(parent, nominal_name);
                if (type_check_func_symbol_is_self_host(self_sym))
                    param_types[i] = self_sym->type;
            }

            if (param_types[i] == NULL) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_CALL_ARG_TYPE_MISMATCH,
                    PGY_FIX_REPORT_COMPILER_BUG,
                    node,
                    "Cannot infer implicit self type for function '%s'.\n"
                    "Reason:\n"
                    "- function is in a nominal scope, but current_nominal_decl is not set\n"
                    "- beta semantic lowering no longer guesses self from the parent scope symbol order\n"
                    "Fix:\n"
                    "- route class/subject/enum method checking through the nominal owner pass\n"
                    "- or add an explicit self type annotation",
                    name != NULL ? name : "<anonymous>");
                param_types[i] = TYPE_UNKNOWN;
            }
        } else {
            param_types[i] = type_check_func_resolve_param_type(param, ctx);
        }
        type_check_func_validate_param_boundary(node, ctx, name, param,
            param_types[i]);
        /* Subject parameters are passed by reference (pointer) internally.
         * The language hides pointer semantics from the user: subjects
         * are identity-bearing types, so reference passing is automatic. */
    }

    Type *func_type = type_create_function(param_types, param_count,
                                            return_type);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = ast_func_param(node, i);
        if (param != NULL)
            type_function_set_param_mode(func_type, i, param->mode);
    }

    Symbol *func_sym = symbol_create_function(name, func_type,
                                               node->line, node->column);
    /* If a forward-declaration placeholder exists from Pass 1,
       update its type instead of re-declaring. */
    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind == SYMBOL_FUNCTION) {
        existing->type = func_type;
        symbol_destroy(func_sym);
    } else if (!scope_declare(ctx->scope, func_sym)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_FUNCTION_DUPLICATE_NAME,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node, "Redeclaration of function '%s'", name);
        symbol_destroy(func_sym);
        return false;
    }

    /* Close the temporary generic-params scope (if opened) before
     * entering the real function scope — the function scope will
     * re-register the generic params so they're visible in the body. */
    if (has_generics)
        scope_exit(&ctx->scope);

    validate_where_clause_bounds(func_where_clause, ctx, node);
    validate_generic_param_default_bounds(
        func_generics,
        func_where_clause,
        ctx,
        node,
        "function",
        name);

    /* Check body in new function scope */
    scope_enter(&ctx->scope, SCOPE_FUNCTION);
    if (node->origin_path != NULL)
        ctx->current_module_path = node->origin_path;

    /* Re-register generic type params inside the function scope */
    if (has_generics) {
        GenericParams *gp = func_generics;
        size_t generic_count = ast_generic_param_count(gp);
        for (size_t gi = 0; gi < generic_count; gi++) {
            GenericParam *param = ast_generic_param_at(gp, gi);
            const char *param_name = ast_generic_param_name(param);
            if (param_name == NULL)
                continue;
            Type *tp = type_create_generic(param_name);
            Symbol *s = symbol_create_variable(
                param_name,
                tp != NULL ? tp : TYPE_UNKNOWN,
                node->line, node->column);
            s->kind = SYMBOL_TYPE_PARAM;
            scope_declare(ctx->scope, s);
        }
    }

    Type *prev_return  = ctx->current_return;
    bool  prev_inferring = ctx->inferring_return;
    Type *prev_inferred  = ctx->inferred_return;
    bool  prev_infer_conflict = ctx->inferred_return_conflict;
    ctx->current_function_decl = node;
    ctx->current_return = return_type;
    ctx->inferring_return = infer_return;
    ctx->inferred_return = NULL;
    ctx->inferred_return_conflict = false;
    ctx->current_function_effects = EFFECT_NONE;
    ctx->current_function_body_summary = BODY_SUMMARY_NONE;
    ctx->tracking_function_effects = true;
    ctx->in_async_func = prev_async || node->is_async_decl;

    if (declared_effects != EFFECT_NONE)
        semantic_record_body_summary(ctx, BODY_SUMMARY_EFFECTS);
    if (ast_func_within_zone(node) != NULL)
        semantic_record_body_summary(ctx, BODY_SUMMARY_REQUIRES_ZONE);

    /* Register parameters */
    for (size_t i = 0; i < param_count; i++) {
        Type *pt = type_function_param_type(func_type, i);
        FuncParam *param = ast_func_param(node, i);
        const char *param_name = param != NULL ? param->name : NULL;
        if (param != NULL && param->mode == PARAM_MODE_OWN)
            semantic_record_body_summary(ctx, BODY_SUMMARY_MOVES_PARAM);
        else if (param != NULL && param->mode == PARAM_MODE_REF)
            semantic_record_body_summary(ctx, BODY_SUMMARY_BORROWS_PARAM);
        if (type_slot_is_secure(pt))
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (type_is_constructed_named(pt, "Token"))
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (type_is_subject_host_slot_handle(pt, ctx) && param_name != NULL) {
            char token_name[256];
            const char *paired_token = NULL;
            Symbol *slot_sym;

            if (type_slot_is_secure(pt)) {
                if (!semantic_format_secure_token_name(
                        token_name, sizeof(token_name), param_name, node, ctx))
                    continue;
                paired_token = token_name;
            }

            slot_sym = symbol_create_slot(param_name, pt,
                type_slot_is_secure(pt), paired_token,
                node->line, node->column);
            scope_declare(ctx->scope, slot_sym);
            scope_register_slot(ctx->scope, slot_sym);

            if (type_slot_is_secure(pt)) {
                Symbol *tok = symbol_create_token(token_name, param_name,
                    node->line, node->column);
                if (tok != NULL) {
                    Type *token_args[1] = { type_slot_inner_type(pt) };
                    tok->type = type_create_constructed(TYPE_TOKEN, token_args, 1);
                }
                scope_declare(ctx->scope, tok);
            }
            continue;
        }

        Symbol *p = symbol_create_variable(
            param_name, pt,
            node->line, node->column);
        scope_declare(ctx->scope, p);
    }

    if (ast_func_body(node) != NULL) {
        SemanticBodyFlowSummary flow_summary = {0};
        semantic_check_body_flow_summary(ast_func_body(node), ctx, &flow_summary);
        if (!type_equals(return_type, TYPE_VOID)
            && return_type != TYPE_UNKNOWN
            && !flow_summary.must_return) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_MISSING_RETURN,
                PGY_CAUSE_CFG_MISSING_RETURN,
                PGY_FIX_ADD_RETURN_ON_ALL_PATHS,
                node,
                "Function '%s' with return type '%s' may fall through without returning a value.\n"
                "Reason:\n"
                "- the CFG body summary has at least one reachable normal path without a return terminator\n"
                "- summary: fallthrough=%s return=%s break=%s continue=%s defer=%s\n"
                "- beta-stable body safety requires non-Void functions to return on every normal path\n"
                "Fix:\n"
                "- add a return on the missing branch/path\n"
                "- or change the function return type to Void if falling through is intended",
                name != NULL ? name : "<anonymous>",
                return_type->name != NULL ? return_type->name : "<unknown>",
                flow_summary.has_fallthrough ? "yes" : "no",
                flow_summary.has_return ? "yes" : "no",
                flow_summary.has_break ? "yes" : "no",
                flow_summary.has_continue ? "yes" : "no",
                flow_summary.has_defer ? "yes" : "no");
        }
    }

    semantic_check_param_summary_escapes(node, param_count, param_types,
        func_type, ctx);

    {
        uint32_t derived_effects = type_effect_mask_closure(ctx->current_function_effects);
        uint32_t missing_effects =
            type_effect_mask_closure(derived_effects) & ~type_effect_mask_closure(declared_effects);
        char derived_buf[128];
        char missing_buf[128];
        char declared_buf[128];

        if (has_effect_contract && missing_effects != EFFECT_NONE) {
            effect_mask_to_string(derived_effects, derived_buf, sizeof(derived_buf));
            effect_mask_to_string(missing_effects, missing_buf, sizeof(missing_buf));
            effect_mask_to_string(declared_effects, declared_buf, sizeof(declared_buf));
            semantic_error_with_hints(ctx, PGY_CODE_SEM_EFFECT_CONFLICT, PGY_CAUSE_EFFECT_INCOMPATIBLE_COMBO, PGY_FIX_SPLIT_EFFECT_FAMILIES, node,
                "Function '%s' is missing declared effects: %s (declared: %s, derived from body: %s)",
                name != NULL ? name : "<anonymous>",
                missing_buf, declared_buf, derived_buf);
        }

        type_function_set_effects(func_type,
            type_effect_mask_join(declared_effects, derived_effects));

        {
            const char *within_zone = ast_func_within_zone(node);
            uint32_t effect_closure = type_function_effects(func_type);
            if (within_zone != NULL
                && type_effect_mask_has(effect_closure, EFFECT_UNSAFE)) {
                ASTNode *forbidding_zone =
                    semantic_find_zone_decl_by_name(ctx, within_zone);
                if (forbidding_zone != NULL
                    && ast_zone_forbids_unsafe(forbidding_zone)) {
                    semantic_error_with_hints(ctx,
                        PGY_CODE_SEM_EFFECT_CONFLICT,
                        PGY_CAUSE_EFFECT_INCOMPATIBLE_COMBO,
                        PGY_FIX_SPLIT_EFFECT_FAMILIES,
                        node,
                        "Function '%s' performs unsafe work, but zone '%s' "
                        "forbids unsafe; raw-memory operations may not be "
                        "contained within a zone that declares 'forbids unsafe'. "
                        "Move the unsafe work outside the zone boundary.",
                        name != NULL ? name : "<anonymous>",
                        within_zone);
                }
            }
        }

        if (type_effect_mask_conflicts(type_function_effects(func_type),
                                       type_function_effects(func_type))) {
            effect_mask_to_string(type_function_effects(func_type),
                                  derived_buf, sizeof(derived_buf));
            semantic_warning_with_hints(ctx,
                PGY_CODE_SEM_EFFECT_CONFLICT,
                PGY_CAUSE_EFFECT_INCOMPATIBLE_COMBO,
                PGY_FIX_SPLIT_EFFECT_FAMILIES,
                node,
                "Function '%s' combines effect classes that are currently treated as conflicting (%s).\n"
                "Reason:\n"
                "- derived body effects joined into '%s'\n"
                "- current partial order still treats part of that join as conflicting in one routine\n"
                "- this usually means authority-sensitive work and boundary/resource work were merged in one flow\n"
                "Fix:\n"
                "- split the routine into smaller helpers so each helper owns one effect family\n"
                "- or isolate the conflicting branch/handoff path behind an explicit boundary helper",
                name != NULL ? name : "<anonymous>",
                derived_buf,
                derived_buf);
        }

        if (is_action
            && ast_func_within_zone(node) != NULL
            && type_effect_mask_requires_authority(type_function_effects(func_type))
            && ast_func_authorized_by_count(node) == 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "secure action '%s' within zone '%s' must declare 'authorized by'.\n"
                "Reason:\n"
                "- action body derives authority-sensitive effects from authority-bearing work\n"
                "- zone '%s' makes this action part of an explicit authority boundary\n"
                "Contract source:\n"
                "- action header 'within %s' plus the derived authority-sensitive effect path inside the body\n"
                "- without 'authorized by', the approval provenance for that boundary is missing\n"
                "Fix:\n"
                "- add 'authorized by <subject-slot>' to the action contract\n"
                "- or move the authority-sensitive work behind a helper that is called from an already-authorized action",
                name != NULL ? name : "<anonymous>",
                ast_func_within_zone(node),
                ast_func_within_zone(node),
                ast_func_within_zone(node));
        }
        if (is_action
            && ast_func_within_zone(node) != NULL
            && ast_func_causes_effect(node) != NULL
            && ast_func_authorized_by_count(node) == 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ACTION_CONTRACT_INVALID, PGY_CAUSE_ACTION_CONTRACT, PGY_FIX_ALIGN_ACTION_SURFACE_WITH_ZONE, node,
                "action '%s' causing effect '%s' within zone '%s' must declare 'authorized by'.\n"
                "Reason:\n"
                "- action contract declares causes '%s'\n"
                "- causing an effect inside zone '%s' is an authority-sensitive state change\n"
                "Contract source:\n"
                "- action header 'causes %s' together with 'within %s'\n"
                "- without 'authorized by', the approval provenance for that state change is missing\n"
                "Fix:\n"
                "- add 'authorized by <subject-slot>' to the action contract\n"
                "- or remove/change the causes clause if this action should stay authority-free",
                name != NULL ? name : "<anonymous>",
                ast_func_causes_effect(node),
                ast_func_within_zone(node),
                ast_func_causes_effect(node),
                ast_func_within_zone(node),
                ast_func_causes_effect(node),
                ast_func_within_zone(node));
        }
    }

    type_function_set_body_summary(func_type,
        ctx->current_function_body_summary);

    /* Finalize inferred return type: no value-returns -> Void; otherwise the
     * unified type accumulated from the body. Update the function symbol's type
     * so later callers see the inferred return. Conflicts already reported a
     * loud diagnostic at the disagreeing return. */
    if (infer_return) {
        Type *final_ret = TYPE_VOID;
        if (ctx->inferred_return_conflict) {
            final_ret = TYPE_UNKNOWN;        /* already reported at the return */
        } else if (ctx->inferred_return != NULL) {
            if (ctx->inferred_return == TYPE_UNKNOWN) {
                /* Returns are present but a return type stayed unresolved -
                 * typically an unannotated recursive call before any concrete
                 * return. Require an explicit annotation (C++ auto rule). */
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_INFER_REQUIRED,
                    PGY_CAUSE_INFER_NO_SOURCE,
                    PGY_FIX_ALIGN_OPERAND_TYPE,
                    node,
                    "Cannot infer the return type of function '%s'; add an explicit '-> Type'.\n"
                    "Reason:\n"
                    "- a return expression's type is not resolvable here (often a recursive call before a concrete return)\n"
                    "- return-type inference is local; it does not solve recursive fixpoints\n"
                    "Fix:\n"
                    "- annotate the return type so recursion and forward references resolve",
                    name != NULL ? name : "<anonymous>");
                final_ret = TYPE_UNKNOWN;
            } else {
                final_ret = ctx->inferred_return;
            }
        }
        type_function_set_return_type(func_type, final_ret);
        if (final_ret != NULL && final_ret != TYPE_UNKNOWN
            && final_ret->name != NULL
            && !ast_func_set_semantic_return_type_name_copy(node,
                   final_ret->name)) {
            semantic_error(ctx, node,
                "Out of memory while recording inferred function return type");
        }
    }
    ctx->inferring_return = prev_inferring;
    ctx->inferred_return = prev_inferred;
    ctx->inferred_return_conflict = prev_infer_conflict;

    ctx->current_return = prev_return;
    ctx->current_function_decl = prev_function_decl;
    ctx->current_function_effects = prev_effects;
    ctx->current_function_body_summary = prev_body_summary;
    ctx->tracking_function_effects = prev_tracking;
    ctx->in_async_func = prev_async;
    ctx->current_module_path = prev_module_path;
    free(param_types);
    scope_exit(&ctx->scope);
    return !ctx->has_error;
}
