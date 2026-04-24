        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names[i];
            const char *from_slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent,
                from_zone_type, alias);
            const char *to_slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent,
                zone_type, alias);
            if (alias == NULL)
                continue;
            if (from_slot_name != NULL && strcmp(from_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", from_alias, from_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                    alias, from_slot_name, from_zone_type);
            }
            if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", zone_alias, to_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_transfer_export(__intent_handle, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\");\n",
                    alias,
                    from_zone_type != NULL ? from_zone_type : "<zone>",
                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                    zone_type,
                    to_slot_name);
            }
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(%s);\n", from_zone_type, from_alias);
        if (strcmp(from_alias, zone_alias) != 0 || strcmp(from_zone_type, zone_type) != 0) {
            write_indent(ctx);
            codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
        }
        return;
    }

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name(ctx, intent, step, alias);
        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s->%s = *%s;\n", zone_alias, slot_name, alias);
        write_indent(ctx);
        codebuf_write(out,
            "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
            alias, slot_name, zone_type);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static void
emit_intent_step_bind_bound_zone_with_metadata(CodeBuf *out, TranspilerCtx *ctx,
                                               ASTNode *intent,
                                               const char *zone_type,
                                               const char *zone_alias,
                                               const char *from_alias,
                                               const char **who_aliases,
                                               size_t who_alias_count,
                                               const char **participant_aliases,
                                               const char **participant_types,
                                               size_t participant_count)
{
    const char *from_zone_type;

    if (out == NULL || ctx == NULL || intent == NULL
        || zone_alias == NULL || zone_type == NULL) {
        return;
    }

    from_zone_type = intent_zone_binding_type_name_with_metadata(
        intent, from_alias, participant_aliases, participant_types, participant_count);

    if (from_alias != NULL && from_zone_type != NULL) {
        for (size_t i = 0; i < who_alias_count; i++) {
            const char *alias = who_aliases[i];
            const char *from_slot_name = resolve_intent_zone_slot_name_for_zone(
                ctx, intent, from_zone_type, alias);
            const char *to_slot_name = resolve_intent_zone_slot_name_for_zone(
                ctx, intent, zone_type, alias);
            if (alias == NULL)
                continue;
            if (from_slot_name != NULL && strcmp(from_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", from_alias, from_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                    alias, from_slot_name, from_zone_type);
            }
            if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", zone_alias, to_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_transfer_export(__intent_handle, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\");\n",
                    alias,
                    from_zone_type != NULL ? from_zone_type : "<zone>",
                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                    zone_type,
                    to_slot_name);
            }
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(%s);\n", from_zone_type, from_alias);
        if (strcmp(from_alias, zone_alias) != 0 || strcmp(from_zone_type, zone_type) != 0) {
            write_indent(ctx);
            codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
        }
        return;
    }

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s->%s = *%s;\n", zone_alias, slot_name, alias);
        write_indent(ctx);
        codebuf_write(out,
            "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
            alias, slot_name, zone_type);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static bool
emit_intent_step_rebind_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                           ASTNode *intent, ASTNode *step,
                                           size_t step_index)
{
    const char *zone_alias;
    const char *zone_type;
    bool rebound = false;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return false;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return false;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_participant_local(intent, alias);
        const char *participant_c_type;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        participant_c_type = pergyra_ast_type_to_c(involves->data.intent_involves.subject_type);
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            participant_c_type, step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = &%s->%s;\n", alias, zone_alias, slot_name);
        rebound = true;
    }

    return rebound;
}

static bool
emit_intent_step_rebind_bound_zone_aliases_with_metadata(CodeBuf *out,
                                                         TranspilerCtx *ctx,
                                                         ASTNode *intent,
                                                         const char *zone_type,
                                                         const char *zone_alias,
                                                         const char **who_aliases,
                                                         size_t who_alias_count,
                                                         size_t step_index,
                                                         const char **participant_aliases,
                                                         const char **participant_types,
                                                         size_t participant_count)
{
    bool rebound = false;

    if (out == NULL || ctx == NULL || intent == NULL
        || zone_alias == NULL || zone_type == NULL) {
        return false;
    }

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        const char *participant_type = intent_zone_binding_type_name_with_metadata(
            intent, alias, participant_aliases, participant_types, participant_count);
        const char *participant_c_type;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || participant_type == NULL) {
            continue;
        }

        participant_c_type = pergyra_type_to_c(participant_type);
        if (participant_c_type == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            participant_c_type, step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = &%s->%s;\n", alias, zone_alias, slot_name);
        rebound = true;
    }

    return rebound;
}

static void
emit_intent_step_mark_caused_effect(CodeBuf *out, TranspilerCtx *ctx,
                                    const char *zone_type,
                                    const char *zone_alias,
                                    const char *causes_effect)
{
    ASTNode *zone_decl;

    if (out == NULL || ctx == NULL || zone_type == NULL || zone_alias == NULL
        || causes_effect == NULL) {
        return;
    }

    zone_decl = find_zone_decl(ctx, zone_type);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return;

    for (size_t i = 0; i < zone_decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer_slot = zone_decl->data.zone_decl.layer_slots[i];
        const char *layer_name;

        if (layer_slot == NULL || layer_slot->type != AST_ZONE_LAYER_SLOT
            || layer_slot->data.zone_layer_slot.is_relation
            || layer_slot->data.zone_layer_slot.slot_name == NULL
            || layer_slot->data.zone_layer_slot.layer_type == NULL
            || strcmp(layer_slot->data.zone_layer_slot.layer_type, causes_effect) != 0) {
            continue;
        }

        layer_name = layer_slot->data.zone_layer_slot.slot_name;
        write_indent(ctx);
        codebuf_write(out, "%s->__layer_epoch_%s++;\n", zone_alias, layer_name);
        write_indent(ctx);
        codebuf_write(out, "%s->__layer_cause_%s = 11;\n", zone_alias, layer_name);

        for (size_t j = 0; j < zone_decl->data.zone_decl.state_count; j++) {
            ASTNode *state = zone_decl->data.zone_decl.states[j];
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.state_name == NULL
                || state->data.zone_state.layer_slot_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name, layer_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(out, "%s->__state_epoch_%s++;\n",
                zone_alias, state->data.zone_state.state_name);
            write_indent(ctx);
            codebuf_write(out, "%s->__state_cause_%s = 11;\n",
                zone_alias, state->data.zone_state.state_name);
        }
    }
}

static void
emit_intent_step_validate_authority(CodeBuf *out,
                                    TranspilerCtx *ctx,
                                    const char *intent_name,
                                    const char *step_name,
                                    const char *zone_type,
                                    const char *zone_alias,
                                    const char **authorized_aliases,
                                    size_t authorized_alias_count,
                                    bool emit_cleanup_from_mir,
                                    size_t cleanup_block)
{
    if (out == NULL || ctx == NULL || zone_type == NULL
        || authorized_aliases == NULL || authorized_alias_count == 0) {
        return;
    }

    for (size_t i = 0; i < authorized_alias_count; i++) {
        const char *alias = authorized_aliases[i];
        TypedVarEntry *entry;
        const char *participant_present = "true";
        const char *zone_present = "true";

        if (alias == NULL)
            continue;

        entry = lookup_typed_entry(ctx, alias);
        if (entry != NULL && entry->is_subject_ref)
            participant_present = transpiler_scratch_fmt(ctx, "(%s != NULL)", alias);
        if (zone_alias != NULL)
            zone_present = transpiler_scratch_fmt(ctx, "(%s != NULL)", zone_alias);

        write_indent(ctx);
        codebuf_write(out,
            "if (!pgy_zone_authority_validate_flags_export(%s, %s, \"%s\", \"%s\")) { ",
            zone_present,
            participant_present,
            zone_type,
            alias);
        codebuf_write(out, "__intent_failed = true; ");
        codebuf_write(out,
            "pgy_intent_trace_fail_export(__intent_handle, \"authority:%s\"); __intent_result = false; ",
            step_name != NULL ? step_name : "<step>");
        if (emit_cleanup_from_mir) {
            codebuf_write(out, "goto _pgy_mir_bb_%s_%zu; }\n",
                intent_name != NULL ? intent_name : "intent", cleanup_block);
        } else {
            codebuf_write(out, "goto __intent_cleanup; }\n");
        }
    }
}

static const char *
infer_intent_step_causes_from_on_exprs(TranspilerCtx *ctx,
                                       ASTNode *intent,
                                       ASTNode **on_exprs,
                                       size_t on_expr_count,
                                       const char **participant_aliases,
                                       const char **participant_types,
                                       size_t participant_count)
{
    if (ctx == NULL || intent == NULL || on_exprs == NULL)
        return NULL;

    for (size_t i = 0; i < on_expr_count; i++) {
        ASTNode *expr = on_exprs[i];
        ASTNode *callee;
        ASTNode *receiver;
        const char *alias;
        const char *method_name;
        const char *subject_name;
        ASTNode *action_decl;

        if (expr == NULL || expr->type != AST_CALL)
            continue;
        callee = expr->data.call.callee;
        if (callee == NULL || callee->type != AST_MEMBER_ACCESS)
            continue;
        receiver = callee->data.member.object;
        method_name = callee->data.member.name;
        if (receiver == NULL || receiver->type != AST_IDENTIFIER
            || receiver->data.identifier.name == NULL || method_name == NULL) {
            continue;
        }

        alias = receiver->data.identifier.name;
        subject_name = intent_zone_binding_type_name_with_metadata(
            intent, alias, participant_aliases, participant_types, participant_count);
        action_decl = find_subject_action_decl(ctx, subject_name, method_name);
        if (action_decl != NULL && action_decl->type == AST_FUNC_DECL
            && action_decl->data.func_decl.causes_effect != NULL) {
            return action_decl->data.func_decl.causes_effect;
        }
    }

    return NULL;
}

static void
emit_intent_step_sync_effective_zone(CodeBuf *out, TranspilerCtx *ctx,
                                     ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type;

    if (out == NULL || ctx == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return;

    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static void
emit_intent_step_sync_effective_zone_with_metadata(CodeBuf *out, TranspilerCtx *ctx,
                                                   const char *zone_type,
                                                   const char *zone_alias)
{
    if (out == NULL || ctx == NULL || zone_alias == NULL || zone_type == NULL)
        return;

    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static void
emit_intent_step_restore_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                            ASTNode *intent, ASTNode *step,
                                            size_t step_index)
{
    const char *zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_type == NULL)
        return;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_participant_local(intent, alias);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}

static void
emit_intent_step_restore_bound_zone_aliases_with_metadata(CodeBuf *out,
                                                          TranspilerCtx *ctx,
                                                          ASTNode *intent,
                                                          const char *zone_type,
                                                          const char **who_aliases,
                                                          size_t who_alias_count,
                                                          size_t step_index)
{
    if (out == NULL || ctx == NULL || intent == NULL || zone_type == NULL)
        return;

    for (size_t i = 0; i < who_alias_count; i++) {
        const char *alias = who_aliases[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}

static const char *
resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                              ASTNode *step, const char *alias)
{
    if (ctx == NULL || intent == NULL || step == NULL || alias == NULL
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE
        || step->data.intent_step.where_type->data.type.name == NULL) {
        return "<unbound>";
    }
    return resolve_intent_zone_slot_name_for_zone(ctx, intent,
        step->data.intent_step.where_type->data.type.name, alias);
}

static const char *
resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx, ASTNode *intent,
                                       const char *zone_type_name, const char *alias)
{
    ASTNode *zone_decl = NULL;
    const char *participant_type = NULL;
    ASTNode *named_match = NULL;
    ASTNode *typed_match = NULL;

    if (ctx == NULL || intent == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_decl = find_zone_decl_in_program_view(ctx, zone_type_name);
    participant_type = intent_participant_type_name(intent, alias);
    if (zone_decl == NULL)
        return "<unbound>";

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        if (strcmp(slot->data.domain_slot.slot_name, alias) == 0) {
            named_match = slot;
            break;
        }
        if (participant_type != NULL
            && slot->data.domain_slot.type != NULL
            && slot->data.domain_slot.type->type == AST_TYPE
            && slot->data.domain_slot.type->data.type.name != NULL
            && strcmp(slot->data.domain_slot.type->data.type.name, participant_type) == 0) {
            if (typed_match != NULL)
                typed_match = (ASTNode *)(uintptr_t)1;
            else
                typed_match = slot;
        }
    }

    if (named_match != NULL)
        return named_match->data.domain_slot.slot_name;
    if (typed_match != NULL && typed_match != (ASTNode *)(uintptr_t)1)
        return typed_match->data.domain_slot.slot_name;
    return "<unbound>";
}

static bool
intent_action_has_only_self(ASTNode *action_decl)
{
    size_t real_pc = 0;
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return false;
    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *p = action_decl->data.func_decl.params[i];
        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        real_pc++;
    }
    return real_pc == 0;
}

static void
emit_intent_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    size_t participant_count = 0;
    size_t binding_count = 0;
    size_t participant_index = 0;

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;
    mir_routine = transpiler_find_mir_intent(ctx, node);
    if (mir_routine != NULL) {
        participant_count = transpiler_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
    }
    binding_count = node->data.intent_decl.binding_count > 0
        ? node->data.intent_decl.binding_count
        : (node->data.intent_decl.involve_count + node->data.intent_decl.value_count);
    codebuf_write(buf, "\nbool\n%s(", node->data.intent_decl.name);
    for (size_t i = 0; i < binding_count; i++) {
        ASTNode *binding = node->data.intent_decl.binding_count > 0
            ? node->data.intent_decl.bindings[i]
            : (i < node->data.intent_decl.involve_count
                ? node->data.intent_decl.involves[i]
                : node->data.intent_decl.values[i - node->data.intent_decl.involve_count]);
        const char *pt = NULL;
        const char *alias = "value";
        bool pointer_param = false;
        char surface_desc[256];

        if (i > 0)
            codebuf_write(buf, ", ");

        if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
            const char *participant_type = participant_types != NULL
                    && participant_index < participant_count
                ? participant_types[participant_index]
                : NULL;
            alias = binding->data.intent_involves.alias != NULL
                ? binding->data.intent_involves.alias : "participant";
            snprintf(surface_desc, sizeof(surface_desc),
                "intent participant '%s' of '%s'",
                alias != NULL ? alias : "(anonymous)",
                node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
            if (participant_type != NULL) {
                pt = transpiler_require_type_name_c_type(ctx, participant_type, surface_desc);
                pointer_param = is_pointer_self_host_type_name(ctx, participant_type);
            } else if (binding->data.intent_involves.subject_type != NULL) {
                pt = transpiler_require_ast_c_type(
                    ctx, binding->data.intent_involves.subject_type, surface_desc);
                pointer_param = intent_involves_uses_pointer_self(ctx, binding);
            }
            if (participant_aliases != NULL && participant_index < participant_count
                && participant_aliases[participant_index] != NULL) {
                alias = participant_aliases[participant_index];
            }
            participant_index++;
            if (pt == NULL) {
                free((void *)participant_aliases);
                free((void *)participant_types);
                return;
            }
            codebuf_write(buf, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
            continue;
        }

        alias = binding != NULL && binding->data.intent_value.alias != NULL
            ? binding->data.intent_value.alias : "value";
        snprintf(surface_desc, sizeof(surface_desc),
            "intent value '%s' of '%s'",
            alias,
            node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
        pt = transpiler_require_ast_c_type(
            ctx,
            binding != NULL ? binding->data.intent_value.value_type : NULL,
            surface_desc);
        if (pt == NULL) {
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
        codebuf_write(buf, "%s %s", pt, alias);
    }
    codebuf_write(buf, ");\n");
    free((void *)participant_aliases);
    free((void *)participant_types);
}

static bool
transpiler_can_forward_declare_intent_early(TranspilerCtx *ctx, ASTNode *intent)
{
    if (ctx == NULL || intent == NULL || intent->type != AST_INTENT_DECL)
        return false;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES
            || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, involves->data.intent_involves.subject_type)) {
            return false;
        }
    }
    for (size_t i = 0; i < intent->data.intent_decl.value_count; i++) {
        ASTNode *value = intent->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE
            || value->data.intent_value.value_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, value->data.intent_value.value_type)) {
            return false;
        }
    }
    return true;
}

static void
