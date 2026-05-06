#ifndef PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H
#define PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H

/* C backend intent signature and runtime entry emission owner. */

static bool
transpiler_emit_intent_signature_and_entry(ASTNode *node,
                                           TranspilerCtx *ctx,
                                           bool mir_only_intent,
                                           bool has_compensate_steps,
                                           size_t step_count,
                                           const char **participant_aliases,
                                           const char **participant_types,
                                           size_t participant_count,
                                           bool emit_cleanup_from_mir,
                                           const MIRRoutine *mir_routine)
{
    size_t subject_count = 0;

    if (node == NULL || ctx == NULL)
        return false;

    codebuf_write(ctx->out, "\nbool\n%s(", node->data.intent_decl.name);
    {
        size_t binding_count = node->data.intent_decl.binding_count > 0
            ? node->data.intent_decl.binding_count
            : (node->data.intent_decl.involve_count + node->data.intent_decl.value_count);
        size_t participant_index = 0;
        for (size_t i = 0; i < binding_count; i++) {
            ASTNode *binding = node->data.intent_decl.binding_count > 0
                ? node->data.intent_decl.bindings[i]
                : (i < node->data.intent_decl.involve_count
                    ? node->data.intent_decl.involves[i]
                    : node->data.intent_decl.values[i - node->data.intent_decl.involve_count]);
            const char *pt = NULL;
            const char *alias = "value";
            char *type_name = NULL;
            bool pointer_param = false;
            char surface_desc[256];

            if (i > 0)
                codebuf_write(ctx->out, ", ");

            if (binding != NULL && binding->type == AST_INTENT_INVOLVES) {
                const char *participant_type = participant_types != NULL && participant_index < participant_count
                    ? participant_types[participant_index]
                    : NULL;
                alias = (mir_only_intent && participant_aliases != NULL && participant_index < participant_count
                    && participant_aliases[participant_index] != NULL)
                    ? participant_aliases[participant_index]
                    : (binding->data.intent_involves.alias != NULL
                        ? binding->data.intent_involves.alias : "participant");
                snprintf(surface_desc, sizeof(surface_desc),
                    "intent participant '%s' of '%s'",
                    alias != NULL ? alias : "(anonymous)",
                    node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
                if (participant_type != NULL) {
                    pt = transpiler_require_type_name_c_type(ctx, participant_type, surface_desc);
                    type_name = pergyra_strdup(participant_type);
                    pointer_param = is_pointer_self_host_type_name(ctx, participant_type);
                } else if (!mir_only_intent && binding->data.intent_involves.subject_type != NULL) {
                    pt = transpiler_require_ast_c_type(
                        ctx, binding->data.intent_involves.subject_type, surface_desc);
                    type_name = render_type_name(binding->data.intent_involves.subject_type);
                    pointer_param = intent_involves_uses_pointer_self(ctx, binding);
                }
                participant_index++;
            } else {
                ASTNode *value_type = binding != NULL ? binding->data.intent_value.value_type : NULL;
                alias = (binding != NULL && binding->data.intent_value.alias != NULL)
                    ? binding->data.intent_value.alias : "value";
                snprintf(surface_desc, sizeof(surface_desc),
                    "intent value '%s' of '%s'",
                    alias,
                    node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
                pt = transpiler_require_ast_c_type(ctx, value_type, surface_desc);
                if (value_type != NULL)
                    type_name = render_type_name(value_type);
            }
            if (pt == NULL)
                return false;
            codebuf_write(ctx->out, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
            if (type_name != NULL) {
                register_typed_var(ctx, alias, type_name);
                if (pointer_param) {
                    TypedVarEntry *entry = lookup_typed_entry(ctx, alias);
                    if (entry != NULL)
                        entry->is_subject_ref = true;
                }
                free(type_name);
            }
        }
    }
    codebuf_write(ctx->out, ")\n{\n");
    ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "bool __intent_result = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "bool __intent_failed = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "(void)__intent_failed;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "int32_t __intent_handle = 0;\n");
    if (has_compensate_steps && step_count > 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "bool __intent_step_completed[%zu] = { false };\n",
            step_count);
    }
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        const char *participant_type = participant_types != NULL && i < participant_count
            ? participant_types[i]
            : NULL;
        bool is_subject = participant_type != NULL
            ? is_subject_type_name(ctx, participant_type)
            : intent_involves_is_subject_participant(ctx, involves);
        if (is_subject)
            subject_count++;
    }
    if (subject_count > 0) {
        write_indent(ctx);
        codebuf_write(ctx->out, "void *__intent_subjects[%zu];\n",
            subject_count);
        {
            size_t subject_index = 0;
            for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
                ASTNode *involves = node->data.intent_decl.involves[i];
                const char *alias = (mir_only_intent && participant_aliases != NULL && i < participant_count
                    && participant_aliases[i] != NULL)
                    ? participant_aliases[i]
                    : ((involves != NULL && involves->data.intent_involves.alias != NULL)
                        ? involves->data.intent_involves.alias : "participant");
                const char *participant_type = participant_types != NULL && i < participant_count
                    ? participant_types[i]
                    : NULL;
                bool is_subject = participant_type != NULL
                    ? is_subject_type_name(ctx, participant_type)
                    : intent_involves_is_subject_participant(ctx, involves);
                if (!is_subject)
                    continue;
                write_indent(ctx);
                codebuf_write(ctx->out, "__intent_subjects[%zu] = (void *)%s;\n",
                    subject_index++, alias);
            }
        }
    }
    {
        char *priority = NULL;
        bool owns_priority = false;
        if (node->data.intent_decl.priority_expr != NULL) {
            priority = emit_expression(node->data.intent_decl.priority_expr, ctx);
            owns_priority = (priority != NULL);
        } else {
            priority = (char *)transpiler_scratch_strdup(ctx, "0");
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "__intent_handle = pgy_intent_enter_export(\"%s\", %s, %zu, %s, %s);\n",
            node->data.intent_decl.name,
            subject_count > 0 ? "__intent_subjects" : "NULL",
            subject_count,
            node->data.intent_decl.is_concurrent ? "true" : "false",
            priority != NULL ? priority : "0");
        if (owns_priority)
            free(priority);
    }
    {
        write_indent(ctx);
        codebuf_write(ctx->out, "if (__intent_handle == 0) {\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_failed = true;\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "__intent_result = false;\n");
        write_indent(ctx);
        if (emit_cleanup_from_mir) {
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                node->data.intent_decl.name, mir_routine->cleanup_block);
        } else {
            codebuf_write(ctx->out, "goto __intent_cleanup;\n");
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    return true;
}

#endif /* PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H */
