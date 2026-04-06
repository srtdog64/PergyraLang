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
        case RIR_RESOURCE_AUTHORITY_HANDLE: return "AuthorityHandle";
        case RIR_RESOURCE_CAPABILITY_TOKEN: return "CapabilityToken";
        case RIR_RESOURCE_SUBJECT_SLOT: return "SubjectSlot";
        case RIR_RESOURCE_OBJECT_SLOT: return "ObjectSlot";
        case RIR_RESOURCE_TOBJECT_SLOT: return "TObjectSlot";
        case RIR_RESOURCE_VESSEL_SLOT: return "VesselSlot";
        case RIR_RESOURCE_QUBIT_HANDLE: return "QubitHandle";
        case RIR_RESOURCE_REMOTE_FUTURE_HANDLE: return "RemoteFutureHandle";
        case RIR_RESOURCE_PROJECTION_OBJECT: return "ProjectionObject";
        case RIR_RESOURCE_PROJECTION_TOBJECT: return "ProjectionTObject";
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
        case RIR_STATE_AUTHORIZED: return "Authorized";
        case RIR_STATE_AUTHORITY_LOST: return "AuthorityLost";
        case RIR_STATE_SYNCED: return "Synced";
        case RIR_STATE_DIRTY: return "Dirty";
        case RIR_STATE_STALE: return "Stale";
        case RIR_STATE_DETACHED: return "Detached";
        case RIR_STATE_PUBLISHED: return "Published";
        case RIR_STATE_HANDOFF_PENDING: return "HandoffPending";
        case RIR_STATE_HANDED_OFF: return "HandedOff";
        case RIR_STATE_COMPENSATED: return "Compensated";
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
                            "RIR scope '%s' object projection '%s' cannot be Published",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            fact->name != NULL ? fact->name : "(unnamed)");
                    }
                    return false;
                }
                if (fact->resource_kind == RIR_RESOURCE_PROJECTION_TOBJECT
                    && strcmp(fact->arg1 != NULL ? fact->arg1 : "", "refresh") == 0) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' tobject projection '%s' cannot use refresh mode",
                            scope->name != NULL ? scope->name : "(anonymous)",
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

                if (kind == RIR_RESOURCE_UNKNOWN) {
                    continue;
                }

                if (op->kind == RIR_OP_PROJECT_PUBLISH
                    && kind != RIR_RESOURCE_PROJECTION_TOBJECT
                    && kind != RIR_RESOURCE_TOBJECT_SLOT) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' ProjectPublish requires tobject projection slot '%s'",
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
                            "RIR scope '%s' ProjectRefresh requires object projection slot '%s'",
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
                if (block->facts[k].slot_anchor == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%zu] fact '%s' is missing slot anchor",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            block->block_id,
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
rir_scope_has_capability_fact(const RIRScope *scope, const char *actor, const char *ability)
{
    if (scope == NULL || actor == NULL || ability == NULL)
        return false;

    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->kind == RIR_FACT_CAPABILITY
            && fact->name != NULL
            && strcmp(fact->name, actor) == 0
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
            else if (node->ast->data.domain_slot.is_dto)
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
                        "RIR scope '%s' is missing projection fact for DIR projection-slot '%s'",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        node->name != NULL ? node->name : "(unnamed)");
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
