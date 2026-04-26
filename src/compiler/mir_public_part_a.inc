static bool
mir_append_decl_header(MIRProgram *mir, MIRDeclHeader header)
{
    MIRDeclHeader *grown;

    if (mir == NULL)
        return false;
    grown = realloc(mir->decl_headers,
                    (mir->decl_header_count + 1) * sizeof(MIRDeclHeader));
    if (grown == NULL)
        return false;
    mir->decl_headers = grown;
    mir->decl_headers[mir->decl_header_count++] = header;
    return true;
}

static bool
mir_decl_header_set_methods(MIRDeclHeader *header,
                            ASTNode **methods,
                            size_t method_count)
{
    if (header == NULL)
        return false;

    header->methods = methods;
    header->method_count = method_count;
    header->method_metadata = NULL;
    header->method_metadata_count = 0;

    if (method_count == 0)
        return true;

    header->method_metadata = calloc(method_count, sizeof(MIRDeclMethod));
    if (header->method_metadata == NULL)
        return false;

    for (size_t i = 0; i < method_count; i++) {
        ASTNode *method = methods != NULL ? methods[i] : NULL;
        MIRDeclMethod *meta = &header->method_metadata[i];
        meta->ast = method;
        meta->owner_name = header->name;
        if (method != NULL && method->type == AST_FUNC_DECL) {
            meta->name = method->data.func_decl.name;
            meta->params = method->data.func_decl.params;
            meta->param_count = method->data.func_decl.param_count;
            meta->return_type = method->data.func_decl.return_type;
            meta->is_action_like = method->data.func_decl.is_action;
            meta->within_zone = method->data.func_decl.within_zone;
        }
    }
    header->method_metadata_count = method_count;
    return true;
}

static bool
mir_record_decl_header(MIRProgram *mir, ASTNode *decl)
{
    MIRDeclHeader header;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (mir == NULL || decl == NULL)
        return true;

    memset(&header, 0, sizeof(header));
    header.ast = decl;
    header.ast_type = decl->type;

    switch (decl->type) {
    case AST_CLASS_DECL:
        header.name = decl->data.class_decl.name;
        methods = decl->data.class_decl.methods;
        method_count = decl->data.class_decl.method_count;
        header.uses_pointer_self =
            decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL;
        break;
    case AST_ENUM_DECL:
        header.name = decl->data.enum_decl.name;
        methods = decl->data.enum_decl.methods;
        method_count = decl->data.enum_decl.method_count;
        break;
    case AST_PARTY_DECL:
        header.name = decl->data.party_decl.name;
        methods = decl->data.party_decl.methods;
        method_count = decl->data.party_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_ROSTER_DECL:
        header.name = decl->data.roster_decl.name;
        header.uses_pointer_self = true;
        break;
    case AST_WORLD_DECL:
        header.name = decl->data.world_decl.name;
        methods = decl->data.world_decl.methods;
        method_count = decl->data.world_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_RELATION_DECL:
        header.name = decl->data.relation_decl.name;
        methods = decl->data.relation_decl.methods;
        method_count = decl->data.relation_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_EFFECT_DECL:
        header.name = decl->data.effect_decl.name;
        methods = decl->data.effect_decl.methods;
        method_count = decl->data.effect_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_ZONE_DECL:
        header.name = decl->data.zone_decl.name;
        methods = decl->data.zone_decl.methods;
        method_count = decl->data.zone_decl.method_count;
        header.uses_pointer_self = true;
        break;
    default:
        return true;
    }

    if (header.name == NULL)
        return true;
    if (!mir_decl_header_set_methods(&header, methods, method_count))
        return false;
    if (!mir_append_decl_header(mir, header)) {
        free(header.method_metadata);
        return false;
    }
    return true;
}

static void
mir_link_decl_method_routines(MIRProgram *mir)
{
    if (mir == NULL)
        return;

    for (size_t hi = 0; hi < mir->decl_header_count; hi++) {
        MIRDeclHeader *header = &mir->decl_headers[hi];
        for (size_t mi = 0; mi < header->method_metadata_count; mi++) {
            MIRDeclMethod *method = &header->method_metadata[mi];
            method->has_routine = false;
            method->routine_index = 0;
            if (method->name == NULL)
                continue;

            for (size_t ri = 0; ri < mir->routine_count; ri++) {
                const MIRRoutine *routine = &mir->routines[ri];
                if (routine->kind != MIR_SCOPE_METHOD)
                    continue;
                if (routine->ast == method->ast) {
                    method->has_routine = true;
                    method->routine_index = ri;
                    break;
                }
                if (routine->name == NULL
                    || strcmp(routine->name, method->name) != 0) {
                    continue;
                }
                if (method->owner_name != NULL
                    && routine->owner_name != NULL
                    && strcmp(routine->owner_name, method->owner_name) != 0) {
                    continue;
                }
                method->has_routine = true;
                method->routine_index = ri;
                break;
            }
        }
    }
}

MIRProgram *
mir_lower(const HIRProgram *hir, const RIRProgram *rir, char **error_message)
{
    const char *debug_mir_lower;
    MIRProgram *mir;
    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR lowering requires HIR");
        return NULL;
    }

    debug_mir_lower = getenv("PGY_DEBUG_MIR_LOWER");

    /* Initialize ABI type lookup table */
    mir_abi_table_init();

    mir = calloc(1, sizeof(MIRProgram));
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return NULL;
    }

#define MIR_COPY_AST_LIST(field, count_field) \
    do { \
        mir->count_field = hir->count_field; \
        if (hir->count_field > 0) { \
            mir->field = calloc(hir->count_field, sizeof(ASTNode *)); \
            if (mir->field == NULL) { \
                if (error_message != NULL) \
                    *error_message = pergyra_strdup("out of memory"); \
                mir_destroy(mir); \
                return NULL; \
            } \
            memcpy(mir->field, hir->field, hir->count_field * sizeof(ASTNode *)); \
        } \
    } while (0)

    MIR_COPY_AST_LIST(externs, extern_count);
    MIR_COPY_AST_LIST(types, type_count);
    MIR_COPY_AST_LIST(abilities, ability_count);
    MIR_COPY_AST_LIST(roles, role_count);
    MIR_COPY_AST_LIST(parties, party_count);
    MIR_COPY_AST_LIST(rosters, roster_count);
    MIR_COPY_AST_LIST(worlds, world_count);
    MIR_COPY_AST_LIST(relations, relation_count);
    MIR_COPY_AST_LIST(effects, effect_count);
    MIR_COPY_AST_LIST(zones, zone_count);
    MIR_COPY_AST_LIST(events, event_count);
    MIR_COPY_AST_LIST(intents, intent_count);
    MIR_COPY_AST_LIST(functions, function_count);
    mir->has_top_level_exec = false;
    mir->has_main_function = false;
    for (size_t i = 0; i < mir->function_count; i++) {
        ASTNode *fn = mir->functions[i];
        if (fn == NULL || fn->type != AST_FUNC_DECL
            || fn->data.func_decl.name == NULL) {
            continue;
        }
        if (strcmp(fn->data.func_decl.name, "__pgy_top_level_exec") == 0) {
            mir->has_top_level_exec = true;
        }
        if (strcmp(fn->data.func_decl.name, "Main") == 0) {
            mir->has_main_function = true;
        }
    }

#undef MIR_COPY_AST_LIST

    for (size_t i = 0; i < hir->type_count; i++) {
        if (!mir_record_decl_header(mir, hir->types[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->party_count; i++) {
        if (!mir_record_decl_header(mir, hir->parties[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->roster_count; i++) {
        if (!mir_record_decl_header(mir, hir->rosters[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->world_count; i++) {
        if (!mir_record_decl_header(mir, hir->worlds[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->relation_count; i++) {
        if (!mir_record_decl_header(mir, hir->relations[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->effect_count; i++) {
        if (!mir_record_decl_header(mir, hir->effects[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->zone_count; i++) {
        if (!mir_record_decl_header(mir, hir->zones[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *hir_routine = &hir->routines[i];
        MIRRoutine routine;
        const HIRBasicBlock *cfg_blocks_before = NULL;
        size_t cfg_block_count_before = 0;
        memset(&routine, 0, sizeof(routine));
        pgy_arena_init(&routine.scratch, 0);
        routine.id = mir->routine_count;
        routine.kind = mir_scope_kind_from_hir(hir_routine);
        routine.name = hir_routine->name;
        routine.ast = hir_routine->ast;
        routine.is_action_like = hir_routine->is_action_like;
        routine.hir_routine = hir_routine;
        routine.rir_scope = mir_find_matching_rir_scope(rir, hir_routine);
        routine.owner_name = routine.rir_scope != NULL
            ? routine.rir_scope->owner_name
            : hir_routine->owner_name;
        routine.owner_ast_type = hir_routine->owner_ast_type;
        cfg_blocks_before = hir_routine->has_cfg ? hir_routine->cfg.blocks : NULL;
        cfg_block_count_before = hir_routine->has_cfg ? hir_routine->cfg.block_count : 0;

        if (!mir_build_blocks_from_hir(&routine, hir_routine)
            || !mir_append_cleanup_block(&routine, routine.rir_scope)
            || !mir_populate_instructions(&routine)
            || !mir_apply_ssa_rename(&routine)
            || !mir_populate_use_edges(&routine)
            || !mir_populate_stmt_instructions(&routine)
            || !mir_materialize_cleanup_edges(&routine)
            || !mir_recompute_analysis(&routine)
            || !append_routine(mir, routine)) {
            /* On failure the stack routine never reaches mir->routines[],
             * so mir_destroy() will not see any scratch blocks the earlier
             * passes allocated.  Release them here to avoid a leak. */
            pgy_arena_destroy(&routine.scratch);
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }

        if (hir_routine->has_cfg
            && (hir_routine->cfg.blocks != cfg_blocks_before
                || hir_routine->cfg.block_count != cfg_block_count_before)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "HIR CFG pointer changed during MIR lowering for routine '%s' (before=%p/%zu after=%p/%zu)",
                    routine.name != NULL ? routine.name : "(anonymous)",
                    (void *)cfg_blocks_before,
                    cfg_block_count_before,
                    (void *)hir_routine->cfg.blocks,
                    hir_routine->cfg.block_count);
            }
            mir_destroy(mir);
            return NULL;
        }

        /* Optional MIR intent debug trace. Hidden by default because this
         * path runs during normal compilation and should not pollute stdout. */
        if (debug_mir_lower != NULL && debug_mir_lower[0] != '\0'
            && routine.kind == MIR_SCOPE_INTENT) {
            fprintf(stdout, "[MIR LOWER] Intent '%s' after build: has_cleanup=%d, blocks=%zu\n",
                routine.name ? routine.name : "(null)",
                routine.has_cleanup_block,
                routine.block_count);
            for (size_t b = 0; b < routine.block_count; b++) {
                fprintf(stdout, "  block[%zu] has_cleanup_succ=%d has_rollback_succ=%d has_invalidation_succ=%d\n",
                    b, routine.blocks[b].has_cleanup_succ, routine.blocks[b].has_rollback_succ, routine.blocks[b].has_invalidation_succ);
            }
        }
    }

    mir_link_decl_method_routines(mir);

    if (!mir_run_dce_pass(mir, error_message)) {
        mir_destroy(mir);
        return NULL;
    }

    return mir;
}

ASTNode *
mir_find_function_decl(const MIRProgram *mir, const char *name)
{
    if (mir == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < mir->function_count; i++) {
        ASTNode *fn = mir->functions[i];
        if (fn == NULL || fn->type != AST_FUNC_DECL
            || fn->data.func_decl.name == NULL) {
            continue;
        }
        if (strcmp(fn->data.func_decl.name, name) == 0)
            return fn;
    }

    return NULL;
}

const MIRDeclHeader *
mir_find_decl_header(const MIRProgram *mir, const char *name)
{
    if (mir == NULL || name == NULL)
        return NULL;
    for (size_t i = 0; i < mir->decl_header_count; i++) {
        const MIRDeclHeader *header = &mir->decl_headers[i];
        if (header->name != NULL && strcmp(header->name, name) == 0)
            return header;
    }
    return NULL;
}

bool
mir_run_liveness_pass(MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }
    for (size_t i = 0; i < mir->routine_count; i++) {
        if (!mir_recompute_analysis(&mir->routines[i])) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "failed to compute liveness for MIR routine '%s'",
                    mir->routines[i].name != NULL ? mir->routines[i].name : "(anonymous)");
            }
            return false;
        }
    }
    return true;
}

bool
mir_run_dce_pass(MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        MIRRoutine *routine = &mir->routines[i];
        bool changed = false;

        routine->dce_removed_count = 0;
        routine->has_dce = false;
        if (!mir_recompute_analysis(routine)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "failed to prepare MIR routine '%s' for DCE",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }

        do {
            changed = false;
            if (!mir_run_dce_on_routine(routine, &changed)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "failed to run DCE on MIR routine '%s'",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                return false;
            }
            if (changed && !mir_recompute_analysis(routine)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "failed to recompute MIR analysis after DCE for routine '%s'",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                return false;
            }
        } while (changed);

        routine->has_dce = true;
    }

    return true;
}

void
mir_destroy(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    for (size_t i = 0; i < mir->routine_count; i++) {
        MIRRoutine *routine = &mir->routines[i];
        for (size_t j = 0; j < routine->block_count; j++) {
            free(routine->blocks[j].predecessors);
            free(routine->blocks[j].source_statements);
            free((void *)routine->blocks[j].source_local_defs);
            free(routine->blocks[j].source_dom_tree_children);
            if (routine->blocks[j].source_phi_nodes != NULL) {
                for (size_t m = 0; m < routine->blocks[j].source_phi_node_count; m++)
                    free(routine->blocks[j].source_phi_nodes[m].incoming_predecessors);
            }
            free(routine->blocks[j].source_phi_nodes);
            for (size_t k = 0; k < routine->blocks[j].instruction_count; k++) {
                free((void *)routine->blocks[j].instructions[k].result_name);
                for (size_t m = 0; m < routine->blocks[j].instructions[k].use_count; m++)
                    free((void *)routine->blocks[j].instructions[k].uses[m]);
                free((void *)routine->blocks[j].instructions[k].uses);
                if (routine->blocks[j].instructions[k].phi_incomings != NULL) {
                    for (size_t m = 0; m < routine->blocks[j].instructions[k].phi_incoming_count; m++)
                        free((void *)routine->blocks[j].instructions[k].phi_incomings[m].value_name);
                }
                free(routine->blocks[j].instructions[k].phi_incomings);
            }
            for (size_t k = 0; k < routine->blocks[j].renamed_local_count; k++)
                free((void *)routine->blocks[j].renamed_locals[k]);
            free((void *)routine->blocks[j].renamed_locals);
            for (size_t k = 0; k < routine->blocks[j].ssa_entry_value_count; k++)
                free((void *)routine->blocks[j].ssa_entry_values[k]);
            free((void *)routine->blocks[j].ssa_entry_values);
            for (size_t k = 0; k < routine->blocks[j].ssa_exit_value_count; k++)
                free((void *)routine->blocks[j].ssa_exit_values[k]);
            free((void *)routine->blocks[j].ssa_exit_values);
            free((void *)routine->blocks[j].use_names);
            free((void *)routine->blocks[j].def_names);
            free((void *)routine->blocks[j].live_in_names);
            free((void *)routine->blocks[j].live_out_names);
            free(routine->blocks[j].ssa_entry_versions);
            free(routine->blocks[j].ssa_exit_versions);
            free(routine->blocks[j].instructions);
        }
        for (size_t j = 0; j < routine->value_summary_count; j++)
            free((void *)routine->value_summaries[j].name);
        free(routine->value_summaries);
        free(routine->blocks);
        pgy_arena_destroy(&routine->scratch);
    }
    free(mir->externs);
    for (size_t i = 0; i < mir->decl_header_count; i++)
        free(mir->decl_headers[i].method_metadata);
    free(mir->decl_headers);
    free(mir->types);
    free(mir->abilities);
    free(mir->roles);
    free(mir->parties);
    free(mir->rosters);
    free(mir->worlds);
    free(mir->relations);
    free(mir->effects);
    free(mir->zones);
    free(mir->events);
    free(mir->intents);
    free(mir->functions);
    free(mir->routines);
    free(mir);
}

static bool
mir_validate_cfg_contract_state(const MIRRoutine *routine,
                               bool require_cleanup,
                               bool require_cleanup_source_mapping,
                               bool require_mapping_for_all_blocks,
                               char **error_message)
{
    bool *hir_block_seen = NULL;
    const size_t cfg_block_count = (routine != NULL && routine->hir_routine != NULL
                                   && routine->hir_routine->has_cfg)
                                    ? routine->hir_routine->cfg.block_count
                                    : 0;

    if (routine == NULL)
        return false;

    if (routine->blocks == NULL || routine->block_count == 0) {
        return false;
    }

    if (routine->entry_block >= routine->block_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has invalid entry block",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }

    if (routine->has_cleanup_block && routine->cleanup_block >= routine->block_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has invalid cleanup block",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (routine->has_rollback_block && routine->rollback_block >= routine->block_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has invalid rollback block",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (routine->has_invalidation_block && routine->invalidation_block >= routine->block_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has invalid invalidation block",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (!routine->has_cleanup_block && routine->has_rollback_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has rollback block without cleanup root",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (!routine->has_cleanup_block && routine->has_invalidation_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' has invalidation block without cleanup root",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }
    if (routine->has_rollback_block && routine->has_invalidation_block
        && routine->rollback_block == routine->invalidation_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' rollback and invalidation blocks must be distinct",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        return false;
    }

    if (routine->has_cleanup_block) {
        const MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
        if (!cleanup->is_cleanup) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block %zu is not marked as cleanup",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->cleanup_block);
            }
            return false;
        }
        if (cfg_block_count > 0 && routine->cleanup_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has invalid cleanup block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
    }
    if (routine->has_rollback_block) {
        const MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
        if (!rollback->is_cleanup) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' rollback block %zu is not marked as cleanup",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->rollback_block);
            }
            return false;
        }
    }
    if (routine->has_invalidation_block) {
        const MIRBasicBlock *invalidation = &routine->blocks[routine->invalidation_block];
        if (!invalidation->is_cleanup) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' invalidation block %zu is not marked as cleanup",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->invalidation_block);
            }
            return false;
        }
    }

    if (cfg_block_count > 0) {
        hir_block_seen = calloc(cfg_block_count, sizeof(bool));
        if (hir_block_seen == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            return false;
        }
    }

    if (require_cleanup && !routine->has_cleanup_block) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' requires cleanup block for exceptional flow",
                routine->name != NULL ? routine->name : "(anonymous)");
        }
        free(hir_block_seen);
        return false;
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        bool require_source_mapping = require_mapping_for_all_blocks
            ? (!block->is_cleanup)
            : (block->is_reachable && (!block->is_cleanup || require_cleanup_source_mapping));

        if (cfg_block_count > 0 && require_source_mapping) {
            size_t source_id = block->source_hir_block_id;
            if (source_id == SIZE_MAX || source_id >= cfg_block_count) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] uses invalid source_hir_block_id",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
            if (hir_block_seen[source_id]) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' source_hir_block_id collision on block[%zu] -> hir[%zu]",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i,
                        source_id);
                }
                free(hir_block_seen);
                return false;
            }
            hir_block_seen[source_id] = true;
        }

        if (block->is_reachable && !block->is_cleanup) {
            if (routine->has_cleanup_block) {
                if (!block->has_cleanup_succ) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] missing cleanup edge from reachable non-cleanup block",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i);
                    }
                    free(hir_block_seen);
                    return false;
                }
                if (block->cleanup_succ >= routine->block_count
                    || block->cleanup_succ != routine->cleanup_block) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] cleanup edge does not target cleanup block",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i);
                    }
                    free(hir_block_seen);
                    return false;
                }
            } else if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] has exceptional successor without cleanup root",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
        }

        if (!block->is_cleanup && block->has_rollback_succ && !routine->has_rollback_block) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has rollback successor but no rollback block",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            }
            free(hir_block_seen);
            return false;
        }
        if (!block->is_cleanup && block->has_invalidation_succ && !routine->has_invalidation_block) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has invalidation successor but no invalidation block",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i);
            }
            free(hir_block_seen);
            return false;
        }

        if (require_mapping_for_all_blocks ? (block->is_reachable && !block->is_cleanup)
                                          : (block->predecessor_count > 0)) {
            for (size_t p = 0; p < block->predecessor_count; p++) {
                if (block->predecessors[p] == i) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] contains self predecessor",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i);
                    }
                    free(hir_block_seen);
                    return false;
                }
                if (block->predecessors[p] >= routine->block_count) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] has invalid predecessor %zu",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            i,
                            block->predecessors[p]);
                    }
                    free(hir_block_seen);
                    return false;
                }
            }
            for (size_t p = 0; p < block->predecessor_count; p++) {
                for (size_t q = p + 1; q < block->predecessor_count; q++) {
                    if (block->predecessors[p] == block->predecessors[q]) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] has duplicate predecessor %zu",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                i,
                                block->predecessors[p]);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                }
            }
        }

        if (block->has_succ_true && block->succ_true >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has invalid succ_true %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i,
                    block->succ_true);
            }
            free(hir_block_seen);
            return false;
        }
        if (block->has_succ_false && block->succ_false >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has invalid succ_false %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i,
                    block->succ_false);
            }
            free(hir_block_seen);
            return false;
        }
        if (block->has_cleanup_succ && block->cleanup_succ >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has invalid cleanup successor %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i,
                    block->cleanup_succ);
            }
            free(hir_block_seen);
            return false;
        }
        if (block->has_rollback_succ && block->rollback_succ >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has invalid rollback successor %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i,
                    block->rollback_succ);
            }
            free(hir_block_seen);
            return false;
        }
        if (block->has_invalidation_succ && block->invalidation_succ >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] has invalid invalidation successor %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    i,
                    block->invalidation_succ);
            }
            free(hir_block_seen);
            return false;
        }

        if (routine->has_rollback_block && block->has_rollback_succ) {
            if (!routine->has_rollback_block || block->rollback_succ != routine->rollback_block) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(

                        "MIR routine '%s' block[%zu] rollback successor must target routine rollback block",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (routine->has_invalidation_block && block->has_invalidation_succ) {
            if (!routine->has_invalidation_block
                || block->invalidation_succ != routine->invalidation_block) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] invalidation successor must target routine invalidation block",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i);
                }
                free(hir_block_seen);
                return false;
            }
        }

        if (block->has_succ_true) {
            const MIRBasicBlock *succ_block = &routine->blocks[block->succ_true];
            if (!mir_block_has_predecessor(succ_block, i)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] missing predecessor link to true successor %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i,
                        block->succ_true);
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (block->has_succ_false) {
            const MIRBasicBlock *succ_block = &routine->blocks[block->succ_false];
            if (!mir_block_has_predecessor(succ_block, i)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] missing predecessor link to false successor %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i,
                        block->succ_false);
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (block->has_cleanup_succ) {
            const MIRBasicBlock *cleanup_block = &routine->blocks[block->cleanup_succ];
            if (!mir_block_has_predecessor(cleanup_block, i)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] missing predecessor link to cleanup successor %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i,
                        block->cleanup_succ);
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (block->has_rollback_succ) {
            const MIRBasicBlock *rollback_block = &routine->blocks[block->rollback_succ];
            if (!mir_block_has_predecessor(rollback_block, i)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] missing predecessor link to rollback successor %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i,
                        block->rollback_succ);
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (block->has_invalidation_succ) {
            const MIRBasicBlock *invalidation_block = &routine->blocks[block->invalidation_succ];
            if (!mir_block_has_predecessor(invalidation_block, i)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] missing predecessor link to invalidation successor %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        i,
                        block->invalidation_succ);
                }
                free(hir_block_seen);
                return false;
            }
        }
    }

    if (routine->has_cleanup_block) {
        const MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];

        if (routine->has_rollback_block
            && (!cleanup->has_rollback_succ || cleanup->rollback_succ != routine->rollback_block)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block does not converge to rollback block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            free(hir_block_seen);
            return false;
        }
        if (routine->has_invalidation_block
            && (!cleanup->has_invalidation_succ
                || cleanup->invalidation_succ != routine->invalidation_block)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block does not converge to invalidation block",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            free(hir_block_seen);
            return false;
        }
        if (routine->has_rollback_block && routine->has_invalidation_block) {
            const MIRBasicBlock *rollback = &routine->blocks[routine->rollback_block];
            if (!rollback->has_invalidation_succ
                || rollback->invalidation_succ != routine->invalidation_block
                || !rollback->has_cleanup_succ
                || rollback->cleanup_succ != routine->invalidation_block) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' rollback block does not converge to invalidation block",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                free(hir_block_seen);
                return false;
            }
        }
    }

    free(hir_block_seen);
    return true;
}

const char *
mir_scope_kind_name(MIRScopeKind kind)
{
    switch (kind) {
        case MIR_SCOPE_FUNCTION: return "function";
        case MIR_SCOPE_METHOD: return "method";
        case MIR_SCOPE_INTENT: return "intent";
        default: return "unknown";
    }
}

const char *
mir_inst_kind_name(MIRInstKind kind)
{
    switch (kind) {
        case MIR_INST_DEF: return "def";
        case MIR_INST_RESOURCE_OP: return "resource-op";
        case MIR_INST_PHI: return "phi";
        case MIR_INST_BRANCH: return "branch";
        case MIR_INST_RETURN: return "return";
        case MIR_INST_CLEANUP_EDGE: return "cleanup";
        case MIR_INST_STMT: return "stmt";
        default: return "unknown";
    }
}
