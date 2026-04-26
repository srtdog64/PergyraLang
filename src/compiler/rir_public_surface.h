static void
rir_dump_flow_semantics(FILE *out, unsigned int flags)
{
    bool wrote = false;

    if (flags == RIR_FLOW_NONE) {
        fputs("-", out);
        return;
    }
    if ((flags & RIR_FLOW_AUTHORITY) != 0U) {
        fputs("authority", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_PROJECTION) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("projection", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_WORLD_HANDOFF) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("world-handoff", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_INVALIDATION) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("invalidation", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_AUTHORITY_LOSS) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("authority-loss", out);
        wrote = true;
    }
    if ((flags & RIR_FLOW_PROJECTION_INVALIDATION) != 0U) {
        if (wrote)
            fputc('|', out);
        fputs("projection-invalidation", out);
    }
}

void
rir_destroy(RIRProgram *rir)
{
    if (rir == NULL)
        return;
    g_rir_program_root = NULL;
    for (size_t i = 0; i < rir->scope_count; i++) {
        free(rir->scopes[i].facts);
        free(rir->scopes[i].ops);
        free(rir->scopes[i].state_summaries);
        rir_free_flow_blocks(&rir->scopes[i]);
    }
    free(rir->scopes);
    free(rir);
}

static void
json_write_str(FILE *out, const char *s)
{
    if (s == NULL) {
        fputs("null", out);
        return;
    }
    fputc('"', out);
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:   fputc(*p, out); break;
        }
    }
    fputc('"', out);
}

static void
rir_dump_json_fact(FILE *out, const RIRFact *f)
{
    fprintf(out,
        "      {\"kind\":\"%s\",\"resource\":\"%s\",\"name\":",
        rir_fact_kind_name(f->kind),
        rir_resource_kind_name(f->resource_kind));
    json_write_str(out, f->name);
    fprintf(out, ",\"slot_anchor\":");
    json_write_str(out, f->slot_anchor);
    fprintf(out, ",\"arg0\":");
    json_write_str(out, f->arg0);
    fprintf(out, ",\"arg1\":");
    json_write_str(out, f->arg1);
    fprintf(out, ",\"state\":\"%s\"}",
        rir_resource_state_name(f->state));
}

static void
rir_dump_json_op(FILE *out, const RIROp *op)
{
    fprintf(out, "      {\"kind\":\"%s\",\"subject\":",
        rir_op_kind_name(op->kind));
    json_write_str(out, op->subject);
    fprintf(out, ",\"slot_anchor\":");
    json_write_str(out, op->slot_anchor);
    fprintf(out, ",\"arg0\":");
    json_write_str(out, op->arg0);
    fprintf(out, ",\"arg1\":");
    json_write_str(out, op->arg1);
    fputs("}", out);
}

static void
rir_dump_json_summary(FILE *out, const RIRStateSummary *s)
{
    fprintf(out, "      {\"name\":");
    json_write_str(out, s->name);
    fprintf(out, ",\"slot_anchor\":");
    json_write_str(out, s->slot_anchor);
    fprintf(out, ",\"kind\":\"%s\",\"resource\":\"%s\",",
        rir_fact_kind_name(s->origin_kind),
        rir_resource_kind_name(s->resource_kind));
    fprintf(out, "\"initial_state\":\"%s\",\"final_state\":\"%s\",",
        rir_resource_state_name(s->initial_state),
        rir_resource_state_name(s->final_state));
    fprintf(out, "\"last_op\":");
    json_write_str(out, s->last_op_name);
    fprintf(out, ",\"has_error\":%s}",
        s->has_transition_error ? "true" : "false");
}

void
rir_dump_json(const RIRProgram *rir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (rir == NULL) {
        fputs("{\"error\":\"RIR program is null\"}\n", out);
        return;
    }

    fputs("{\n  \"rir_version\": 1,\n", out);
    fprintf(out, "  \"scope_count\": %zu,\n", rir->scope_count);
    fputs("  \"scopes\": [\n", out);

    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        if (i > 0) fputs(",\n", out);

        fprintf(out,
            "    {\n"
            "      \"index\": %zu,\n"
            "      \"kind\": \"%s\",\n"
            "      \"name\": ",
            i, rir_scope_kind_name(scope->kind));
        json_write_str(out, scope->name);
        fprintf(out, ",\n"
                     "      \"owner\": ");
        json_write_str(out, scope->owner_name);
        fprintf(out, ",\n"
                     "      \"fact_count\": %zu,\n"
                     "      \"op_count\": %zu,\n"
                     "      \"facts\": [\n",
                scope->fact_count, scope->op_count);

        for (size_t j = 0; j < scope->fact_count; j++) {
            if (j > 0) fputs(",\n", out);
            rir_dump_json_fact(out, &scope->facts[j]);
        }

        fprintf(out, "\n      ],\n"
                     "      \"ops\": [\n");

        for (size_t j = 0; j < scope->op_count; j++) {
            if (j > 0) fputs(",\n", out);
            rir_dump_json_op(out, &scope->ops[j]);
        }

        fprintf(out, "\n      ],\n"
                     "      \"summaries\": [\n");

        for (size_t j = 0; j < scope->state_summary_count; j++) {
            if (j > 0) fputs(",\n", out);
            rir_dump_json_summary(out, &scope->state_summaries[j]);
        }

        fprintf(out, "\n      ],\n"
                     "      \"has_state_errors\": %s\n"
                     "    }",
                scope->has_state_errors ? "true" : "false");
    }

    fputs("\n  ]\n}\n", out);
}

void
rir_dump(const RIRProgram *rir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (rir == NULL) {
        fprintf(out, "RIR: (null)\n");
        return;
    }

    fprintf(out, "RIR Program\n  scopes: %zu\n", rir->scope_count);
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        fprintf(out, "  scope[%02zu] %-8s %s%s%s facts=%zu ops=%zu\n",
                i,
                rir_scope_kind_name(scope->kind),
                scope->owner_name != NULL ? scope->owner_name : "",
                scope->owner_name != NULL ? "." : "",
                scope->name != NULL ? scope->name : "(anonymous)",
                scope->fact_count,
                scope->op_count);
        fprintf(out, "    normalize summaries=%zu state-errors=%s semantics=",
                scope->state_summary_count,
                scope->has_state_errors ? "yes" : "no");
        rir_dump_flow_semantics(out, scope->conservative_semantics);
        fputc('\n', out);
        for (size_t j = 0; j < scope->fact_count; j++) {
            const RIRFact *fact = &scope->facts[j];
            fprintf(out, "    fact[%02zu] %-13s name=%s slot=%s arg0=%s arg1=%s kind=%s state=%s\n",
                    j,
                    rir_fact_kind_name(fact->kind),
                    fact->name != NULL ? fact->name : "-",
                    fact->slot_anchor != NULL ? fact->slot_anchor : "-",
                    fact->arg0 != NULL ? fact->arg0 : "-",
                    fact->arg1 != NULL ? fact->arg1 : "-",
                    rir_resource_kind_name(fact->resource_kind),
                    rir_resource_state_name(fact->state));
        }
        for (size_t j = 0; j < scope->op_count; j++) {
            const RIROp *op = &scope->ops[j];
            fprintf(out, "    op[%02zu] %-20s subject=%s slot=%s arg0=%s arg1=%s\n",
                    j,
                    rir_op_kind_name(op->kind),
                    op->subject != NULL ? op->subject : "-",
                    op->slot_anchor != NULL ? op->slot_anchor : "-",
                    op->arg0 != NULL ? op->arg0 : "-",
                    op->arg1 != NULL ? op->arg1 : "-");
        }
        for (size_t j = 0; j < scope->state_summary_count; j++) {
            const RIRStateSummary *summary = &scope->state_summaries[j];
            fprintf(out,
                    "    state[%02zu] %-13s name=%s slot=%s kind=%s init=%s final=%s last-op=%s error=%s\n",
                    j,
                    rir_fact_kind_name(summary->origin_kind),
                    summary->name != NULL ? summary->name : "-",
                    summary->slot_anchor != NULL ? summary->slot_anchor : "-",
                    rir_resource_kind_name(summary->resource_kind),
                    rir_resource_state_name(summary->initial_state),
                    rir_resource_state_name(summary->final_state),
                    summary->last_op_name != NULL ? summary->last_op_name : "-",
                    summary->has_transition_error ? "yes" : "no");
        }
        for (size_t j = 0; j < scope->flow_block_count; j++) {
            const RIRFlowBlock *block = &scope->flow_blocks[j];
            fprintf(out,
                    "    flow-block[%02zu] reachable=%s join=%s facts=%zu sem-entry=",
                    block->block_id,
                    block->is_reachable ? "yes" : "no",
                    block->is_join ? "yes" : "no",
                    block->fact_count);
            rir_dump_flow_semantics(out, block->entry_semantics);
            fputs(" sem-exit=", out);
            rir_dump_flow_semantics(out, block->exit_semantics);
            fputc('\n', out);
            for (size_t k = 0; k < block->fact_count; k++) {
                const RIRFlowFact *fact = &block->facts[k];
                fprintf(out,
                        "      flow[%02zu] name=%s slot=%s entry=%s exit=%s join=%s widened=%s entry-conflict=%s exit-conflict=%s\n",
                        k,
                        fact->name != NULL ? fact->name : "-",
                        fact->slot_anchor != NULL ? fact->slot_anchor : "-",
                        rir_resource_state_name(fact->entry_state),
                        rir_resource_state_name(fact->exit_state),
                        fact->merged_from_join ? "yes" : "no",
                        fact->widened_by_loop ? "yes" : "no",
                        fact->entry_conflict ? "yes" : "no",
                        fact->has_merge_conflict ? "yes" : "no");
            }
        }
    }
}
