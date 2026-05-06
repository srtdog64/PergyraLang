#ifndef PGY_SRC_CODEGEN_TRANSPILER_BLOCK_INTENT_REBIND_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_BLOCK_INTENT_REBIND_HELPERS_H

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
#endif /* PGY_SRC_CODEGEN_TRANSPILER_BLOCK_INTENT_REBIND_HELPERS_H */
