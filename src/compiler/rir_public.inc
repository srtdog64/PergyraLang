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
    }
}

void
rir_destroy(RIRProgram *rir)
{
    if (rir == NULL)
        return;
    for (size_t i = 0; i < rir->scope_count; i++) {
        free(rir->scopes[i].facts);
        free(rir->scopes[i].ops);
        free(rir->scopes[i].state_summaries);
        rir_free_flow_blocks(&rir->scopes[i]);
    }
    free(rir->scopes);
    free(rir);
}

const char *
rir_scope_kind_name(RIRScopeKind kind)
{
    switch (kind) {
        case RIR_SCOPE_FUNCTION: return "function";
        case RIR_SCOPE_METHOD: return "method";
        case RIR_SCOPE_INTENT: return "intent";
        case RIR_SCOPE_ZONE: return "zone";
        case RIR_SCOPE_RELATION: return "relation";
        case RIR_SCOPE_EFFECT: return "effect";
        case RIR_SCOPE_WORLD: return "world";
        default: return "unknown";
    }
}

const char *
rir_fact_kind_name(RIRFactKind kind)
{
    switch (kind) {
        case RIR_FACT_RESOURCE: return "resource";
        case RIR_FACT_PROJECTION: return "projection";
        case RIR_FACT_AUTHORITY: return "authority";
        case RIR_FACT_CAPABILITY: return "capability";
        case RIR_FACT_INTENT_POLICY: return "intent-policy";
        default: return "unknown";
    }
}

const char *
rir_resource_kind_name(RIRResourceKind kind)
{
    switch (kind) {
        case RIR_RESOURCE_UNKNOWN: return "unknown";
        case RIR_RESOURCE_LOCAL_SLOT: return "LocalSlot";
        case RIR_RESOURCE_SECURE_SLOT: return "SecureSlot";
        case RIR_RESOURCE_DEVICE_SLOT: return "DeviceSlot";
        case RIR_RESOURCE_QUBIT_HANDLE: return "QubitHandle";
        case RIR_RESOURCE_REMOTE_FUTURE_HANDLE: return "RemoteFutureHandle";
        case RIR_RESOURCE_PROJECTION_OBJECT: return "ProjectionObject";
        case RIR_RESOURCE_PROJECTION_DTO: return "ProjectionDto";
        case RIR_RESOURCE_EFFECT_INSTANCE: return "EffectInstance";
        case RIR_RESOURCE_RELATION_INSTANCE: return "RelationInstance";
        case RIR_RESOURCE_ZONE_HANDLE: return "ZoneHandle";
        case RIR_RESOURCE_WORLD_HANDLE: return "WorldHandle";
        default: return "unknown";
    }
}

const char *
rir_resource_state_name(RIRResourceState state)
{
    switch (state) {
        case RIR_STATE_UNINIT: return "Uninit";
        case RIR_STATE_OWNED: return "Owned";
        case RIR_STATE_BORROWED_READ: return "BorrowedRead";
        case RIR_STATE_BORROWED_WRITE: return "BorrowedWrite";
        case RIR_STATE_MOVED: return "Moved";
        case RIR_STATE_RELEASED: return "Released";
        case RIR_STATE_INVALID: return "Invalid";
        case RIR_STATE_MEASURED: return "Measured";
        case RIR_STATE_REMOTE_PENDING: return "RemotePending";
        case RIR_STATE_SYNCED: return "Synced";
        case RIR_STATE_DIRTY: return "Dirty";
        case RIR_STATE_DETACHED: return "Detached";
        case RIR_STATE_PUBLISHED: return "Published";
        default: return "unknown";
    }
}

const char *
rir_op_kind_name(RIROpKind kind)
{
    switch (kind) {
        case RIR_OP_CLAIM: return "Claim";
        case RIR_OP_READ: return "Read";
        case RIR_OP_WRITE: return "Write";
        case RIR_OP_RELEASE: return "Release";
        case RIR_OP_MOVE: return "Move";
        case RIR_OP_BORROW_READ: return "BorrowRead";
        case RIR_OP_BORROW_WRITE: return "BorrowWrite";
        case RIR_OP_PROJECT_REFRESH: return "ProjectRefresh";
        case RIR_OP_PROJECT_PUBLISH: return "ProjectPublish";
        case RIR_OP_ATTACH_EFFECT: return "AttachEffect";
        case RIR_OP_DETACH_EFFECT: return "DetachEffect";
        case RIR_OP_LINK_RELATION: return "LinkRelation";
        case RIR_OP_UNLINK_RELATION: return "UnlinkRelation";
        case RIR_OP_AUTHORIZE: return "Authorize";
        case RIR_OP_AWAIT_REMOTE: return "AwaitRemote";
        case RIR_OP_COMMIT_INTENT: return "CommitIntent";
        case RIR_OP_ABORT_INTENT: return "AbortIntent";
        case RIR_OP_COMPENSATE_INTENT_STEP: return "CompensateIntentStep";
        default: return "unknown";
    }
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
                        "RIR scope '%s' has incomplete state summary[%zu]",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        j);
                }
                return false;
            }
        }

        for (size_t j = 0; j < scope->flow_block_count; j++) {
            const RIRFlowBlock *block = &scope->flow_blocks[j];
            if (block->fact_count > 0 && block->facts == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' flow-block[%zu] has missing fact storage",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        block->block_id);
                }
                return false;
            }
            for (size_t k = 0; k < block->fact_count; k++) {
                if (block->facts[k].name == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%zu] has unnamed fact",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            block->block_id);
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
            fprintf(out, "    fact[%02zu] %-13s name=%s arg0=%s arg1=%s kind=%s state=%s\n",
                    j,
                    rir_fact_kind_name(fact->kind),
                    fact->name != NULL ? fact->name : "-",
                    fact->arg0 != NULL ? fact->arg0 : "-",
                    fact->arg1 != NULL ? fact->arg1 : "-",
                    rir_resource_kind_name(fact->resource_kind),
                    rir_resource_state_name(fact->state));
        }
        for (size_t j = 0; j < scope->op_count; j++) {
            const RIROp *op = &scope->ops[j];
            fprintf(out, "    op[%02zu] %-20s subject=%s arg0=%s arg1=%s\n",
                    j,
                    rir_op_kind_name(op->kind),
                    op->subject != NULL ? op->subject : "-",
                    op->arg0 != NULL ? op->arg0 : "-",
                    op->arg1 != NULL ? op->arg1 : "-");
        }
        for (size_t j = 0; j < scope->state_summary_count; j++) {
            const RIRStateSummary *summary = &scope->state_summaries[j];
            fprintf(out,
                    "    state[%02zu] %-13s name=%s kind=%s init=%s final=%s last-op=%s error=%s\n",
                    j,
                    rir_fact_kind_name(summary->origin_kind),
                    summary->name != NULL ? summary->name : "-",
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
                        "      flow[%02zu] name=%s entry=%s exit=%s join=%s widened=%s entry-conflict=%s exit-conflict=%s\n",
                        k,
                        fact->name != NULL ? fact->name : "-",
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
