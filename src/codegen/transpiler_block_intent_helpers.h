static ASTNode *
find_intent_participant_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

static ASTNode *
find_subject_action_decl(TranspilerCtx *ctx, const char *subject_name, const char *action_name)
{
    ASTNode *decl;
    ASTNode *method;

    if (ctx == NULL || subject_name == NULL || action_name == NULL)
        return NULL;

    decl = find_subject_host_decl(ctx, subject_name);
    if (decl == NULL || decl->type != AST_CLASS_DECL
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        return NULL;
    }

    method = find_nominal_host_method_decl(ctx, subject_name, action_name);
    if (method == NULL || method->type != AST_FUNC_DECL
        || !method->data.func_decl.is_action) {
        return NULL;
    }
    return method;
}

static ASTNode *
find_zone_decl_in_program_view(TranspilerCtx *ctx, const char *zone_name)
{
    return transpiler_find_decl_in_inventory_local(ctx, AST_ZONE_DECL, zone_name);
}

static const char *
intent_participant_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                              ASTNode *step, const char *alias);

static const char *
resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx, ASTNode *intent,
                                       const char *zone_type_name, const char *alias);

static const char *
intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        return step->data.intent_step.using_expr->data.identifier.name;
    }
    return step->data.intent_step.transfer_to_alias;
}

static const char *
intent_zone_binding_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
intent_zone_binding_type_name_with_metadata(ASTNode *intent,
                                            const char *alias,
                                            const char **participant_aliases,
                                            const char **participant_types,
                                            size_t participant_count)
{
    if (alias != NULL && participant_aliases != NULL && participant_types != NULL) {
        for (size_t i = 0; i < participant_count; i++) {
            if (participant_aliases[i] != NULL
                && participant_types[i] != NULL
                && strcmp(participant_aliases[i], alias) == 0) {
                return participant_types[i];
            }
        }
    }
    return intent_zone_binding_type_name(intent, alias);
}

static const char *
intent_involves_type_name_local(ASTNode *involves)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL
        || involves->data.intent_involves.subject_type->type != AST_TYPE) {
        return NULL;
    }
    return involves->data.intent_involves.subject_type->data.type.name;
}

static bool
intent_involves_is_subject_participant(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_subject_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
}

static bool
intent_involves_uses_pointer_self(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_pointer_self_host_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
}

static void
emit_intent_step_bind_bound_zone(CodeBuf *out, TranspilerCtx *ctx,
                                 ASTNode *intent, ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type;
    const char *from_alias;
    const char *from_zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return;

    from_alias = step->data.intent_step.transfer_from_alias;
    from_zone_type = intent_zone_binding_type_name(intent, from_alias);

    if (from_alias != NULL && from_zone_type != NULL) {

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
                if (ctx->uses_intent_observability) {
                    write_indent(ctx);
                    codebuf_write(out,
                        "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                        alias, from_slot_name, from_zone_type);
                }
            }
            if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", zone_alias, to_slot_name, alias);
                if (ctx->uses_intent_observability) {
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
        if (ctx->uses_intent_observability) {
            write_indent(ctx);
            codebuf_write(out,
                "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                alias, slot_name, zone_type);
        }
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
                if (ctx->uses_intent_observability) {
                    write_indent(ctx);
                    codebuf_write(out,
                        "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                        alias, from_slot_name, from_zone_type);
                }
            }
            if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", zone_alias, to_slot_name, alias);
                if (ctx->uses_intent_observability) {
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
        if (ctx->uses_intent_observability) {
            write_indent(ctx);
            codebuf_write(out,
                "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                alias, slot_name, zone_type);
        }
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

#include "transpiler_block_intent_rebind_helpers.h"

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
        if (ctx->uses_intent_observability) {
            codebuf_write(out,
                "pgy_intent_trace_fail_export(__intent_handle, \"authority:%s\"); ",
                step_name != NULL ? step_name : "<step>");
        }
        codebuf_write(out, "__intent_result = false; ");
        if (emit_cleanup_from_mir) {
            codebuf_write(out, "goto _pgy_mir_bb_%s_%zu; }\n",
                intent_name != NULL ? intent_name : "intent", cleanup_block);
        } else {
            codebuf_write(out, "goto __intent_cleanup; }\n");
        }
    }
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
