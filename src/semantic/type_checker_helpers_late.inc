flatten_static_member_access(const ASTNode *expr, char separator)
{
    if (expr == NULL)
        return NULL;

    if (expr->type == AST_IDENTIFIER)
        return expr->data.identifier.name != NULL
            ? pergyra_strdup(expr->data.identifier.name) : NULL;

    if (expr->type != AST_MEMBER_ACCESS
        || expr->data.member.object == NULL
        || expr->data.member.name == NULL) {
        return NULL;
    }

    char *lhs = flatten_static_member_access(expr->data.member.object, separator);
    if (lhs == NULL)
        return NULL;

    size_t lhs_len = strlen(lhs);
    size_t rhs_len = strlen(expr->data.member.name);
    char *result = malloc(lhs_len + rhs_len + 2);
    if (result == NULL) {
        free(lhs);
        return NULL;
    }

    memcpy(result, lhs, lhs_len);
    result[lhs_len] = separator;
    memcpy(result + lhs_len + 1, expr->data.member.name, rhs_len);
    result[lhs_len + rhs_len + 1] = '\0';
    free(lhs);
    return result;
}

static uint32_t
declared_effects_from_function_node(ASTNode *node, SemanticContext *ctx,
                                    bool *has_contract_out);

void
semantic_report_function_generic_bound_failure(SemanticContext *ctx,
                                               ASTNode *site,
                                               const char *function_name,
                                               const char *param_name,
                                               const char *bound_name,
                                               const char *bounds_text,
                                               const char *expected_sig,
                                               const char *actual_sig,
                                               const char *concrete_name)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID,
        PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, site,
        "Type '%s' does not satisfy constraint '%s' for generic parameter '%s'.\n"
        "Reason:\n"
        "- generic subject is function '%s'\n"
        "- function '%s' requires '%s: %s'\n"
        "- full bound set is '%s: %s'\n"
        "- expected type args are '%s'\n"
        "- actual type args are '%s'\n"
        "- consumer path is call site of '%s'\n"
        "- broken bound is '%s'\n"
        "Fix:\n"
        "- pass an argument whose type satisfies '%s'\n"
        "- or relax the function where-clause",
        concrete_name != NULL ? concrete_name : "<type>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        function_name != NULL ? function_name : "<function>",
        function_name != NULL ? function_name : "<function>",
        param_name != NULL ? param_name : "<type-param>",
        bound_name != NULL ? bound_name : "<constraint>",
        param_name != NULL ? param_name : "<type-param>",
        bounds_text != NULL ? bounds_text : "<constraint>",
        expected_sig != NULL ? expected_sig : "<function>",
        actual_sig != NULL ? actual_sig : "<actual>",
        function_name != NULL ? function_name : "<function>",
        bound_name != NULL ? bound_name : "<constraint>",
        bound_name != NULL ? bound_name : "<constraint>");
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

#include "type_checker_ownership_call.inc"

static Type *
type_check_function_symbol_call(ASTNode *expr, Symbol *sym,
                                const char *display_name,
                                SemanticContext *ctx)
{
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

    if (sym->kind == SYMBOL_CLASS || sym->kind == SYMBOL_PARTY
        || sym->kind == SYMBOL_RELATION
        || sym->kind == SYMBOL_EFFECT || sym->kind == SYMBOL_ROSTER
        || sym->kind == SYMBOL_WORLD || sym->kind == SYMBOL_ZONE) {
        if (ctx->program_root != NULL) {
            ASTNode *decl = constructor_decl_for_symbol_kind(ctx->program_root,
                sym->kind, display_name);
            size_t field_count = 0;
            bool decl_is_generic = false;
            if (decl != NULL
                && !explicit_type_reference_allowed(decl, expr, ctx)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr,
                    "Constructor '%s' is not accessible across the current visibility boundary",
                    display_name);
                return TYPE_UNKNOWN;
            }
            if (decl != NULL && decl->type == AST_CLASS_DECL) {
                field_count = decl->data.class_decl.field_count;
                decl_is_generic =
                    (decl->data.class_decl.generic_params != NULL
                     && decl->data.class_decl.generic_params->count > 0);
            } else if (decl != NULL
                       && (decl->type == AST_RELATION_DECL
                           || decl->type == AST_EFFECT_DECL
                           || decl->type == AST_ROSTER_DECL
                           || decl->type == AST_WORLD_DECL
                           || decl->type == AST_ZONE_DECL)) {
                field_count = overlay_field_count(decl);
            }
            if (decl != NULL) {
                size_t provided = expr->data.call.arg_count;
                /* Skip field-type validation for generic classes — the
                 * generic params (T, U) aren't in scope at the call site.
                 * Type safety is handled by the let-annotation type. */
                if (provided > field_count) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr,
                        "Constructor '%s' accepts at most %llu positional field argument(s), got %llu",
                        display_name, (unsigned long long) field_count, (unsigned long long) provided);
                } else if (!decl_is_generic) {
                    for (size_t i = 0; i < provided; i++) {
                        const char *field_name = NULL;
                        ASTNode *field_type_node = NULL;
                        if (decl->type == AST_CLASS_DECL) {
                            ClassField *field = subject_host_field_at(decl, i);
                            if (field != NULL) {
                                field_name = field->name;
                                field_type_node = field->type;
                            }
                        } else {
                            field_type_node = overlay_field_decl_at(decl, i, &field_name);
                        }
                        if (field_type_node == NULL)
                            continue;
                        Type *field_type = resolve_type_node(field_type_node, ctx);
                        Type *arg_type = type_check_expression(expr->data.call.arguments[i], ctx);
                        if (field_type != NULL && arg_type != NULL
                            && !type_is_assignable(arg_type, field_type)) {
                            semantic_error_with_hints(ctx, PGY_CODE_SEM_CLASS_CONTRACT_INVALID, PGY_CAUSE_CLASS_CONTRACT, PGY_FIX_SATISFY_GENERIC_BOUND_OR_WIDEN, expr->data.call.arguments[i],
                                "Constructor '%s' argument %llu initializes field '%s' of type '%s', got '%s'",
                                display_name, (unsigned long long) (i + 1),
                                field_name != NULL ? field_name : "<field>",
                                field_type->name != NULL ? field_type->name : "<type>",
                                arg_type->name != NULL ? arg_type->name : "<type>");
                        }
                        if (expr->data.call.arguments[i] != NULL) {
                            const char *borrowed_name =
                                semantic_borrowed_boundary_root_name(
                                    expr->data.call.arguments[i], ctx);
                            const char *constructor_name =
                                display_name != NULL ? display_name : "<constructor>";
                            const char *constructor_field =
                                field_name != NULL ? field_name : "<field>";

                            if (borrowed_name != NULL) {
                                semantic_validate_borrowed_escape(
                                    expr->data.call.arguments[i],
                                    expr->data.call.arguments[i],
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
                            && expr->data.call.arguments[i] != NULL
                            && expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                            Symbol *arg_sym = scope_lookup(ctx->scope,
                                expr->data.call.arguments[i]->data.identifier.name);
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
                                for (size_t zi = 0; zi < decl->data.world_decl.zone_count; zi++) {
                                    ASTNode *zone_slot = decl->data.world_decl.zones[zi];
                                    if (zone_slot == NULL
                                        || zone_slot->type != AST_WORLD_ZONE
                                        || zone_slot->data.world_zone.zone_type == NULL) {
                                        continue;
                                    }
                                    if (strcmp(arg_type_name,
                                            zone_slot->data.world_zone.zone_type) == 0) {
                                        semantic_error_with_hints(ctx,
                                            PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
                                            PGY_CAUSE_ANCHORED_HANDLE_COPY_ATTEMPT,
                                            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                                            expr->data.call.arguments[i],
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
                                            expr->data.call.arguments[i]->data.identifier.name != NULL
                                                ? expr->data.call.arguments[i]->data.identifier.name
                                                : "<zone>",
                                            zone_slot->data.world_zone.slot_name != NULL
                                                ? zone_slot->data.world_zone.slot_name
                                                : "<slot>",
                                            expr->data.call.arguments[i]->data.identifier.name != NULL
                                                ? expr->data.call.arguments[i]->data.identifier.name
                                                : "<zone>",
                                            display_name != NULL ? display_name : "<world>",
                                            zone_slot->data.world_zone.slot_name != NULL
                                                ? zone_slot->data.world_zone.slot_name
                                                : "<slot>",
                                            expr->data.call.arguments[i]->data.identifier.name != NULL
                                                ? expr->data.call.arguments[i]->data.identifier.name
                                                : "<zone>",
                                            display_name != NULL ? display_name : "<world>",
                                            zone_slot->data.world_zone.slot_name != NULL
                                                ? zone_slot->data.world_zone.slot_name
                                                : "<slot>",
                                            expr->data.call.arguments[i]->data.identifier.name != NULL
                                                ? expr->data.call.arguments[i]->data.identifier.name
                                                : "<zone>");
                                        arg_sym->embedded_in_world = true;
                                        semantic_ctx_mark_embedded_world_zone_name(ctx,
                                            expr->data.call.arguments[i]->data.identifier.name,
                                            display_name,
                                            zone_slot->data.world_zone.slot_name);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    /* For generic classes, still type-check expressions */
                    for (size_t i = 0; i < provided; i++)
                        type_check_expression(expr->data.call.arguments[i], ctx);
                }
            }
        }
        sym->is_used = true;
        return sym->type;
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
        ASTNode *decl = find_callable_decl_by_name(ctx->program_root, display_name);
        if (decl != NULL && !explicit_type_reference_allowed(decl, expr, ctx)) {
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
    if (ctx->in_parallel
        && type_effect_mask_has(type_function_effects(sym->type), EFFECT_SECURE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN, PGY_CAUSE_PARALLEL_SECURE_IN_TASK, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, expr,
            "Parallel context does not permit calling secure-effect function '%s'; serialize authority-bearing operations outside the parallel block",
            display_name);
        return sym->type->data.function.return_type;
    }

    size_t expected = sym->type->data.function.param_count;
    size_t provided = expr->data.call.arg_count;
    ASTNode *callable_decl = NULL;
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
                            resolve_type_node(gp->default_type, ctx);
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
                if (borrowed_name == NULL)
                    goto type_check_call_movable_done;
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
type_check_call_movable_done:
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
                        effective_generic_types[gi] = resolve_type_node(gp->default_type, ctx);
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
                    char *actual_sig = format_effective_generic_type_list(
                        display_name, effective_generic_types, decl_gp->count);

                    if (tc == NULL || tc->type_param == NULL)
                    {
                        free(actual_sig);
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
                        free(actual_sig);
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
                        free(actual_sig);
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
                    free(actual_sig);
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

static Symbol *
lookup_identifier_symbol(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL || expr->type != AST_IDENTIFIER
        || expr->data.identifier.name == NULL) {
        return NULL;
    }
    return scope_lookup(ctx->scope, expr->data.identifier.name);
}

bool
consume_qubit_value(ASTNode *expr, SemanticContext *ctx, const char *action)
{
    Symbol *sym = lookup_identifier_symbol(expr, ctx);
    if (sym == NULL
        || !type_is_general_boundary_type(sym->type, ctx))
        return false;

    if (sym->is_consumed) {
        return false;
    }

    sym->is_consumed = true;
    sym->is_used = true;
    (void)action;
    return true;
}
