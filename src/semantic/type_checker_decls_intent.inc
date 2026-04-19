type_check_intent_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.intent_decl.name;
    Symbol *existing = scope_lookup_current(ctx->scope, name);
    if (existing != NULL && existing->kind != SYMBOL_INTENT) {
        semantic_error(ctx, node, "Redeclaration of intent '%s'", name);
        return false;
    }
    if (existing != NULL && existing->kind == SYMBOL_INTENT) {
        size_t ipc = node->data.intent_decl.binding_count > 0
            ? node->data.intent_decl.binding_count
            : (node->data.intent_decl.involve_count
                + node->data.intent_decl.value_count);
        Type **ptypes = calloc(ipc > 0 ? ipc : 1, sizeof(Type *));
        Type *ft;
        for (size_t i = 0; i < ipc; i++) {
            ASTNode *binding = node->data.intent_decl.binding_count > 0
                ? node->data.intent_decl.bindings[i]
                : (i < node->data.intent_decl.involve_count
                    ? node->data.intent_decl.involves[i]
                    : node->data.intent_decl.values[i - node->data.intent_decl.involve_count]);
            if (binding != NULL && binding->type == AST_INTENT_INVOLVES
                && binding->data.intent_involves.subject_type != NULL) {
                ptypes[i] = resolve_type_node(
                    binding->data.intent_involves.subject_type, ctx);
            } else if (binding != NULL && binding->type == AST_INTENT_VALUE
                && binding->data.intent_value.value_type != NULL) {
                ptypes[i] = resolve_type_node(
                    binding->data.intent_value.value_type, ctx);
            } else {
                ptypes[i] = TYPE_UNKNOWN;
            }
        }
        ft = type_create_function(ptypes, ipc, TYPE_BOOL);
        free(ptypes);
        if (ft != NULL) {
            existing->type = ft;
        }
    }

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        (void)resolve_type_node(involves->data.intent_involves.subject_type, ctx);
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;
        (void)resolve_type_node(value->data.intent_value.value_type, ctx);
    }

    scope_enter(&ctx->scope, SCOPE_BLOCK);
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        Type *subject_type;
        Symbol *participant_sym;

        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;

        subject_type = resolve_type_node(involves->data.intent_involves.subject_type, ctx);
        participant_sym = symbol_create_variable(involves->data.intent_involves.alias,
            subject_type, involves->line, involves->column);
        scope_declare(ctx->scope, participant_sym);
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        Type *value_type;
        Symbol *value_sym;

        if (value == NULL || value->type != AST_INTENT_VALUE)
            continue;

        value_type = resolve_type_node(value->data.intent_value.value_type, ctx);
        value_sym = symbol_create_variable(value->data.intent_value.alias,
            value_type, value->line, value->column);
        scope_declare(ctx->scope, value_sym);
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        bool matched_action = false;
        ASTNode *zone_decl = NULL;
        bool has_subintent = false;
        bool step_requires_authority_flow = false;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;

        has_subintent = (step->data.intent_step.intent_expr != NULL);
        intent_step_derive_who_from_action(node, step, ctx);
        intent_step_inherit_action_contract(node, step, ctx);
        intent_step_derive_transfer_context(node, step, ctx);
        intent_step_derive_zone_binding_context(node, step, ctx);
        intent_step_warn_redundant_action_contract(node, step, ctx);

        if (step->data.intent_step.where_type == NULL && !has_subintent) {
            char contract_summary[512];
            intent_step_format_contract_source_summary(
                node, step, ctx, contract_summary, sizeof(contract_summary));
            semantic_error(ctx, step,
                "Intent step '%s' requires a where zone type.\n"
                "Reason:\n"
                "- no local, reused, or derived zone contract is available for this step\n"
                "%s%s"
                "Fix:\n"
                "- add 'where: <Zone>;' to the step\n"
                "- or add 'within <Zone>' to the matching action contract\n"
                "- or add a transfer/using binding that can derive the zone",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                contract_summary[0] != '\0' ? contract_summary : "",
                contract_summary[0] != '\0' ? "\n" : "");
        } else {
            Type *zone_type = resolve_type_node(step->data.intent_step.where_type, ctx);
            zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                zone_type != NULL ? zone_type->name : NULL);
            if (step->data.intent_step.where_type != NULL && zone_decl == NULL) {
                semantic_error(ctx, step->data.intent_step.where_type,
                    "Intent step '%s' refers to unknown zone '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    zone_type != NULL ? zone_type->name : "<zone>");
            }
        }

        if (step->data.intent_step.using_expr != NULL) {
            Type *using_type = type_check_expression(step->data.intent_step.using_expr, ctx);
            Type *zone_type = step->data.intent_step.where_type != NULL
                ? resolve_type_node(step->data.intent_step.where_type, ctx) : NULL;
            intent_clause_rejects_control_transfer(step->data.intent_step.using_expr, ctx,
                step->data.intent_step.name, "using");
            if (step->data.intent_step.using_expr->type != AST_IDENTIFIER) {
                semantic_error(ctx, step->data.intent_step.using_expr,
                    "Intent step '%s' using clause must reference an intent parameter alias",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            } else if (find_intent_involves_local(node,
                    step->data.intent_step.using_expr->data.identifier.name) == NULL) {
                semantic_error(ctx, step->data.intent_step.using_expr,
                    "Intent step '%s' using clause refers to unknown binding '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>");
            }
            if (using_type != NULL && zone_type != NULL && !type_equals(using_type, zone_type)) {
                semantic_error(ctx, step->data.intent_step.using_expr,
                    "Intent step '%s' using binding must match zone type '%s', got '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    zone_type->name != NULL ? zone_type->name : "<zone>",
                    using_type->name != NULL ? using_type->name : "<type>");
            }
        }

        if (step->data.intent_step.transfer_from_alias != NULL
            || step->data.intent_step.transfer_to_alias != NULL) {
            const char *from_alias = step->data.intent_step.transfer_from_alias;
            const char *to_alias = NULL;
            ASTNode *from_involves = find_intent_involves_local(node, from_alias);
            ASTNode *to_involves = intent_step_resolve_transfer_target_involves(
                node, step, &to_alias);
            Type *from_type = from_involves != NULL
                ? resolve_type_node(from_involves->data.intent_involves.subject_type, ctx) : NULL;
            Type *to_type = to_involves != NULL
                ? resolve_type_node(to_involves->data.intent_involves.subject_type, ctx) : NULL;
            ASTNode *from_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                from_type != NULL ? from_type->name : NULL);
            ASTNode *to_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                to_type != NULL ? to_type->name : NULL);
            Type *where_zone_type = step->data.intent_step.where_type != NULL
                ? resolve_type_node(step->data.intent_step.where_type, ctx) : NULL;

            if (from_alias == NULL || to_alias == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer clause requires both source and target zone aliases.\n"
                    "Reason:\n"
                    "- transfer declares a zone handoff contract\n"
                    "- contract source is the step-local transfer clause itself\n"
                    "- handoff materialization cannot proceed without both source and target bindings\n"
                    "Fix:\n"
                    "- provide 'transfer: <sourceZoneAlias> -> <targetZoneAlias>;'\n"
                    "- or remove the incomplete transfer clause",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            }
            if (from_alias != NULL && from_involves == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer source '%s' is not a declared intent binding.\n"
                    "Reason:\n"
                    "- contract source is transfer handoff edge '%s' -> '%s'\n"
                    "- the handoff source must name an intent binding that participates in this orchestration\n"
                    "- no declared binding named '%s' was found on the current intent\n"
                    "Fix:\n"
                    "- use a declared zone binding as the transfer source\n"
                    "- or add '%s' as an intent binding before using it in transfer",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_alias,
                    from_alias,
                    to_alias != NULL ? to_alias : "<target>",
                    from_alias,
                    from_alias);
            }
            if (to_alias != NULL && to_involves == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer target '%s' is not a declared intent binding or a unique zone type in this intent.\n"
                    "Reason:\n"
                    "- contract source is transfer handoff edge '%s' -> '%s'\n"
                    "- transfer target resolution first looks for an explicit binding alias\n"
                    "- then it allows a unique zone-type shorthand only when that target is unambiguous\n"
                    "- current intent provides no unique target for '%s'\n"
                    "Fix:\n"
                    "- use a declared zone binding alias as the transfer target\n"
                    "- or make the target zone type unique within this intent",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias,
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias,
                    to_alias);
            }
            if (from_involves != NULL && from_zone_decl == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer source '%s' must be a zone binding.\n"
                    "Reason:\n"
                    "- contract source is transfer handoff edge '%s' -> '%s'\n"
                    "- transfer performs zone-to-zone handoff\n"
                    "- source binding '%s' does not resolve to a zone declaration\n"
                    "Fix:\n"
                    "- use a zone binding as the transfer source\n"
                    "- or move ordinary participant movement into 'who'/'on' instead of 'transfer'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_alias != NULL ? from_alias : "<source>",
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias != NULL ? to_alias : "<target>",
                    from_alias != NULL ? from_alias : "<source>");
            }
            if (to_involves != NULL && to_zone_decl == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' transfer target '%s' must be a zone binding.\n"
                    "Reason:\n"
                    "- contract source is transfer handoff edge '%s' -> '%s'\n"
                    "- transfer materializes handoff into a target zone\n"
                    "- target binding '%s' does not resolve to a zone declaration\n"
                    "Fix:\n"
                    "- use a zone binding as the transfer target\n"
                    "- or change the step contract so the handoff target is a declared zone",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias != NULL ? to_alias : "<target>",
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias != NULL ? to_alias : "<target>",
                    to_alias != NULL ? to_alias : "<target>");
            }
            if (to_type != NULL && where_zone_type != NULL && !type_equals(to_type, where_zone_type)) {
                char contract_summary[512];
                intent_step_format_contract_source_summary(
                    node, step, ctx, contract_summary, sizeof(contract_summary));
                semantic_error(ctx, step,
                    "Intent step '%s' transfer target '%s' does not match the current zone contract.\n"
                    "Reason:\n"
                    "- contract source is the transfer handoff edge '%s' -> '%s'\n"
                    "- transfer target '%s' has zone type '%s'\n"
                    "- current step zone is '%s'\n"
                    "- transfer target derivation would choose zone '%s'\n"
                    "%s- current zone contract provenance is:\n"
                    "%s%s"
                    "Fix:\n"
                    "- change the transfer target to a '%s' binding\n"
                    "- or override the step zone to '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    to_alias != NULL ? to_alias : "<target>",
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias != NULL ? to_alias : "<target>",
                    to_alias != NULL ? to_alias : "<target>",
                    to_type->name != NULL ? to_type->name : "<zone>",
                    where_zone_type->name != NULL ? where_zone_type->name : "<zone>",
                    to_type->name != NULL ? to_type->name : "<zone>",
                    contract_summary[0] != '\0' ? "" : "- no additional step contract provenance was recorded\n",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    where_zone_type->name != NULL ? where_zone_type->name : "<zone>",
                    to_type->name != NULL ? to_type->name : "<zone>");
            }
            if (step->data.intent_step.using_expr != NULL
                && step->data.intent_step.using_expr->type == AST_IDENTIFIER
                && to_alias != NULL
                && strcmp(step->data.intent_step.using_expr->data.identifier.name, to_alias) != 0) {
                char contract_summary[512];
                intent_step_format_contract_source_summary(
                    node, step, ctx, contract_summary, sizeof(contract_summary));
                semantic_error(ctx, step->data.intent_step.using_expr,
                    "Intent step '%s' using binding does not match the transfer target.\n"
                    "Reason:\n"
                    "- contract source is the transfer handoff edge '%s' -> '%s'\n"
                    "- current using binding is '%s'\n"
                    "- transfer target binding is '%s'\n"
                    "- transfer target derivation would choose using '%s'\n"
                    "%s- current handoff provenance is:\n"
                    "%s%s"
                    "Fix:\n"
                    "- change 'using' to '%s'\n"
                    "- or change the transfer target to '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    from_alias != NULL ? from_alias : "<source>",
                    to_alias != NULL ? to_alias : "<target>",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>",
                    to_alias,
                    to_alias,
                    to_alias,
                    contract_summary[0] != '\0' ? "" : "- no additional step contract provenance was recorded\n",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    step->data.intent_step.using_expr->data.identifier.name != NULL
                        ? step->data.intent_step.using_expr->data.identifier.name : "<binding>");
            }
        }

        if (step->data.intent_step.who_count == 0 && !has_subintent) {
            semantic_error(ctx, step, "Intent step '%s' requires at least one who participant",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
        }

        if (step->data.intent_step.intent_expr != NULL) {
            Type *intent_type = type_check_expression(step->data.intent_step.intent_expr, ctx);
            const char *callee_name = "<callee>";
            intent_clause_rejects_control_transfer(step->data.intent_step.intent_expr, ctx,
                step->data.intent_step.name, "intent");
            if (step->data.intent_step.intent_expr->type != AST_CALL
                || step->data.intent_step.intent_expr->data.call.callee == NULL
                || step->data.intent_step.intent_expr->data.call.callee->type != AST_IDENTIFIER) {
                semantic_error(ctx, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause must call a named intent before lowering to Bool-gated orchestration",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>");
            } else {
                const char *resolved_name =
                    step->data.intent_step.intent_expr->data.call.callee->data.identifier.name;
                callee_name = resolved_name != NULL ? resolved_name : callee_name;
                Symbol *intent_sym = callee_name != NULL
                    ? scope_lookup(ctx->scope, callee_name)
                    : NULL;
                if (intent_sym == NULL || intent_sym->kind != SYMBOL_INTENT) {
                    semantic_error(ctx, step->data.intent_step.intent_expr,
                        "Intent step '%s' intent clause target '%s' is not a declared intent (resolved kind: %s). "
                        "Boolean-gated orchestration requires a declared intent that returns Bool. "
                        "Declare '%s' as an intent or replace this clause with a Bool-compatible predicate.",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        callee_name != NULL ? callee_name : "<callee>",
                        intent_sym != NULL
                            ? semantic_symbol_kind_label(intent_sym->kind)
                            : "unresolved",
                        callee_name != NULL ? callee_name : "<callee>");
                }
            }
            if (intent_type != NULL && !type_equals(intent_type, TYPE_BOOL)) {
                semantic_error(ctx, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause must return Bool for boolean-orchestration, got '%s'. "
                    "Expected `Bool`; adjust the callee '%s' return type to Bool or wrap with a Bool predicate.",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    intent_type->name != NULL ? intent_type->name : "<type>",
                    callee_name != NULL ? callee_name : "<callee>");
            } else if (intent_type == NULL) {
                semantic_error(ctx, step->data.intent_step.intent_expr,
                    "Intent step '%s' intent clause type could not be resolved as Bool. "
                    "Boolean-gated orchestration requires the called clause to evaluate to Bool; "
                    "make '%s' return Bool or wrap it in a Bool predicate.",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    callee_name != NULL ? callee_name : "<callee>");
            }
        }

        for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
            if (step->data.intent_step.on_exprs[j] != NULL) {
                intent_clause_rejects_control_transfer(step->data.intent_step.on_exprs[j], ctx,
                    step->data.intent_step.name, "on");
                type_check_expression(step->data.intent_step.on_exprs[j], ctx);
                if (intent_clause_invokes_authority_sensitive_call(
                        step->data.intent_step.on_exprs[j], ctx))
                    step_requires_authority_flow = true;
            }
        }
        for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
            if (step->data.intent_step.compensate_exprs[j] != NULL) {
                intent_clause_rejects_control_transfer(
                    step->data.intent_step.compensate_exprs[j], ctx,
                    step->data.intent_step.name, "compensate");
                type_check_expression(step->data.intent_step.compensate_exprs[j], ctx);
                if (intent_clause_invokes_authority_sensitive_call(
                        step->data.intent_step.compensate_exprs[j], ctx))
                    step_requires_authority_flow = true;
            }
        }
        if (step->data.intent_step.pre_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.pre_expr, ctx,
                step->data.intent_step.name, "pre");
            intent_condition_is_bool(step->data.intent_step.pre_expr, ctx, "pre");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.pre_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.guard_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.guard_expr, ctx,
                step->data.intent_step.name, "guard");
            intent_condition_is_bool(step->data.intent_step.guard_expr, ctx, "guard");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.guard_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.post_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.post_expr, ctx,
                step->data.intent_step.name, "post");
            intent_condition_is_bool(step->data.intent_step.post_expr, ctx, "post");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.post_expr, ctx))
                step_requires_authority_flow = true;
        }
        if (step->data.intent_step.invariant_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.invariant_expr, ctx,
                step->data.intent_step.name, "invariant");
            intent_condition_is_bool(step->data.intent_step.invariant_expr, ctx, "invariant");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.invariant_expr, ctx))
                step_requires_authority_flow = true;
        }

        for (size_t j = 0; j < step->data.intent_step.who_count; j++) {
            const char *alias = step->data.intent_step.who_names[j];
            ASTNode *involves = find_intent_involves_local(node, alias);
            const char *participant_type_name = NULL;
            if (involves == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' refers to unknown participant '%s'",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    alias != NULL ? alias : "<participant>");
                continue;
            }

            participant_type_name = intent_involves_type_name(involves);
            if (!intent_involves_is_subject_host(ctx->program_root, involves)) {
                semantic_error(ctx, step,
                    "Intent step '%s' who participant '%s' must bind to a subject type",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    alias != NULL ? alias : "<participant>");
                continue;
            }
            if (zone_decl != NULL && participant_type_name != NULL
                && !domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                    zone_decl->data.zone_decl.slot_count, ctx, participant_type_name)) {
                semantic_error(ctx, step,
                    "Intent step '%s' binds participant '%s' of type '%s', but zone '%s' has no matching subject slot",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    alias != NULL ? alias : "<participant>",
                    participant_type_name,
                    zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
            }
            if (step->data.intent_step.transfer_from_alias != NULL && participant_type_name != NULL) {
                ASTNode *from_involves = find_intent_involves_local(node,
                    step->data.intent_step.transfer_from_alias);
                Type *from_type = from_involves != NULL
                    ? resolve_type_node(from_involves->data.intent_involves.subject_type, ctx) : NULL;
                ASTNode *from_zone_decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                    from_type != NULL ? from_type->name : NULL);
                if (from_zone_decl != NULL
                    && !domain_has_subject_slot_type(from_zone_decl->data.zone_decl.slots,
                        from_zone_decl->data.zone_decl.slot_count, ctx, participant_type_name)) {
                    semantic_error(ctx, step,
                        "Intent step '%s' transfer source zone '%s' has no matching subject slot for participant '%s' of type '%s'.\n"
                        "Reason:\n"
                        "- contract source is transfer handoff edge '%s' -> '%s'\n"
                        "- handoff source zone '%s' must be able to host participant '%s' before transfer can materialize\n"
                        "- participant '%s' has type '%s', but zone '%s' declares no matching subject slot for that type\n"
                        "Fix:\n"
                        "- add a subject slot of type '%s' to zone '%s'\n"
                        "- or use a participant whose type matches one of zone '%s' subject slots\n"
                        "- or change the transfer source zone binding",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        from_zone_decl->data.zone_decl.name != NULL
                            ? from_zone_decl->data.zone_decl.name : "<zone>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        step->data.intent_step.transfer_from_alias != NULL
                            ? step->data.intent_step.transfer_from_alias : "<sourceZone>",
                        step->data.intent_step.transfer_to_alias != NULL
                            ? step->data.intent_step.transfer_to_alias : "<targetZone>",
                        from_zone_decl->data.zone_decl.name != NULL
                            ? from_zone_decl->data.zone_decl.name : "<zone>",
                        alias != NULL ? alias : "<participant>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        from_zone_decl->data.zone_decl.name != NULL
                            ? from_zone_decl->data.zone_decl.name : "<zone>",
                        participant_type_name,
                        from_zone_decl->data.zone_decl.name != NULL
                            ? from_zone_decl->data.zone_decl.name : "<zone>",
                        from_zone_decl->data.zone_decl.name != NULL
                            ? from_zone_decl->data.zone_decl.name : "<zone>");
                }
            }

            if (involves->data.intent_involves.subject_type != NULL
                && involves->data.intent_involves.subject_type->type == AST_TYPE
                && involves->data.intent_involves.subject_type->data.type.name != NULL) {
                ASTNode *subject_decl = find_subject_host_decl_by_name(ctx->program_root,
                    involves->data.intent_involves.subject_type->data.type.name);
                if (subject_decl_has_action_named(subject_decl, step->data.intent_step.name))
                    matched_action = true;
            }
        }

        for (size_t j = 0; j < step->data.intent_step.required_ability_count; j++) {
            ASTNode *ability_ref = step->data.intent_step.required_abilities[j];
            const char *ability = ability_ref_name(ability_ref);
            char *required_text = ability_ref_display(ability_ref);
            semantic_type_resolution_record_type_ref_dependency(
                ctx,
                step,
                step->data.intent_step.name != NULL
                    ? step->data.intent_step.name : "<step>",
                ability_ref,
                "intent step ability consumer lookup");
            if (resolve_required_ability_decl(
                    ability_ref, step, ctx, "Intent step",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>") == NULL) {
                free(required_text);
                continue;
            }

            for (size_t k = 0; k < step->data.intent_step.who_count; k++) {
                const char *alias = step->data.intent_step.who_names[k];
                ASTNode *involves = find_intent_involves_local(node, alias);
                const char *participant_type_name = intent_involves_type_name(involves);
                if (!intent_involves_is_subject_host(ctx->program_root, involves))
                    continue;
                if (participant_type_name != NULL
                    && !subject_type_has_ability(ctx->program_root, participant_type_name, ability_ref)) {
                    ASTNode *actual_impl = subject_type_find_base_ability_impl(
                        ctx->program_root, participant_type_name, ability);
                    char *actual_text = actual_impl != NULL ? ability_ref_display(actual_impl) : NULL;
                    char contract_summary[512];
                    intent_step_format_contract_source_summary(
                        node, step, ctx, contract_summary, sizeof(contract_summary));
                    if (actual_impl != NULL) {
                        semantic_error(ctx, step,
                            "Intent step '%s' requires ability '%s', but participant '%s' of type '%s' implements '%s' instead.\n"
                            "Reason:\n"
                            "- participant type '%s' does not satisfy required ability '%s'\n"
                            "- actual implementation is '%s'\n"
                            "%s%s"
                            "Fix:\n"
                            "- implement '%s' for subject type '%s'\n"
                            "- or choose a participant whose subject type satisfies the contract\n"
                            "- or override/remove the inherited step requirement",
                            step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            alias != NULL ? alias : "<participant>",
                            participant_type_name,
                            actual_text,
                            participant_type_name,
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            actual_text,
                            contract_summary[0] != '\0' ? contract_summary : "",
                            contract_summary[0] != '\0' ? "\n" : "",
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            participant_type_name);
                    } else {
                        semantic_error(ctx, step,
                            "Intent step '%s' requires ability '%s', but participant '%s' of type '%s' does not implement it.\n"
                            "Reason:\n"
                            "- participant type '%s' does not satisfy required ability '%s'\n"
                            "- no matching implementation was found for '%s'\n"
                            "%s%s"
                            "Fix:\n"
                            "- implement '%s' for subject type '%s'\n"
                            "- or choose a participant whose subject type satisfies the contract\n"
                            "- or override/remove the inherited step requirement",
                            step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            alias != NULL ? alias : "<participant>",
                            participant_type_name,
                            participant_type_name,
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            contract_summary[0] != '\0' ? contract_summary : "",
                            contract_summary[0] != '\0' ? "\n" : "",
                            required_text != NULL ? required_text
                                                  : (ability != NULL ? ability : "<ability>"),
                            participant_type_name);
                    }
                    free(actual_text);
                }
            }
            free(required_text);
        }

        if (step->data.intent_step.causes_effect != NULL
            && find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                step->data.intent_step.causes_effect) == NULL) {
            semantic_error(ctx, step,
                "Intent step '%s' causes unknown effect '%s'",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                step->data.intent_step.causes_effect);
        } else if (step->data.intent_step.causes_effect != NULL
            && zone_decl != NULL
            && !zone_has_effect_layer_type(zone_decl, step->data.intent_step.causes_effect)) {
            semantic_error(ctx, step,
                "Intent step '%s' causes effect '%s', but zone '%s' has no matching effect slot.\n"
                "Reason:\n"
                "- causes requires the current zone to materialize a matching effect layer\n"
                "- zone '%s' does not declare any effect slot of type '%s'\n"
                "Fix:\n"
                "- add an effect slot of type '%s' to zone '%s'\n"
                "- or remove/change the causes clause",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                step->data.intent_step.causes_effect,
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                step->data.intent_step.causes_effect,
                step->data.intent_step.causes_effect,
                zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
        }

        if (zone_decl != NULL
            && zone_decl->data.zone_decl.authority_count > 0
            && step->data.intent_step.authorized_by_count == 0
            && !has_subintent
            && (((step->data.intent_step.where_type != NULL
                  && !step->data.intent_step.derived_where_from_using)
                 || step->data.intent_step.inherited_where_from_action
                 || step->data.intent_step.derived_where_from_transfer)
                || step->data.intent_step.causes_effect != NULL
                || step->data.intent_step.transfer_from_alias != NULL
                || step->data.intent_step.transfer_to_alias != NULL
                || step_requires_authority_flow)) {
            char inherited_summary[512];
            const char *suggested_authorizer = intent_step_single_who_alias(step);
            const char *authority_reason =
                (step->data.intent_step.causes_effect != NULL
                 || step->data.intent_step.transfer_from_alias != NULL
                 || step->data.intent_step.transfer_to_alias != NULL
                 || step_requires_authority_flow)
                    ? "- the step causes effects, transfers zone state, or invokes authority-sensitive helpers"
                    : "- zone authority requires explicit approval even for declarative steps in this zone";
            intent_step_format_contract_source_summary(
                node, step, ctx, inherited_summary, sizeof(inherited_summary));
            if (step->data.intent_step.inherited_where_from_action) {
                semantic_error(ctx, step,
                    "Intent step '%s' cannot run in authority-bearing zone '%s' without 'authorized by'.\n"
                    "Reason:\n"
                    "%s\n"
                    "- zone '%s' was reused from the matching action contract\n"
                    "- contract source is the inherited/derived zone provenance listed below\n"
                    "%s- approval boundary provenance is:\n"
                    "%s%s"
                    "Fix:\n"
                    "- add 'authorized by: %s;' to the step\n"
                    "- or move the step to a non-authority zone\n"
                    "- or change the action contract that this step reuses",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                    authority_reason,
                    zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                    inherited_summary[0] != '\0' ? "" : "- no inherited or derived zone contract provenance was recorded\n",
                    inherited_summary[0] != '\0' ? inherited_summary : "",
                    inherited_summary[0] != '\0' ? "\n" : "",
                    suggested_authorizer != NULL ? suggested_authorizer : "<participant>");
            } else {
                semantic_error(ctx, step,
                    "Intent step '%s' cannot run in authority-bearing zone '%s' without 'authorized by'.\n"
                    "Reason:\n"
                    "%s\n"
                    "- zone '%s' declares authority, so explicit approval is required\n"
                    "- contract source is the inherited/derived zone provenance listed below\n"
                    "%s- approval boundary provenance is:\n"
                    "%s%s"
                    "Fix:\n"
                    "- add 'authorized by: %s;' to the step\n"
                    "- or move the step to a non-authority zone\n"
                    "- or remove the authority-sensitive operation from this step",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                    authority_reason,
                    zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                    inherited_summary[0] != '\0' ? "" : "- no inherited or derived zone contract provenance was recorded\n",
                    inherited_summary[0] != '\0' ? inherited_summary : "",
                    inherited_summary[0] != '\0' ? "\n" : "",
                    suggested_authorizer != NULL ? suggested_authorizer : "<participant>");
            }
        }

        for (size_t j = 0; j < step->data.intent_step.authorized_by_count; j++) {
            const char *alias = step->data.intent_step.authorized_by[j];
            ASTNode *involves = find_intent_involves_local(node, alias);
            const char *participant_type_name = intent_involves_type_name(involves);
            char contract_summary[512];
            intent_step_format_contract_source_summary(
                node, step, ctx, contract_summary, sizeof(contract_summary));
            if (involves == NULL) {
                semantic_error(ctx, step,
                    "Intent step '%s' authorizes unknown participant '%s'.\n"
                    "Reason:\n"
                    "- the authorized-by clause references a participant alias that is not declared on the intent\n"
                    "- contract source is the authority-bearing zone/step approval requirement summarized below\n"
                    "%s%s"
                    "Fix:\n"
                    "- add 'involves %s: <Subject>' to the intent\n"
                    "- or change 'authorized by' to one of the declared participants",
                    step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                    alias != NULL ? alias : "<participant>",
                    contract_summary[0] != '\0' ? contract_summary : "",
                    contract_summary[0] != '\0' ? "\n" : "",
                    alias != NULL ? alias : "<participant>");
                continue;
            }

            if (zone_decl != NULL && participant_type_name != NULL) {
                if (!intent_involves_is_subject_host(ctx->program_root, involves)) {
                    semantic_error(ctx, step,
                        "Intent step '%s' authorized participant '%s' must bind to a subject type.\n"
                        "Reason:\n"
                        "- only subject participants can satisfy zone authority contracts\n"
                        "- participant '%s' is not a subject host\n"
                        "- contract source is the authority-bearing zone/step approval requirement summarized below\n"
                        "%s%s"
                        "Fix:\n"
                        "- authorize a subject participant instead\n"
                        "- or change the intent binding so '%s' is a subject type",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        alias != NULL ? alias : "<participant>",
                        alias != NULL ? alias : "<participant>",
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        alias != NULL ? alias : "<participant>");
                    continue;
                }
                if (!domain_has_subject_slot_type(zone_decl->data.zone_decl.slots,
                        zone_decl->data.zone_decl.slot_count, ctx, participant_type_name)) {
                    semantic_error(ctx, step,
                        "Intent step '%s' authorized participant '%s' has type '%s', but zone '%s' has no matching subject slot.\n"
                        "Reason:\n"
                        "- authorized participants must map to a subject slot inside the current zone\n"
                        "- contract source is authority-bearing zone '%s' with authorized participant '%s'\n"
                        "- zone '%s' does not declare a subject slot of type '%s'\n"
                        "%s%s"
                        "Fix:\n"
                        "- authorize a participant whose subject type exists in zone '%s'\n"
                        "- or add a matching subject slot to zone '%s'",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        alias != NULL ? alias : "<participant>",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        participant_type_name,
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
                } else if (zone_decl->data.zone_decl.authority_count > 0
                    && !zone_has_authority_for_subject_type(zone_decl, ctx, participant_type_name)) {
                    semantic_error(ctx, step,
                        "Intent step '%s' authorized participant '%s' has type '%s', but zone '%s' declares no matching authority.\n"
                        "Reason:\n"
                        "- contract source is authority-bearing zone '%s' with authorized participant '%s'\n"
                        "- zone '%s' has authority rules, but none match subject type '%s'\n"
                        "%s%s"
                        "Fix:\n"
                        "- authorize a participant whose subject type is listed in zone authority\n"
                        "- or add an authority declaration for subject type '%s' in zone '%s'",
                        step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                        alias != NULL ? alias : "<participant>",
                        participant_type_name,
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        alias != NULL ? alias : "<participant>",
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>",
                        participant_type_name,
                        contract_summary[0] != '\0' ? contract_summary : "",
                        contract_summary[0] != '\0' ? "\n" : "",
                        participant_type_name,
                        zone_decl->data.zone_decl.name != NULL ? zone_decl->data.zone_decl.name : "<zone>");
                }
            }
        }

        if (step->data.intent_step.expect_expr != NULL) {
            intent_clause_rejects_control_transfer(step->data.intent_step.expect_expr, ctx,
                step->data.intent_step.name, "expect");
            intent_condition_is_bool(step->data.intent_step.expect_expr, ctx, "expect");
            if (intent_clause_invokes_authority_sensitive_call(
                    step->data.intent_step.expect_expr, ctx))
                step_requires_authority_flow = true;
        }

        if (!matched_action && step->data.intent_step.on_expr_count == 0
            && step->data.intent_step.intent_expr == NULL) {
            char contract_summary[512];
            intent_step_format_contract_source_summary(
                node, step, ctx, contract_summary, sizeof(contract_summary));
            semantic_warning(ctx, step,
                "Intent step '%s' does not currently match a subject action of the same name; intent is declarative only.\n"
                "Reason:\n"
                "- matching action contract reuse only applies when a unique matching subject action exists\n"
                "%s%s"
                "Fix:\n"
                "- align the step name and who participants with a subject action if you want reused contracts\n"
                "- or keep the step declarative and spell out who/where/requires/authorized by explicitly",
                step->data.intent_step.name != NULL ? step->data.intent_step.name : "<step>",
                contract_summary[0] != '\0' ? contract_summary : "",
                contract_summary[0] != '\0' ? "\n" : "");
        }
    }

    if (node->data.intent_decl.priority_expr != NULL) {
        Type *priority_type = type_check_expression(node->data.intent_decl.priority_expr, ctx);
        intent_clause_rejects_control_transfer(node->data.intent_decl.priority_expr, ctx,
            name, "priority");
        if (priority_type != NULL && !type_equals(priority_type, TYPE_INT)) {
            semantic_error(ctx, node->data.intent_decl.priority_expr,
                "Intent priority expression must be Int, got '%s'",
                priority_type->name != NULL ? priority_type->name : "<type>");
        }
    }

    if (node->data.intent_decl.success_expr != NULL) {
        intent_clause_rejects_control_transfer(node->data.intent_decl.success_expr, ctx,
            name, "success");
        intent_condition_is_bool(node->data.intent_decl.success_expr, ctx, "success");
    }
    if (node->data.intent_decl.failure_expr != NULL) {
        intent_clause_rejects_control_transfer(node->data.intent_decl.failure_expr, ctx,
            name, "failure");
        intent_condition_is_bool(node->data.intent_decl.failure_expr, ctx, "failure");
    }

    scope_exit(&ctx->scope);
    return !ctx->has_error;
}

bool
type_check_world_decl(ASTNode *node, SemanticContext *ctx);

static ASTNode *
