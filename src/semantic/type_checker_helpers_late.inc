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

static Type *
type_check_function_symbol_call(ASTNode *expr, Symbol *sym,
                                const char *display_name,
                                SemanticContext *ctx)
{
    if (sym == NULL) {
        if (name_looks_qualified(display_name)) {
            semantic_error(ctx, expr,
                "Undefined function '%s' (expected a callable symbol). "
                "Check namespace spelling/export visibility or define/import a callable declaration before use.",
                display_name);
        } else {
            semantic_error(ctx, expr,
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
                semantic_error(ctx, expr,
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
                    semantic_error(ctx, expr,
                        "Constructor '%s' accepts at most %zu positional field argument(s), got %zu",
                        display_name, field_count, provided);
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
                            semantic_error(ctx, expr->data.call.arguments[i],
                                "Constructor '%s' argument %zu initializes field '%s' of type '%s', got '%s'",
                                display_name, i + 1,
                                field_name != NULL ? field_name : "<field>",
                                field_type->name != NULL ? field_type->name : "<type>",
                                arg_type->name != NULL ? arg_type->name : "<type>");
                        }
                        if (expr->data.call.arguments[i] != NULL) {
                            const char *borrowed_name =
                                semantic_borrowed_boundary_root_name(
                                    expr->data.call.arguments[i], ctx);
                            char *source_path = borrowed_name != NULL
                                ? semantic_assignment_target_path(
                                    expr->data.call.arguments[i])
                                : NULL;
                            const char *constructor_name =
                                display_name != NULL ? display_name : "<constructor>";
                            const char *constructor_field =
                                field_name != NULL ? field_name : "<field>";
                            const char *consumer_name =
                                ctx->current_function_decl != NULL
                                && ctx->current_function_decl->data.func_decl.name != NULL
                                    ? ctx->current_function_decl->data.func_decl.name
                                    : "<anonymous>";

                            if (borrowed_name != NULL
                                && type_is_movable_resource_handle(field_type)) {
                                semantic_error(ctx, expr->data.call.arguments[i],
                                    "Borrowed ref movable resource '%s' cannot escape through constructor field store '%s.%s' from '%s'.\n"
                                    "Reason:\n"
                                    "- consumer path is function '%s'\n"
                                    "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                                    "- '%s' is derived from that borrowed movable-resource provenance\n"
                                    "- constructor '%s' would store it into field '%s'\n"
                                    "- constructor field storage is treated as a boundary-visible escaping store\n"
                                    "Fix:\n"
                                    "- keep using '%s' directly within this function\n"
                                    "- or construct the field from a copied/value/projection result\n"
                                    "- or change the parameter to 'own' if transfer is intended",
                                    borrowed_name,
                                    constructor_name,
                                    constructor_field,
                                    source_path != NULL ? source_path : borrowed_name,
                                    consumer_name,
                                    borrowed_name,
                                    source_path != NULL ? source_path : borrowed_name,
                                    constructor_name,
                                    constructor_field,
                                    source_path != NULL ? source_path : borrowed_name);
                            } else if (borrowed_name != NULL
                                       && type_is_subject_type(field_type, ctx)) {
                                semantic_error(ctx, expr->data.call.arguments[i],
                                    "Borrowed ref subject '%s' cannot escape through constructor field store '%s.%s' from '%s'.\n"
                                    "Reason:\n"
                                    "- consumer path is function '%s'\n"
                                    "- '%s' entered this function as a borrowed 'ref' subject\n"
                                    "- '%s' is derived from that borrowed subject provenance\n"
                                    "- constructor '%s' would store it into field '%s'\n"
                                    "- constructor field storage would create a second boundary-visible binding for the borrowed identity\n"
                                    "Fix:\n"
                                    "- keep mutating '%s' through its original binding\n"
                                    "- or construct the field from a projection/object/tobject/value result\n"
                                    "- or change the parameter to 'own' if transfer is intended",
                                    borrowed_name,
                                    constructor_name,
                                    constructor_field,
                                    source_path != NULL ? source_path : borrowed_name,
                                    consumer_name,
                                    borrowed_name,
                                    source_path != NULL ? source_path : borrowed_name,
                                    constructor_name,
                                    constructor_field,
                                    source_path != NULL ? source_path : borrowed_name);
                            } else if (borrowed_name != NULL
                                       && type_requires_boundary_borrow_tracking(field_type, ctx)) {
                                semantic_error(ctx, expr->data.call.arguments[i],
                                    "Borrowed ref boundary value '%s' cannot escape through constructor field store '%s.%s' from '%s'.\n"
                                    "Reason:\n"
                                    "- consumer path is function '%s'\n"
                                    "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                                    "- '%s' is derived from that borrowed boundary provenance\n"
                                    "- constructor '%s' would store it into field '%s'\n"
                                    "- constructor field storage is treated as a boundary-visible escaping store for borrowed provenance\n"
                                    "Fix:\n"
                                    "- keep using '%s' directly within this function\n"
                                    "- or construct the field from a copied/value/projection snapshot\n"
                                    "- or change the parameter to 'own' if transfer is intended",
                                    borrowed_name,
                                    constructor_name,
                                    constructor_field,
                                    source_path != NULL ? source_path : borrowed_name,
                                    consumer_name,
                                    borrowed_name,
                                    source_path != NULL ? source_path : borrowed_name,
                                    constructor_name,
                                    constructor_field,
                                    source_path != NULL ? source_path : borrowed_name);
                            }
                            free(source_path);
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
                                        semantic_error(ctx, expr->data.call.arguments[i],
                                            "World constructor '%s' implicitly copies zone binding '%s' into slot '%s'.\n"
                                            "Reason:\n"
                                            "- origin binding is '%s'\n"
                                            "- contract source is world '%s' zone slot '%s'\n"
                                            "- embedding handoff edge is '%s' -> world '%s' slot '%s'\n"
                                            "- ownership/authority after construction belongs to the world-owned slot, not the origin binding\n"
                                            "- world embedding currently snapshots the zone binding into a world-owned zone slot\n"
                                            "- mutating the original binding afterwards would diverge from the embedded world state\n"
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
        semantic_error(ctx, expr,
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
            semantic_error(ctx, expr,
                "Callable '%s' is not accessible across the current visibility boundary",
                display_name);
            return sym->type->data.function.return_type;
        }
    }
    sym->is_used = true;
    semantic_record_effect(ctx, type_function_effects(sym->type));
    if (ctx->in_parallel
        && type_effect_mask_has(type_function_effects(sym->type), EFFECT_SECURE)) {
        semantic_error(ctx, expr,
            "Parallel context does not permit calling secure-effect function '%s'; serialize capability-bearing operations outside the parallel block",
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
        semantic_error(ctx, expr,
            "'%s' expects %zu argument(s), got %zu. Declared signature: %s. "
            "Adjust caller arguments to satisfy the declaration.",
            display_name, expected, provided, sig_buf);
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
        Type *arg_type = type_is_movable_resource_handle(param_type)
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

        if (type_is_movable_resource_handle(arg_type)
            || type_is_movable_resource_handle(param_type)) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (!type_is_movable_resource_handle(arg_type)
                || !type_is_movable_resource_handle(param_type)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Movable resource argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            if (expr->data.call.arguments[i]->type != AST_IDENTIFIER) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "%s arguments must be moved from a named variable; bind the value first, then pass that variable",
                    resource_handle_display_name(param_type));
                continue;
            }
            if (ctx->program_root != NULL) {
                ASTNode *prog = ctx->program_root;
                for (size_t si = 0; si < prog->data.program.count; si++) {
                    ASTNode *stmt = prog->data.program.statements[si];
                    if (stmt != NULL && stmt->type == AST_FUNC_DECL
                        && stmt->data.func_decl.name != NULL
                        && strcmp(stmt->data.func_decl.name, display_name) == 0
                        && i < stmt->data.func_decl.param_count) {
                        callee_decl = stmt;
                        pmode = stmt->data.func_decl.params[i]->mode;
                        break;
                    }
                }
            }
            if (pmode == PARAM_MODE_REF
                && expr->data.call.arguments[i]->type == AST_IDENTIFIER
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)
                && callee_decl != NULL
                && callee_decl->data.func_decl.body != NULL) {
                unsigned callee_mask = slot_analyze_param_summary_in_program(
                    callee_decl->data.func_decl.body,
                    callee_decl->data.func_decl.params[i] != NULL
                        ? callee_decl->data.func_decl.params[i]->name
                        : NULL,
                    ctx->program_root);
                if ((callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
                    char *source_path = semantic_assignment_target_path(
                        expr->data.call.arguments[i]);
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "Borrowed ref movable resource '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                        "- '%s' is derived from that borrowed movable-resource provenance\n"
                        "- callee '%s' derives a transitive call-escape path for its borrowed parameter\n"
                        "- forwarding this borrow into that helper would let it escape beyond the current call boundary\n"
                        "Fix:\n"
                        "- perform the movable-resource operation locally in this function\n"
                        "- or change the current parameter to 'own' if transfer/forwarding is intended",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>",
                        source_path != NULL
                            ? source_path
                            : expr->data.call.arguments[i]->data.identifier.name,
                        ctx->current_function_decl != NULL
                            && ctx->current_function_decl->data.func_decl.name != NULL
                                ? ctx->current_function_decl->data.func_decl.name
                                : "<anonymous>",
                        expr->data.call.arguments[i]->data.identifier.name,
                        source_path != NULL
                            ? source_path
                            : expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>");
                    free(source_path);
                    continue;
                }
            }
            if (pmode != PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)) {
                char *source_path = semantic_assignment_target_path(
                    expr->data.call.arguments[i]);
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Borrowed ref movable resource '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
                    "Reason:\n"
                    "- consumer path is function '%s'\n"
                    "- '%s' entered this function as a borrowed 'ref' movable resource\n"
                    "- '%s' is derived from that borrowed movable-resource provenance\n"
                    "- forwarding it to '%s' as '%s' would create a transitive transfer beyond the current call boundary\n"
                    "Fix:\n"
                    "- call a 'ref' helper instead\n"
                    "- or change the current parameter to 'own' if forwarding/transfer is intended",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    source_path != NULL
                        ? source_path
                        : expr->data.call.arguments[i]->data.identifier.name,
                    ctx->current_function_decl != NULL
                        && ctx->current_function_decl->data.func_decl.name != NULL
                            ? ctx->current_function_decl->data.func_decl.name
                            : "<anonymous>",
                    expr->data.call.arguments[i]->data.identifier.name,
                    source_path != NULL
                        ? source_path
                        : expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    pmode == PARAM_MODE_OWN ? "own" : "default");
                free(source_path);
                continue;
            }
            if (pmode == PARAM_MODE_REF)
                continue;
            consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            continue;
        }

        if (type_is_subject_type(param_type, ctx)) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (!type_is_subject_type(arg_type, ctx)) {
                semantic_error(ctx, expr->data.call.arguments[i],
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
            if (ctx->program_root != NULL) {
                ASTNode *prog = ctx->program_root;
                for (size_t si = 0; si < prog->data.program.count; si++) {
                    ASTNode *stmt = prog->data.program.statements[si];
                    if (stmt != NULL && stmt->type == AST_FUNC_DECL
                        && stmt->data.func_decl.name != NULL
                        && strcmp(stmt->data.func_decl.name, display_name) == 0
                        && i < stmt->data.func_decl.param_count) {
                        callee_decl = stmt;
                        pmode = stmt->data.func_decl.params[i]->mode;
                        break;
                    }
                }
            }
            if (expr->data.call.arguments[i]->type != AST_IDENTIFIER) {
                if (pmode == PARAM_MODE_DEFAULT)
                    continue;
                {
                    const char *borrowed_root_name =
                        semantic_borrowed_boundary_root_name(
                            expr->data.call.arguments[i], ctx);
                    char *source_path = semantic_assignment_target_path(
                        expr->data.call.arguments[i]);
                    char *borrowed_line = borrowed_root_name != NULL
                        ? tc_strdup_fmt(
                            "- '%s' is derived from borrowed source '%s'\n",
                            source_path != NULL ? source_path : "<subject>",
                            borrowed_root_name)
                        : NULL;
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "Subject boundary arguments must use a named variable%s%s%s.\n"
                        "Reason:\n"
                        "- own/ref subject boundaries need one stable source binding for moved-here / borrowed-here provenance\n"
                        "%s"
                        "- unnamed subject expressions would make later ownership diagnostics ambiguous\n"
                        "Fix:\n"
                        "- bind the subject to a local variable first\n"
                        "- then pass that named variable through the boundary",
                        source_path != NULL ? " instead of '" : "",
                        source_path != NULL ? source_path : "",
                        source_path != NULL ? "'" : "",
                        borrowed_line != NULL ? borrowed_line : "");
                    free(borrowed_line);
                    free(source_path);
                }
                continue;
            }
            if (pmode == PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)
                && callee_decl != NULL
                && callee_decl->data.func_decl.body != NULL) {
                unsigned callee_mask = slot_analyze_param_summary_in_program(
                    callee_decl->data.func_decl.body,
                    callee_decl->data.func_decl.params[i] != NULL
                        ? callee_decl->data.func_decl.params[i]->name
                        : NULL,
                    ctx->program_root);
                if ((callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
                    char *source_path = semantic_assignment_target_path(
                        expr->data.call.arguments[i]);
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "Borrowed ref subject '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' subject\n"
                        "- '%s' is derived from that borrowed subject provenance\n"
                        "- callee '%s' derives a transitive call-escape path for its borrowed parameter\n"
                        "- forwarding this borrow into that helper would let it escape beyond the current call boundary\n"
                        "Fix:\n"
                        "- perform the subject operation locally in this function\n"
                        "- or change the current parameter to 'own' if transfer/forwarding is intended",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>",
                        source_path != NULL
                            ? source_path
                            : expr->data.call.arguments[i]->data.identifier.name,
                        ctx->current_function_decl != NULL
                            && ctx->current_function_decl->data.func_decl.name != NULL
                                ? ctx->current_function_decl->data.func_decl.name
                                : "<anonymous>",
                        expr->data.call.arguments[i]->data.identifier.name,
                        source_path != NULL
                            ? source_path
                            : expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>");
                    free(source_path);
                    continue;
                }
            }
            if (pmode != PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)) {
                char *source_path = semantic_assignment_target_path(
                    expr->data.call.arguments[i]);
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Borrowed ref subject '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
                    "Reason:\n"
                    "- consumer path is function '%s'\n"
                    "- '%s' entered this function as a borrowed 'ref' subject\n"
                    "- '%s' is derived from that borrowed subject provenance\n"
                    "- forwarding it to '%s' as '%s' would create a transitive transfer beyond the current call boundary\n"
                    "Fix:\n"
                    "- call a 'ref' helper instead\n"
                    "- or change the current parameter to 'own' if forwarding/transfer is intended",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    source_path != NULL
                        ? source_path
                        : expr->data.call.arguments[i]->data.identifier.name,
                    ctx->current_function_decl != NULL
                        && ctx->current_function_decl->data.func_decl.name != NULL
                            ? ctx->current_function_decl->data.func_decl.name
                            : "<anonymous>",
                    expr->data.call.arguments[i]->data.identifier.name,
                    source_path != NULL
                        ? source_path
                        : expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    pmode == PARAM_MODE_OWN ? "own" : "default");
                free(source_path);
                continue;
            }
            if (pmode == PARAM_MODE_OWN) {
                consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            }
            continue;
        }

        if (type_is_general_boundary_type(param_type, ctx)
            && !type_is_subject_type(param_type, ctx)
            && !type_is_movable_resource_handle(param_type)) {
            ParamMode pmode = PARAM_MODE_DEFAULT;
            ASTNode *callee_decl = NULL;
            if (!type_is_general_boundary_type(arg_type, ctx)
                || type_is_subject_type(arg_type, ctx)
                || type_is_movable_resource_handle(arg_type)
                || !type_is_assignable(arg_type, param_type)
                || !type_is_assignable(param_type, arg_type)) {
                semantic_error(ctx, expr->data.call.arguments[i],
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
            if (ctx->program_root != NULL) {
                ASTNode *prog = ctx->program_root;
                for (size_t si = 0; si < prog->data.program.count; si++) {
                    ASTNode *stmt = prog->data.program.statements[si];
                    if (stmt != NULL && stmt->type == AST_FUNC_DECL
                        && stmt->data.func_decl.name != NULL
                        && strcmp(stmt->data.func_decl.name, display_name) == 0
                        && i < stmt->data.func_decl.param_count) {
                        callee_decl = stmt;
                        pmode = stmt->data.func_decl.params[i]->mode;
                        break;
                    }
                }
            }
            if (expr->data.call.arguments[i]->type != AST_IDENTIFIER) {
                if (pmode == PARAM_MODE_DEFAULT)
                    continue;
                {
                    const char *borrowed_root_name =
                        semantic_borrowed_boundary_root_name(
                            expr->data.call.arguments[i], ctx);
                    char *source_path = semantic_assignment_target_path(
                        expr->data.call.arguments[i]);
                    char *borrowed_line = borrowed_root_name != NULL
                        ? tc_strdup_fmt(
                            "- '%s' is derived from borrowed source '%s'\n",
                            source_path != NULL ? source_path : "<value>",
                            borrowed_root_name)
                        : NULL;
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Value boundary arguments must use a named variable%s%s%s.\n"
                    "Reason:\n"
                    "- own/ref value boundaries need one stable source binding for moved-here / borrowed-here provenance\n"
                    "%s"
                    "- unnamed expressions would make later ownership diagnostics ambiguous\n"
                    "Fix:\n"
                    "- bind the value to a local variable first\n"
                    "- then pass that named variable through the boundary",
                    source_path != NULL ? " instead of '" : "",
                    source_path != NULL ? source_path : "",
                    source_path != NULL ? "'" : "",
                    borrowed_line != NULL ? borrowed_line : "");
                    free(borrowed_line);
                    free(source_path);
                }
                continue;
            }
            if (pmode == PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)
                && type_requires_boundary_borrow_tracking(param_type, ctx)
                && callee_decl != NULL
                && callee_decl->data.func_decl.body != NULL) {
                unsigned callee_mask = slot_analyze_param_summary_in_program(
                    callee_decl->data.func_decl.body,
                    callee_decl->data.func_decl.params[i] != NULL
                        ? callee_decl->data.func_decl.params[i]->name
                        : NULL,
                    ctx->program_root);
                if ((callee_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                                    | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
                    char *source_path = semantic_assignment_target_path(
                        expr->data.call.arguments[i]);
                    semantic_error(ctx, expr->data.call.arguments[i],
                        "Borrowed ref boundary value '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
                        "Reason:\n"
                        "- consumer path is function '%s'\n"
                        "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                        "- '%s' is derived from that borrowed boundary provenance\n"
                        "- callee '%s' derives a transitive call-escape path for its borrowed parameter\n"
                        "- forwarding this borrow into that helper would let it escape beyond the current call boundary\n"
                        "Fix:\n"
                        "- perform the value operation locally in this function\n"
                        "- or change the current parameter to 'own' if transfer/forwarding is intended",
                        expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>",
                        source_path != NULL
                            ? source_path
                            : expr->data.call.arguments[i]->data.identifier.name,
                        ctx->current_function_decl != NULL
                            && ctx->current_function_decl->data.func_decl.name != NULL
                                ? ctx->current_function_decl->data.func_decl.name
                                : "<anonymous>",
                        expr->data.call.arguments[i]->data.identifier.name,
                        source_path != NULL
                            ? source_path
                            : expr->data.call.arguments[i]->data.identifier.name,
                        display_name != NULL ? display_name : "<callee>");
                    free(source_path);
                    continue;
                }
            }
            if (pmode != PARAM_MODE_REF
                && identifier_is_borrowed_boundary_param(expr->data.call.arguments[i], ctx)) {
                if (!type_requires_boundary_borrow_tracking(param_type, ctx))
                    continue;
                char *source_path = semantic_assignment_target_path(
                    expr->data.call.arguments[i]);
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Borrowed ref boundary value '%s' cannot escape through helper/function call to '%s' from '%s'.\n"
                    "Reason:\n"
                    "- consumer path is function '%s'\n"
                    "- '%s' entered this function as a borrowed 'ref' boundary value\n"
                    "- '%s' is derived from that borrowed boundary provenance\n"
                    "- forwarding it to '%s' as '%s' would create a transitive transfer beyond the current call boundary\n"
                    "Fix:\n"
                    "- call a 'ref' helper instead\n"
                    "- or change the current parameter to 'own' if forwarding/transfer is intended",
                    expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    source_path != NULL
                        ? source_path
                        : expr->data.call.arguments[i]->data.identifier.name,
                    ctx->current_function_decl != NULL
                        && ctx->current_function_decl->data.func_decl.name != NULL
                            ? ctx->current_function_decl->data.func_decl.name
                            : "<anonymous>",
                    expr->data.call.arguments[i]->data.identifier.name,
                    source_path != NULL
                        ? source_path
                        : expr->data.call.arguments[i]->data.identifier.name,
                    display_name != NULL ? display_name : "<callee>",
                    pmode == PARAM_MODE_OWN ? "own" : "default");
                free(source_path);
                continue;
            }
            if (pmode == PARAM_MODE_OWN) {
                consume_qubit_value(expr->data.call.arguments[i], ctx, "moved");
            }
            continue;
        }

        if (type_is_anchored_resource_handle(arg_type)
            || type_is_anchored_resource_handle(param_type)) {
            if (!type_is_anchored_resource_handle(arg_type)
                || !type_is_anchored_resource_handle(param_type)) {
                semantic_error(ctx, expr->data.call.arguments[i],
                    "Resource handle argument type mismatch: expected '%s', got '%s'",
                    resource_handle_display_name(param_type),
                    resource_handle_display_name(arg_type));
                continue;
            }
            /* Check own/ref qualifier from function declaration AST */
            {
                ParamMode pmode = PARAM_MODE_DEFAULT;
                if (ctx->program_root != NULL) {
                    ASTNode *prog = ctx->program_root;
                    for (size_t si = 0; si < prog->data.program.count; si++) {
                        ASTNode *stmt = prog->data.program.statements[si];
                        if (stmt != NULL && stmt->type == AST_FUNC_DECL
                            && stmt->data.func_decl.name != NULL
                            && strcmp(stmt->data.func_decl.name, display_name) == 0
                            && i < stmt->data.func_decl.param_count) {
                            pmode = stmt->data.func_decl.params[i]->mode;
                            break;
                        }
                    }
                }
                if (pmode == PARAM_MODE_OWN) {
                    /* own: move — consume the source slot */
                    if (expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                        const char *src = expr->data.call.arguments[i]->data.identifier.name;
                        Symbol *src_sym = scope_lookup(ctx->scope, src);
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
                            if (src_sym->slot_info.state == SLOT_STATE_RELEASED)
                                semantic_error_code(ctx, "PGY_SEM_MOVE_FROM_RELEASED",
                                    expr->data.call.arguments[i],
                                    "Cannot move from released slot '%s'", src);
                            else
                                src_sym->slot_info.state = SLOT_STATE_RELEASED;
                        }
                    }
                    continue;
                } else if (pmode == PARAM_MODE_REF) {
                    /* ref: borrow — source remains valid */
                    if (expr->data.call.arguments[i]->type == AST_IDENTIFIER) {
                        const char *src = expr->data.call.arguments[i]->data.identifier.name;
                        Symbol *src_sym = scope_lookup(ctx->scope, src);
                        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT
                            && src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                            semantic_error(ctx, expr->data.call.arguments[i],
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
                    continue;
                }
            }
            semantic_error(ctx, expr->data.call.arguments[i],
                "Slot arguments require 'own' or 'ref' qualifier in function signature; "
                "use 'func F(own s: Slot<T>)' to take ownership or "
                "'func F(ref s: Slot<T>)' to borrow");
            continue;
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
                        semantic_error(ctx, expr,
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
                        semantic_error(ctx, expr,
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
                            semantic_error(ctx, expr,
                                "Type '%s' does not satisfy constraint '%s' for generic parameter '%s'.\n"
                                "Reason:\n"
                                "- generic subject is function '%s'\n"
                                "- function '%s' requires '%s: %s'\n"
                                "- full bound set is '%s: %s'\n"
                                "- expected type args are '%s'\n"
                                "- actual type args are '%s'\n"
                                "- actual type argument is '%s'\n"
                                "- consumer path is call site of '%s'\n"
                                "- call-site derived type arguments do not satisfy bound '%s'\n"
                                "- expected type args are constrained by '%s: %s'\n"
                                "Fix:\n"
                                "- pass an argument whose type satisfies '%s'\n"
                                "- or relax the function where-clause",
                                concrete_type->name != NULL ? concrete_type->name : "<type>",
                                bound_name != NULL ? bound_name : "<constraint>",
                                param_name,
                                display_name,
                                display_name,
                                param_name,
                                bound_name != NULL ? bound_name : "<constraint>",
                                param_name,
                                bounds_text != NULL ? bounds_text : "<constraint>",
                                expected_sig != NULL ? expected_sig : display_name,
                                actual_sig != NULL ? actual_sig : display_name,
                                concrete_type->name != NULL ? concrete_type->name : "<type>",
                                display_name,
                                bound_name != NULL ? bound_name : "<constraint>",
                                param_name,
                                bound_name != NULL ? bound_name : "<constraint>",
                                bound_name != NULL ? bound_name : "<constraint>");
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
