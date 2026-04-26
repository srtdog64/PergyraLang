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

static const RIRFact *
rir_scope_find_projection_fact(const RIRScope *scope, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_PROJECTION
            && fact->name != NULL
            && strcmp(fact->name, name) == 0) {
            return fact;
        }
    }

    return NULL;
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

static RIRResourceKind
rir_direct_projection_kind_from_ast(const ASTNode *ast)
{
    if (ast == NULL || ast->type != AST_CALL
        || ast->data.call.callee == NULL
        || ast->data.call.callee->type != AST_IDENTIFIER
        || ast->data.call.callee->data.identifier.name == NULL) {
        return RIR_RESOURCE_UNKNOWN;
    }

    if (strcmp(ast->data.call.callee->data.identifier.name, "ToObject") == 0)
        return RIR_RESOURCE_PROJECTION_OBJECT;
    if (strcmp(ast->data.call.callee->data.identifier.name, "ToTObject") == 0)
        return RIR_RESOURCE_PROJECTION_TOBJECT;
    return RIR_RESOURCE_UNKNOWN;
}

RIRResourceState
rir_merge_state_for_kind(RIRResourceKind kind,
                         RIRResourceState a,
                         RIRResourceState b,
                         bool *conflict)
{
    if (conflict != NULL)
        *conflict = false;
    return rir_merge_states_for_kind(kind, a, b, conflict);
}

bool
rir_validate(const RIRProgram *rir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("RIR program is null");
        return false;
    }

    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        for (size_t j = 0; j < scope->state_summary_count; j++) {
            const RIRStateSummary *summary = &scope->state_summaries[j];
            if (summary->name == NULL || summary->resource_kind == RIR_RESOURCE_UNKNOWN) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has incomplete state summary[%llu]",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        (unsigned long long) j);
                }
                return false;
            }
            if (summary->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' state summary '%s' is missing slot anchor",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        summary->name != NULL ? summary->name : "(unnamed)");
                }
                return false;
            }
        }

        for (size_t j = 0; j < scope->fact_count; j++) {
            const RIRFact *fact = &scope->facts[j];
            if (fact->kind != RIR_FACT_INTENT_POLICY && fact->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' fact '%s' is missing slot anchor",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        fact->name != NULL ? fact->name : "(unnamed)");
                }
                return false;
            }
            if (fact->kind == RIR_FACT_PROJECTION) {
                if (fact->resource_kind == RIR_RESOURCE_PROJECTION_OBJECT
                    && fact->state == RIR_STATE_PUBLISHED) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' object projection '%s' cannot be Published.\n"
                            "Reason:\n"
                            "- object projections are local refresh targets\n"
                            "- published state is reserved for boundary tobject projections\n"
                            "Fix:\n"
                            "- keep '%s' in refresh/bind flow\n"
                            "- or change it to a tobject projection before publish",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            fact->name != NULL ? fact->name : "(unnamed)",
                            fact->name != NULL ? fact->name : "(unnamed)");
                    }
                    return false;
                }
                if (fact->resource_kind == RIR_RESOURCE_PROJECTION_TOBJECT
                    && strcmp(fact->arg1 != NULL ? fact->arg1 : "", "refresh") == 0) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' tobject projection '%s' cannot use refresh mode.\n"
                            "Reason:\n"
                            "- tobject projections are boundary publish targets\n"
                            "- refresh mode is only valid for local object projections\n"
                            "Fix:\n"
                            "- use publish for '%s'\n"
                            "- or switch the target declaration to object",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            fact->name != NULL ? fact->name : "(unnamed)",
                            fact->name != NULL ? fact->name : "(unnamed)");
                    }
                    return false;
                }
            }
        }

        for (size_t j = 0; j < scope->op_count; j++) {
            const RIROp *op = &scope->ops[j];
            if (op->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' op '%s' is missing slot anchor",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        rir_op_kind_name(op->kind));
                }
                return false;
            }
            if (op->kind == RIR_OP_PROJECT_PUBLISH || op->kind == RIR_OP_PROJECT_REFRESH) {
                const RIRFact *projection = rir_scope_find_projection_fact(scope, op->slot_anchor);
                const RIRStateSummary *summary = scope_find_state_summary(
                    (RIRScope *)scope, op->slot_anchor);
                RIRResourceKind kind = RIR_RESOURCE_UNKNOWN;

                if (projection != NULL)
                    kind = projection->resource_kind;
                else if (summary != NULL)
                    kind = summary->resource_kind;
                else
                    kind = rir_direct_projection_kind_from_ast(op->ast);

                if (kind == RIR_RESOURCE_UNKNOWN) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' %s on '%s' has unknown projection kind.\n"
                            "Reason:\n"
                            "- the projection op reached validation without a projection fact or state summary\n"
                            "- backend lowering would otherwise guess the projection contract\n"
                            "Fix:\n"
                            "- materialize projection facts before validation\n"
                            "- or ensure DIR projection-slot edges are lowered into RIR summaries",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            op->kind == RIR_OP_PROJECT_PUBLISH ? "ProjectPublish" : "ProjectRefresh",
                            op->slot_anchor);
                    }
                    return false;
                }

                if (op->kind == RIR_OP_PROJECT_PUBLISH
                    && kind != RIR_RESOURCE_PROJECTION_TOBJECT
                    && kind != RIR_RESOURCE_TOBJECT_SLOT) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' ProjectPublish requires tobject projection slot '%s'.\n"
                            "Reason:\n"
                            "- publish emits a boundary transfer snapshot\n"
                            "- object/local projection slots cannot satisfy publish semantics\n"
                            "Fix:\n"
                            "- target a tobject projection slot\n"
                            "- or use ProjectRefresh for local object projection sync",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            op->slot_anchor);
                    }
                    return false;
                }
                if (op->kind == RIR_OP_PROJECT_REFRESH
                    && kind != RIR_RESOURCE_PROJECTION_OBJECT
                    && kind != RIR_RESOURCE_OBJECT_SLOT) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' ProjectRefresh requires object projection slot '%s'.\n"
                            "Reason:\n"
                            "- refresh updates a local object projection from a source anchor\n"
                            "- tobject/boundary slots require publish semantics instead\n"
                            "Fix:\n"
                            "- target an object projection slot\n"
                            "- or switch this op to ProjectPublish for tobject flow",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            op->slot_anchor);
                    }
                    return false;
                }
            }
        }

        for (size_t j = 0; j < scope->flow_block_count; j++) {
            const RIRFlowBlock *block = &scope->flow_blocks[j];
            if (block->fact_count > 0 && block->facts == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' flow-block[%llu] has missing fact storage",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        (unsigned long long) block->block_id);
                }
                return false;
            }
            for (size_t k = 0; k < block->fact_count; k++) {
                if (block->facts[k].name == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%llu] has unnamed fact",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            (unsigned long long) block->block_id);
                    }
                    return false;
                }
                if (block->facts[k].slot_anchor == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%llu] fact '%s' is missing slot anchor",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            (unsigned long long) block->block_id,
                            block->facts[k].name != NULL ? block->facts[k].name : "(unnamed)");
                    }
                    return false;
                }
            }
        }

        if (scope->kind == RIR_SCOPE_INTENT) {
            bool has_commit = false;
            bool has_abort = false;
            for (size_t j = 0; j < scope->op_count; j++) {
                has_commit = has_commit || scope->ops[j].kind == RIR_OP_COMMIT_INTENT;
                has_abort = has_abort || scope->ops[j].kind == RIR_OP_ABORT_INTENT;
            }
            if (!has_commit || !has_abort) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR intent scope '%s' is missing commit/abort structure",
                        scope->name != NULL ? scope->name : "(anonymous)");
                }
                return false;
            }
        }
    }

    return true;
}

static bool
rir_split_qualified_name(const char *qualified_name,
                         const char **owner_name,
                         size_t *owner_len,
                         const char **local_name)
{
    const char *dot;

    if (owner_name != NULL)
        *owner_name = NULL;
    if (owner_len != NULL)
        *owner_len = 0;
    if (local_name != NULL)
        *local_name = NULL;

    if (qualified_name == NULL)
        return false;

    dot = strrchr(qualified_name, '.');
    if (dot == NULL || dot == qualified_name || dot[1] == '\0')
        return false;

    if (owner_name != NULL)
        *owner_name = qualified_name;
    if (owner_len != NULL)
        *owner_len = (size_t)(dot - qualified_name);
    if (local_name != NULL)
        *local_name = dot + 1;
    return true;
}

static bool
rir_scope_name_matches(const RIRScope *scope, const char *owner_name, size_t owner_len)
{
    return scope != NULL
        && scope->name != NULL
        && owner_name != NULL
        && strlen(scope->name) == owner_len
        && strncmp(scope->name, owner_name, owner_len) == 0;
}

static const RIRFact *
rir_scope_find_fact_by_name_kind(const RIRScope *scope, RIRFactKind kind, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < scope->fact_count; i++) {
        if (scope->facts[i].kind == kind
            && scope->facts[i].name != NULL
            && strcmp(scope->facts[i].name, name) == 0) {
            return &scope->facts[i];
        }
    }

    return NULL;
}

static bool
rir_scope_has_capability_fact(const RIRScope *scope, const char *participant, const char *ability)
{
    if (scope == NULL || participant == NULL || ability == NULL)
        return false;

    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_CAPABILITY
            && fact->name != NULL
            && strcmp(fact->name, participant) == 0
            && fact->arg0 != NULL
            && strcmp(fact->arg0, ability) == 0) {
            return true;
        }
    }

    return false;
}

static const RIRScope *
rir_find_domain_scope_for_owner(const RIRProgram *rir, const char *owner_name, size_t owner_len)
{
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        if ((scope->kind == RIR_SCOPE_ZONE
             || scope->kind == RIR_SCOPE_RELATION
             || scope->kind == RIR_SCOPE_EFFECT)
            && rir_scope_name_matches(scope, owner_name, owner_len)) {
            return scope;
        }
    }

    return NULL;
}

bool
rir_validate_against_dir(const RIRProgram *rir, const DIRProgram *dir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL || dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("RIR/DIR validation requires both programs");
        return false;
    }

    for (size_t i = 0; i < dir->node_count; i++) {
        const DIRNode *node = &dir->nodes[i];
        const char *owner_name = NULL;
        const char *local_name = NULL;
        size_t owner_len = 0;
        const RIRScope *scope;
        const RIRFact *fact;
        const RIRStateSummary *summary;
        RIRResourceKind expected_kind = RIR_RESOURCE_UNKNOWN;

        if (node->kind != DIR_NODE_ZONE_SLOT
            && node->kind != DIR_NODE_PROJECTION_SLOT
            && node->kind != DIR_NODE_AUTHORITY_SLOT) {
            continue;
        }

        if (!rir_split_qualified_name(node->name, &owner_name, &owner_len, &local_name)) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "DIR slot-contract node '%s' is not owner-qualified",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        scope = rir_find_domain_scope_for_owner(rir, owner_name, owner_len);
        if (scope == NULL) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "DIR slot-contract '%s' has no matching RIR domain scope",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        if (node->kind == DIR_NODE_AUTHORITY_SLOT) {
            fact = rir_scope_find_fact_by_name_kind(scope, RIR_FACT_AUTHORITY, local_name);
            if (fact == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' is missing authority fact for DIR authority-slot '%s'",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        node->name != NULL ? node->name : "(unnamed)");
                }
                return false;
            }

            for (size_t edge_i = 0; edge_i < dir->edge_count; edge_i++) {
                const DIREdge *edge = &dir->edges[edge_i];
                if (edge->kind != DIR_EDGE_ZONE_AUTHORITY_ABILITY
                    || edge->from_node_id != node->id
                    || edge->target_name == NULL) {
                    continue;
                }
                if (!rir_scope_has_capability_fact(scope, local_name, edge->target_name)) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' is missing capability fact '%s' for DIR authority-slot '%s'",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            edge->target_name,
                            node->name != NULL ? node->name : "(unnamed)");
                    }
                    return false;
                }
            }
            continue;
        }

        fact = rir_scope_find_fact_by_name_kind(scope, RIR_FACT_RESOURCE, local_name);
        summary = scope_find_state_summary((RIRScope *)scope, local_name);

        if (node->ast != NULL && node->ast->type == AST_DOMAIN_SLOT) {
            if (node->ast->data.domain_slot.is_subject)
                expected_kind = RIR_RESOURCE_SUBJECT_SLOT;
            else if (node->ast->data.domain_slot.is_vessel)
                expected_kind = RIR_RESOURCE_VESSEL_SLOT;
            else if (node->ast->data.domain_slot.is_tobject)
                expected_kind = RIR_RESOURCE_TOBJECT_SLOT;
            else
                expected_kind = RIR_RESOURCE_OBJECT_SLOT;
        }

        if (fact == NULL || summary == NULL) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "RIR scope '%s' is missing resource fact/state for DIR slot-contract '%s'",
                    scope->name != NULL ? scope->name : "(anonymous)",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        if (expected_kind != RIR_RESOURCE_UNKNOWN
            && (fact->resource_kind != expected_kind || summary->resource_kind != expected_kind)) {
            if (error_message != NULL) {
                *error_message = rir_strdup_fmt(
                    "RIR scope '%s' kind mismatch for DIR slot-contract '%s'",
                    scope->name != NULL ? scope->name : "(anonymous)",
                    node->name != NULL ? node->name : "(unnamed)");
            }
            return false;
        }

        if (node->kind == DIR_NODE_PROJECTION_SLOT) {
            const RIRFact *projection = rir_scope_find_projection_fact(scope, local_name);
            bool has_projection_source = false;
            for (size_t edge_i = 0; edge_i < dir->edge_count; edge_i++) {
                const DIREdge *edge = &dir->edges[edge_i];
                if (edge->kind == DIR_EDGE_PROJECTION_SLOT_SOURCE
                    && edge->from_node_id == node->id) {
                    has_projection_source = true;
                    break;
                }
            }
            if (has_projection_source && projection == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' is missing projection fact for DIR projection-slot '%s'.\n"
                        "Reason:\n"
                        "- DIR declared a projection slot/source edge for '%s'\n"
                        "- RIR lowering did not materialize the corresponding projection fact\n"
                        "Fix:\n"
                        "- lower DIR projection-slot contracts into RIR facts before validation\n"
                        "- or remove the stale DIR projection edge",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        node->name != NULL ? node->name : "(unnamed)",
                        node->name != NULL ? node->name : "(unnamed)");
                }
                return false;
            }
        }
    }

    return true;
}

/* =================================================================
 * JSON Export — RIR Facts for AI/External Consumption
 *
 * This function dumps the RIR program as structured JSON.
 * The output is consumed by:
 *   1. AI/MCP agents that map RIR Facts → MIR Instructions
 *   2. External tooling that validates resource ownership
 *   3. Debug/audit logs for intent orchestration
 *
 * Schema: see docs/39_test_driven_abi_and_explicit_lowering.md §2.2
 * ================================================================= */

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
