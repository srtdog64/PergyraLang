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
mir_record_decl_header(MIRProgram *mir, ASTNode *decl)
{
    MIRDeclHeader header;

    if (mir == NULL || decl == NULL)
        return true;

    memset(&header, 0, sizeof(header));
    header.ast = decl;
    header.ast_type = decl->type;

    switch (decl->type) {
    case AST_CLASS_DECL:
        header.name = decl->data.class_decl.name;
        header.methods = decl->data.class_decl.methods;
        header.method_count = decl->data.class_decl.method_count;
        header.uses_pointer_self =
            decl->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL;
        break;
    case AST_ENUM_DECL:
        header.name = decl->data.enum_decl.name;
        header.methods = decl->data.enum_decl.methods;
        header.method_count = decl->data.enum_decl.method_count;
        break;
    case AST_PARTY_DECL:
        header.name = decl->data.party_decl.name;
        header.methods = decl->data.party_decl.methods;
        header.method_count = decl->data.party_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_ROSTER_DECL:
        header.name = decl->data.roster_decl.name;
        header.uses_pointer_self = true;
        break;
    case AST_WORLD_DECL:
        header.name = decl->data.world_decl.name;
        header.methods = decl->data.world_decl.methods;
        header.method_count = decl->data.world_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_RELATION_DECL:
        header.name = decl->data.relation_decl.name;
        header.methods = decl->data.relation_decl.methods;
        header.method_count = decl->data.relation_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_EFFECT_DECL:
        header.name = decl->data.effect_decl.name;
        header.methods = decl->data.effect_decl.methods;
        header.method_count = decl->data.effect_decl.method_count;
        header.uses_pointer_self = true;
        break;
    case AST_ZONE_DECL:
        header.name = decl->data.zone_decl.name;
        header.methods = decl->data.zone_decl.methods;
        header.method_count = decl->data.zone_decl.method_count;
        header.uses_pointer_self = true;
        break;
    default:
        return true;
    }

    if (header.name == NULL)
        return true;
    return mir_append_decl_header(mir, header);
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
        memset(&routine, 0, sizeof(routine));
        routine.id = mir->routine_count;
        routine.kind = mir_scope_kind_from_hir(hir_routine);
        routine.name = hir_routine->name;
        routine.ast = hir_routine->ast;
        routine.is_action_like = hir_routine->is_action_like;
        routine.hir_routine = hir_routine;
        routine.rir_scope = mir_find_matching_rir_scope(rir, hir_routine);
        routine.owner_name = routine.rir_scope != NULL
            ? routine.rir_scope->owner_name
            : NULL;

        if (!mir_build_blocks_from_hir(&routine, hir_routine)
            || !mir_append_cleanup_block(&routine, routine.rir_scope)
            || !mir_populate_instructions(&routine)
            || !mir_apply_ssa_rename(&routine)
            || !mir_populate_use_edges(&routine)
            || !mir_populate_stmt_instructions(&routine)
            || !mir_materialize_cleanup_edges(&routine)
            || !mir_recompute_analysis(&routine)
            || !append_routine(mir, routine)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
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

    if (!mir_run_dce_pass(mir, error_message)) {
        mir_destroy(mir);
        return NULL;
    }

    return mir;
}

void
mir_build_hir_inventory_view(const MIRProgram *mir, HIRProgram *hir_view)
{
    if (hir_view == NULL)
        return;
    memset(hir_view, 0, sizeof(*hir_view));
    if (mir == NULL)
        return;

    hir_view->externs = mir->externs;
    hir_view->extern_count = mir->extern_count;
    hir_view->types = mir->types;
    hir_view->type_count = mir->type_count;
    hir_view->abilities = mir->abilities;
    hir_view->ability_count = mir->ability_count;
    hir_view->roles = mir->roles;
    hir_view->role_count = mir->role_count;
    hir_view->parties = mir->parties;
    hir_view->party_count = mir->party_count;
    hir_view->rosters = mir->rosters;
    hir_view->roster_count = mir->roster_count;
    hir_view->worlds = mir->worlds;
    hir_view->world_count = mir->world_count;
    hir_view->relations = mir->relations;
    hir_view->relation_count = mir->relation_count;
    hir_view->effects = mir->effects;
    hir_view->effect_count = mir->effect_count;
    hir_view->zones = mir->zones;
    hir_view->zone_count = mir->zone_count;
    hir_view->events = mir->events;
    hir_view->event_count = mir->event_count;
    hir_view->intents = mir->intents;
    hir_view->intent_count = mir->intent_count;
    hir_view->functions = mir->functions;
    hir_view->function_count = mir->function_count;
    hir_view->executables = NULL;
    hir_view->executable_count = 0;
    hir_view->synthetic_executable_func =
        mir_find_function_decl(mir, "__pgy_top_level_exec");
    hir_view->has_main_function = mir->has_main_function;
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
    }
    free(mir->externs);
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

/*
 * Legacy MIR validation path retained for reference while we migrate callers
 * to the unified CFG contract validator (`mir_validate_cfg_contract_state`).
 * New runtime uses `mir_validate` below.
 */
#if 0
bool
mir_validate_legacy(const MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        const HIRRoutine *hir_routine = routine->hir_routine;
        bool *hir_block_seen = NULL;
        const size_t cfg_block_count = (hir_routine != NULL && hir_routine->has_cfg)
                                        ? hir_routine->cfg.block_count
                                        : 0;
        const MIRBasicBlock *cleanup_block = NULL;
        const MIRBasicBlock *rollback_block = NULL;
        const MIRBasicBlock *invalidation_block = NULL;

        if (!mir_validate_cfg_contract_state(routine, false, true, true, error_message))
            return false;

        if (routine->block_count == 0 || routine->entry_block >= routine->block_count) {
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
        if (routine->block_count > 0 && !routine->has_liveness) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing liveness information",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->block_count > 0 && !routine->has_use_def_summary) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing use-def summary",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->block_count > 0 && !routine->has_dce) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing DCE pass state",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (cfg_block_count > 0) {
            hir_block_seen = calloc(cfg_block_count, sizeof(bool));
            if (hir_block_seen == NULL) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup("out of memory");
                }
                return false;
            }
        }
        if (routine->has_rollback_block && !routine->has_cleanup_block) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has rollback block without cleanup root",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->has_invalidation_block && !routine->has_cleanup_block) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' has invalidation block without cleanup root",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->has_cleanup_block && routine->cleanup_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' cleanup block index %zu is out of range",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->cleanup_block);
            }
            free(hir_block_seen);
            return false;
        }
        if (routine->has_rollback_block && routine->rollback_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' rollback block index %zu is out of range",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->rollback_block);
            }
            free(hir_block_seen);
            return false;
        }
        if (routine->has_invalidation_block && routine->invalidation_block >= routine->block_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' invalidation block index %zu is out of range",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    routine->invalidation_block);
            }
            free(hir_block_seen);
            return false;
        }
        if (routine->has_cleanup_block) {
            cleanup_block = &routine->blocks[routine->cleanup_block];
            if (cleanup_block->is_cleanup == false) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' cleanup block %zu is not marked cleanup",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        routine->cleanup_block);
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (routine->has_rollback_block) {
            rollback_block = &routine->blocks[routine->rollback_block];
            if (rollback_block->is_cleanup == false) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' rollback block %zu is not marked cleanup",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        routine->rollback_block);
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (routine->has_invalidation_block) {
            invalidation_block = &routine->blocks[routine->invalidation_block];
            if (invalidation_block->is_cleanup == false) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' invalidation block %zu is not marked cleanup",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        routine->invalidation_block);
                }
                free(hir_block_seen);
                return false;
            }
        }
        for (size_t j = 0; j < routine->value_summary_count; j++) {
            const MIRValueSummary *summary = &routine->value_summaries[j];
            if (summary->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' value summary '%s' is missing slot anchor",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        summary->name != NULL ? summary->name : "(anonymous)");
                }
                return false;
            }
        }
        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
            if (!mir_validate_block_liveness_sets(routine, block, j, error_message))
                return false;
            if (cfg_block_count > 0 && !block->is_cleanup) {
                if (block->source_hir_block_id == SIZE_MAX
                    || block->source_hir_block_id >= cfg_block_count) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] uses invalid source_hir_block_id",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j);
                    }
                    free(hir_block_seen);
                    return false;
                }
                if (block->source_hir_block_id < cfg_block_count) {
                    if (hir_block_seen[block->source_hir_block_id]) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' source_hir_block_id collision on block[%zu] -> hir[%zu]",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                j,
                                block->source_hir_block_id);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                    hir_block_seen[block->source_hir_block_id] = true;
                }
            }
            if (block->is_reachable && !block->is_cleanup) {
                if (routine->has_cleanup_block && !block->has_cleanup_succ) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] missing cleanup edge from reachable non-cleanup block",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j);
                    }
                    free(hir_block_seen);
                    return false;
                }
                if (routine->has_cleanup_block) {
                    if (block->cleanup_succ >= routine->block_count
                        || block->cleanup_succ != routine->cleanup_block) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] cleanup edge does not target cleanup block",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                j);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                } else if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] has exceptional successor without cleanup root",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j);
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
                        j);
                }
                free(hir_block_seen);
                return false;
            }
            if (!block->is_cleanup && block->has_invalidation_succ && !routine->has_invalidation_block) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] has invalidation successor but no invalidation block",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        j);
                }
                free(hir_block_seen);
                return false;
            }
            if (!block->is_cleanup && block->is_reachable) {
                bool seen_pred = false;
                for (size_t pi = 0; pi < block->predecessor_count; pi++) {
                    if (block->predecessors[pi] == j) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] contains self predecessor",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                j);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                    seen_pred = true;
                    if (block->predecessors[pi] >= routine->block_count) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] has invalid predecessor %zu",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                j,
                                block->predecessors[pi]);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                }
                if (block->predecessor_count > 0 && !seen_pred) {
                    (void)0;
                }
            }
            for (size_t pi = 0; pi < block->predecessor_count; pi++) {
                for (size_t pj = pi + 1; pj < block->predecessor_count; pj++) {
                    if (block->predecessors[pi] == block->predecessors[pj]) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] has duplicate predecessor %zu",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                j,
                                block->predecessors[pi]);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                }
            }
            if (block->has_succ_true && block->succ_true >= routine->block_count) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] has invalid succ_true %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        j,
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
                        j,
                        block->succ_false);
                }
                free(hir_block_seen);
                return false;
            }
            if (block->has_rollback_succ && block->rollback_succ >= routine->block_count) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' block[%zu] has invalid rollback successor %zu",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        j,
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
                        j,
                        block->invalidation_succ);
                }
                free(hir_block_seen);
                return false;
            }

            if (routine->has_cleanup_block) {
                if (block->has_rollback_succ) {
                    if (routine->rollback_block == SIZE_MAX
                        || block->rollback_succ != routine->rollback_block
                        || !routine->has_rollback_block) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] rollback successor must target routine rollback block",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                j);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                }
                if (block->has_invalidation_succ) {
                    if (routine->invalidation_block == SIZE_MAX
                        || block->invalidation_succ != routine->invalidation_block
                        || !routine->has_invalidation_block) {
                        if (error_message != NULL) {
                            *error_message = mir_strdup_fmt(
                                "MIR routine '%s' block[%zu] invalidation successor must target routine invalidation block",
                                routine->name != NULL ? routine->name : "(anonymous)",
                                j);
                        }
                        free(hir_block_seen);
                        return false;
                    }
                }
            }

            if (block->has_succ_true) {
                const MIRBasicBlock *succ_block = &routine->blocks[block->succ_true];
                if (!mir_block_has_predecessor(succ_block, j)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] missing true-predecessor link to block[%zu]",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            block->succ_true);
                    }
                    free(hir_block_seen);
                    return false;
                }
            }
            if (block->has_succ_false) {
                const MIRBasicBlock *succ_block = &routine->blocks[block->succ_false];
                if (!mir_block_has_predecessor(succ_block, j)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] missing false-predecessor link to block[%zu]",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            block->succ_false);
                    }
                    free(hir_block_seen);
                    return false;
                }
            }
            if (block->has_cleanup_succ) {
                const MIRBasicBlock *succ_block = &routine->blocks[block->cleanup_succ];
                if (!mir_block_has_predecessor(succ_block, j)) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] missing cleanup-predecessor link to block[%zu]",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            block->cleanup_succ);
                    }
                    free(hir_block_seen);
                    return false;
                }
            }
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                if (inst->kind == MIR_INST_PHI
                    && inst->phi_incoming_count != block->predecessor_count) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi has %zu incoming edges but %zu predecessors",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            inst->phi_incoming_count,
                            block->predecessor_count);
                    }
                    return false;
                }
                if ((inst->kind == MIR_INST_RESOURCE_OP
                     || (inst->kind == MIR_INST_CLEANUP_EDGE && inst->rir_op != NULL))
                    && inst->slot_anchor == NULL) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] is missing slot anchor",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k);
                    }
                    return false;
                }
                if (inst->rir_op != NULL
                    && inst->slot_anchor != NULL
                    && inst->rir_op->slot_anchor != NULL
                    && strcmp(inst->slot_anchor, inst->rir_op->slot_anchor) != 0) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] slot anchor '%s' diverges from RIR '%s'",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k,
                            inst->slot_anchor,
                            inst->rir_op->slot_anchor);
                    }
                    return false;
                }
            }
            if (!mir_validate_instruction_uses(routine, block, j, error_message))
                return false;
        }
        if (routine->has_cleanup_block) {
            if (!cleanup_block->is_cleanup) {
                free(hir_block_seen);
                return false;
            }
            if (routine->has_rollback_block
                && (!cleanup_block->has_rollback_succ
                    || cleanup_block->rollback_succ != routine->rollback_block)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' cleanup block does not branch to rollback block",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                free(hir_block_seen);
                return false;
            }
            if (routine->has_invalidation_block
                && (!cleanup_block->has_invalidation_succ
                    || cleanup_block->invalidation_succ != routine->invalidation_block)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' cleanup block does not branch to invalidation block",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (routine->has_rollback_block && routine->has_invalidation_block) {
            if (!rollback_block->has_cleanup_succ
                || rollback_block->cleanup_succ != routine->invalidation_block
                || !rollback_block->has_invalidation_succ
                || rollback_block->invalidation_succ != routine->invalidation_block) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' rollback block does not converge to invalidation block",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                free(hir_block_seen);
                return false;
            }
            if (routine->has_invalidation_block
                && !invalidation_block->is_cleanup) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' invalidation block is not marked cleanup",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                free(hir_block_seen);
                return false;
            }
        }
        if (routine->has_cleanup_block) {
            const MIRBasicBlock *cleanup = &routine->blocks[routine->cleanup_block];
            if (routine->has_rollback_block
                && (!cleanup->has_rollback_succ || cleanup->rollback_succ != routine->rollback_block)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' cleanup root does not converge to rollback block",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                return false;
            }
            if (routine->has_invalidation_block
                && (!cleanup->has_invalidation_succ || cleanup->invalidation_succ != routine->invalidation_block)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' cleanup root does not converge to invalidation block",
                        routine->name != NULL ? routine->name : "(anonymous)");
                }
                return false;
            }
        }
        free(hir_block_seen);
    }

    return true;
}
#endif

bool
mir_validate(const MIRProgram *mir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];

        if (!mir_validate_cfg_contract_state(routine, false, true, true, error_message))
            return false;

        if (routine->block_count > 0 && !routine->has_liveness) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing liveness information",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->block_count > 0 && !routine->has_use_def_summary) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing use-def summary",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }
        if (routine->block_count > 0 && !routine->has_dce) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' is missing DCE pass state",
                    routine->name != NULL ? routine->name : "(anonymous)");
            }
            return false;
        }

        for (size_t j = 0; j < routine->value_summary_count; j++) {
            const MIRValueSummary *summary = &routine->value_summaries[j];
            if (summary->slot_anchor == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' value summary '%s' is missing slot anchor",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        summary->name != NULL ? summary->name : "(anonymous)");
                }
                return false;
            }
        }

        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
            if (!mir_validate_block_liveness_sets(routine, block, j, error_message))
                return false;
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                if (inst->kind == MIR_INST_PHI
                    && inst->phi_incoming_count != block->predecessor_count) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi has %zu incoming edges but %zu predecessors",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            inst->phi_incoming_count,
                            block->predecessor_count);
                    }
                    return false;
                }
                if ((inst->kind == MIR_INST_RESOURCE_OP
                     || (inst->kind == MIR_INST_CLEANUP_EDGE && inst->rir_op != NULL))
                    && inst->slot_anchor == NULL) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] is missing slot anchor",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k);
                    }
                    return false;
                }
                if (inst->rir_op != NULL
                    && inst->slot_anchor != NULL
                    && inst->rir_op->slot_anchor != NULL
                    && strcmp(inst->slot_anchor, inst->rir_op->slot_anchor) != 0) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR routine '%s' block[%zu] instruction[%zu] slot anchor '%s' diverges from RIR '%s'",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            j,
                            k,
                            inst->slot_anchor,
                            inst->rir_op->slot_anchor);
                    }
                    return false;
                }
            }
            if (!mir_validate_instruction_uses(routine, block, j, error_message))
                return false;
        }
    }

    return true;
}

bool
mir_validate_emission_topology(const MIRRoutine *routine,
                              bool require_cleanup,
                              bool require_cleanup_source_mapping,
                              char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    return mir_validate_cfg_contract_state(routine,
                                          require_cleanup,
                                          require_cleanup_source_mapping,
                                          false,
                                          error_message);
}

void
mir_dump(const MIRProgram *mir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (mir == NULL) {
        fprintf(out, "MIR: (null)\n");
        return;
    }

    fprintf(out, "MIR Program\n  routines: %zu\n", mir->routine_count);
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        fprintf(out,
                "  routine[%02zu] %-8s %s blocks=%zu instructions=%zu cleanup-block=%s rollback-block=%s invalidation-block=%s phi=%zu renamed=%zu cleanup-edges=%zu uses=%zu live=%zu dce=%zu\n",
                i,
                mir_scope_kind_name(routine->kind),
                routine->name != NULL ? routine->name : "(anonymous)",
                routine->block_count,
                routine->instruction_count,
                routine->has_cleanup_block ? "yes" : "no",
                routine->has_rollback_block ? "yes" : "no",
                routine->has_invalidation_block ? "yes" : "no",
                routine->phi_inserted_count,
                routine->renamed_value_count,
                routine->cleanup_edge_count,
                routine->use_edge_count,
                routine->live_value_count,
                routine->dce_removed_count);
        if (routine->has_use_def_summary) {
            fprintf(out, "    values=%zu\n", routine->value_summary_count);
            for (size_t j = 0; j < routine->value_summary_count; j++) {
                const MIRValueSummary *summary = &routine->value_summaries[j];
                fprintf(out,
                        "      value[%02zu] %s slot=%s def=b%zu:i%zu uses=%zu liveIn=%zu liveOut=%zu cleanup=%s\n",
                        j,
                        summary->name != NULL ? summary->name : "(anonymous)",
                        summary->slot_anchor != NULL ? summary->slot_anchor : "-",
                        summary->def_block,
                        summary->def_inst,
                        summary->use_count,
                        summary->live_in_block_count,
                        summary->live_out_block_count,
                        summary->reaches_cleanup ? "yes" : "no");
                if (summary->first_use_block != SIZE_MAX || summary->last_use_block != SIZE_MAX) {
                    fprintf(out,
                            "        use-range=%s%zu..%s%zu\n",
                            summary->first_use_block == SIZE_MAX ? "-" : "b",
                            summary->first_use_block == SIZE_MAX ? 0 : summary->first_use_block,
                            summary->last_use_block == SIZE_MAX ? "-" : "b",
                            summary->last_use_block == SIZE_MAX ? 0 : summary->last_use_block);
                }
            }
        }
        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
        const char *label_name = NULL;
        const ASTNode *source_stmt = NULL;
        const HIRBasicBlock *source_hir_block = NULL;
        char source_ast_loc[64] = "<none>";
        char source_ast_id[32] = "<none>";
        char label[128];
        bool has_source_stmt = false;
        if (routine->name != NULL)
            snprintf(label, sizeof(label), "_pgy_mir_bb_%s_%zu", routine->name, j);
        else
            snprintf(label, sizeof(label), "_pgy_mir_bb_%s_%zu", "(anonymous)", j);
        if (routine->hir_routine != NULL && routine->hir_routine->has_cfg
            && block->source_hir_block_id != SIZE_MAX
            && block->source_hir_block_id < routine->hir_routine->cfg.block_count) {
            source_hir_block = &routine->hir_routine->cfg.blocks[block->source_hir_block_id];
            if (source_hir_block->statement_count > 0)
                source_stmt = source_hir_block->statements[0];
            else if (source_hir_block->terminator_condition != NULL)
                source_stmt = source_hir_block->terminator_condition;
            else if (source_hir_block->terminator_value != NULL)
                source_stmt = source_hir_block->terminator_value;
            if (source_stmt != NULL)
                snprintf(source_ast_loc, sizeof(source_ast_loc), "line %u:%u",
                    source_stmt->line, source_stmt->column);
            snprintf(source_ast_id, sizeof(source_ast_id), "%p", (const void *)source_stmt);
            has_source_stmt = true;
        }
        label_name = label;
        fprintf(out,
                "    block[%02zu] label=%s reachable=%s cleanup=%s "
                "source-hir=%zu source-ast=%s source-ast-id=%s preds=%zu succT=%s succF=%s cleanupSucc=%s instructions=%zu defs=%zu uses=%zu liveIn=%zu liveOut=%zu\n",
                j,
                label_name,
                block->is_reachable ? "yes" : "no",
                block->is_cleanup ? "yes" : "no",
                (size_t)(block->source_hir_block_id),
                source_ast_loc,
                has_source_stmt ? source_ast_id : "<none>",
                block->predecessor_count,
                block->has_succ_true ? "yes" : "no",
                block->has_succ_false ? "yes" : "no",
                block->has_cleanup_succ ? "yes" : "no",
                    block->instruction_count,
                    block->def_name_count,
                    block->use_name_count,
                    block->live_in_name_count,
                    block->live_out_name_count);
            if (block->has_rollback_succ || block->has_invalidation_succ) {
                fprintf(out,
                        "      exceptional rollback=%s invalidation=%s\n",
                        block->has_rollback_succ ? "yes" : "no",
                        block->has_invalidation_succ ? "yes" : "no");
            }
            if (block->ssa_entry_value_count > 0) {
                fprintf(out, "      entry:");
                for (size_t k = 0; k < block->ssa_entry_value_count; k++)
                    fprintf(out, " %s", block->ssa_entry_values[k]);
                fprintf(out, "\n");
            }
            if (block->renamed_local_count > 0) {
                fprintf(out, "      renamed:");
                for (size_t k = 0; k < block->renamed_local_count; k++)
                    fprintf(out, " %s", block->renamed_locals[k]);
                fprintf(out, "\n");
            }
            if (block->ssa_exit_value_count > 0) {
                fprintf(out, "      exit:");
                for (size_t k = 0; k < block->ssa_exit_value_count; k++)
                    fprintf(out, " %s", block->ssa_exit_values[k]);
                fprintf(out, "\n");
            }
            if (block->def_name_count > 0) {
                fprintf(out, "      defs:");
                for (size_t k = 0; k < block->def_name_count; k++)
                    fprintf(out, " %s", block->def_names[k]);
                fprintf(out, "\n");
            }
            if (block->use_name_count > 0) {
                fprintf(out, "      uses:");
                for (size_t k = 0; k < block->use_name_count; k++)
                    fprintf(out, " %s", block->use_names[k]);
                fprintf(out, "\n");
            }
            if (block->live_in_name_count > 0) {
                fprintf(out, "      live-in:");
                for (size_t k = 0; k < block->live_in_name_count; k++)
                    fprintf(out, " %s", block->live_in_names[k]);
                fprintf(out, "\n");
            }
            if (block->live_out_name_count > 0) {
                fprintf(out, "      live-out:");
                for (size_t k = 0; k < block->live_out_name_count; k++)
                    fprintf(out, " %s", block->live_out_names[k]);
                fprintf(out, "\n");
            }
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                fprintf(out,
                        "      inst[%02zu] %-12s slot=%s name=%s result=%s arg0=%s arg1=%s",
                        k,
                        mir_inst_kind_name(inst->kind),
                        inst->slot_anchor != NULL ? inst->slot_anchor : "-",
                        inst->name != NULL ? inst->name : "-",
                        inst->result_name != NULL ? inst->result_name : "-",
                        inst->arg0 != NULL ? inst->arg0 : "-",
                        inst->arg1 != NULL ? inst->arg1 : "-");
                if (inst->phi_incoming_count > 0) {
                    fprintf(out, " incoming=");
                    for (size_t m = 0; m < inst->phi_incoming_count; m++) {
                        fprintf(out,
                                "%s%zu:%s",
                                m == 0 ? "" : ",",
                                inst->phi_incomings[m].predecessor_block,
                                inst->phi_incomings[m].value_name != NULL
                                    ? inst->phi_incomings[m].value_name
                                    : "-");
                    }
                }
                if (inst->use_count > 0) {
                    fprintf(out, " uses=");
                    for (size_t m = 0; m < inst->use_count; m++)
                        fprintf(out, "%s%s", m == 0 ? "" : ",", inst->uses[m]);
                }
                fprintf(out, "\n");
            }
        }
    }
}

/* =================================================================
 * ABI Type Layout Lookup
 *
 * Maps Pergyra surface type names (e.g. "Slot<Int>") to explicit
 * MIRTypeLayout structs sourced from pgy_abi_spec.h.
 *
 * Backends MUST use these lookups instead of inventing their own
 * struct layouts.
 * ================================================================= */

/* Static ABI type table — populated at MIR lower time */
static MIRTypeLayout g_abi_type_table[64];
static size_t g_abi_type_count = 0;

static MIRTypeLayout *
abi_type_lookup_by_name(const char *pergyra_type_name)
{
    if (pergyra_type_name == NULL)
        return NULL;

    for (size_t i = 0; i < g_abi_type_count; i++) {
        if (g_abi_type_table[i].abi_type_name != NULL
            && strcmp(g_abi_type_table[i].abi_type_name, pergyra_type_name) == 0) {
            return &g_abi_type_table[i];
        }
    }
    return NULL;
}

static MIRTypeLayout *
abi_type_lookup_by_runtime_fn(const char *runtime_fn)
{
    if (runtime_fn == NULL)
        return NULL;

    for (size_t i = 0; i < g_abi_type_count; i++) {
        if (g_abi_type_table[i].runtime_fn != NULL
            && strcmp(g_abi_type_table[i].runtime_fn, runtime_fn) == 0) {
            return &g_abi_type_table[i];
        }
    }
    return NULL;
}

static void
abi_type_table_init(void)
{
    g_abi_type_count = 0;

    /* Helper macro to add a type layout */
#define ADD_TYPE(name, abi, size, align, fn, inner) \
    do { \
        MIRTypeLayout *t = &g_abi_type_table[g_abi_type_count++]; \
        memset(t, 0, sizeof(*t)); \
        t->abi_type_name = (name); \
        t->size_bytes = (size); \
        t->align_bytes = (align); \
        t->runtime_fn = (fn); \
        t->inner_c_type = (inner); \
    } while (0)

    /* Slot<T> — Debug mode */
    ADD_TYPE("Slot<Int>",    "pgy_abi_slot_int_dbg",     8,  4, "pgy_claim_Int",     "int32_t");
    ADD_TYPE("Slot<Long>",   "pgy_abi_slot_long_dbg",   16,  8, "pgy_claim_Long",    "int64_t");
    ADD_TYPE("Slot<Float>",  "pgy_abi_slot_float_dbg",   8,  4, "pgy_claim_Float",   "float");
    ADD_TYPE("Slot<Double>", "pgy_abi_slot_double_dbg", 16,  8, "pgy_claim_Double",  "double");
    ADD_TYPE("Slot<Bool>",   "pgy_abi_slot_bool_dbg",    2,  1, "pgy_claim_Bool",    "bool");
    ADD_TYPE("Slot<String>", "pgy_abi_slot_string_dbg", 16,  8, "pgy_claim_String",  "char*");

    /* Slot<T> — Release mode (sizes differ, but fn names are same) */
    ADD_TYPE("Slot<Int>_rel",    "pgy_abi_slot_int_rel",    4, 4, "pgy_claim_Int",    "int32_t");
    ADD_TYPE("Slot<Long>_rel",   "pgy_abi_slot_long_rel",   8, 8, "pgy_claim_Long",   "int64_t");
    ADD_TYPE("Slot<Float>_rel",  "pgy_abi_slot_float_rel",  4, 4, "pgy_claim_Float",  "float");
    ADD_TYPE("Slot<Double>_rel", "pgy_abi_slot_double_rel", 8, 8, "pgy_claim_Double", "double");
    ADD_TYPE("Slot<Bool>_rel",   "pgy_abi_slot_bool_rel",   1, 1, "pgy_claim_Bool",   "bool");
    ADD_TYPE("Slot<String>_rel", "pgy_abi_slot_string_rel", 8, 8, "pgy_claim_String", "char*");

    /* SecureSlot<T> */
    ADD_TYPE("SecureSlot<Int>",    "pgy_abi_secure_slot_int_dbg", 16, 8, "pgy_claim_secure_Int",    "int32_t");
    ADD_TYPE("SecureSlot<String>", "pgy_abi_secure_slot_string_dbg", 24, 8, "pgy_claim_secure_String", "char*");

    /* DeviceSlot<T> */
    ADD_TYPE("DeviceSlot<Int>",    "pgy_abi_device_slot_int",    8,  4, "pgy_claim_device_Int",    "int32_t");
    ADD_TYPE("DeviceSlot<String>", "pgy_abi_device_slot_string", 16, 8, "pgy_claim_device_String", "char*");

    /* Option<T> */
    ADD_TYPE("Option<Int>",    "pgy_abi_option_int",     8,  4, "pgy_option_some_Int",    "int32_t");
    ADD_TYPE("Option<Long>",   "pgy_abi_option_long",   16,  8, "pgy_option_some_Long",   "int64_t");
    ADD_TYPE("Option<Bool>",   "pgy_abi_option_bool",    8,  4, "pgy_option_some_Bool",   "bool");
    ADD_TYPE("Option<String>", "pgy_abi_option_string", 16,  8, "pgy_option_some_String", "char*");

    /* Result<T, E> */
    ADD_TYPE("Result<Int>",    "pgy_abi_result_int",    16, 8, "pgy_result_ok_Int",    "int32_t");
    ADD_TYPE("Result<Bool>",   "pgy_abi_result_bool",   16, 8, "pgy_result_ok_Bool",   "bool");
    ADD_TYPE("Result<String>", "pgy_abi_result_string", 16, 8, "pgy_result_ok_String", "char*");

    /* Channel — opaque handles */
    ADD_TYPE("ZoneChannel<Int>",    "pgy_abi_zone_channel_handle",    4, 4, "pgy_zone_channel_create_Int",    "int32_t");
    ADD_TYPE("WorldChannel<Int>",   "pgy_abi_world_channel_handle",   4, 4, "pgy_world_channel_create_Int",   "int32_t");
    ADD_TYPE("ZoneChannel<String>", "pgy_abi_zone_channel_handle",    4, 4, "pgy_zone_channel_create_String", "char*");

    /* Box<T> */
    ADD_TYPE("Box<Int>",    "pgy_abi_box_int",    8, 8, "pgy_box_new_Int",    "int32_t");
    ADD_TYPE("Box<String>", "pgy_abi_box_string", 8, 8, "pgy_box_new_String", "char*");

    /* Array<T> */
    ADD_TYPE("Array<Int>",    "pgy_abi_array_int", 24, 8, "pgy_array_new_Int",    "int32_t");
    ADD_TYPE("Array<String>", "pgy_abi_array_string", 24, 8, "pgy_array_new_String", "char*");

    /* Auxiliary */
    ADD_TYPE("Future",           "pgy_abi_future",          8, 4, "pgy_spawn",            "int32_t");
    ADD_TYPE("RemoteFuture",     "pgy_abi_remote_future",  24, 8, "pgy_spawn",            "int32_t");
    ADD_TYPE("Qubit",            "pgy_abi_qubit",          12, 4, "ClaimQubit",           "int32_t");
    ADD_TYPE("TaskHandle",       "pgy_abi_task_handle",     8, 4, NULL,                   "int32_t");

#undef ADD_TYPE
}

/* Extract inner type from "Slot<Int>" → "Int" */
static const char *
mir_extract_inner_type_suffix(const char *pergyra_type_name)
{
    if (pergyra_type_name == NULL)
        return NULL;
    const char *lt = strchr(pergyra_type_name, '<');
    const char *gt = strrchr(pergyra_type_name, '>');
    if (lt == NULL || gt == NULL || gt <= lt)
        return NULL;
    /* Return substring between < and > */
    static char buf[64];
    size_t len = (size_t)(gt - lt - 1);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, lt + 1, len);
    buf[len] = '\0';
    return buf;
}

/* Lookup ABI type by Pergyra type name, with debug/release mode awareness */
const MIRTypeLayout *
mir_abi_lookup(const char *pergyra_type_name)
{
    int written;

    if (pergyra_type_name == NULL)
        return NULL;

    /* First try exact match (debug mode default) */
    MIRTypeLayout *t = abi_type_lookup_by_name(pergyra_type_name);
    if (t != NULL)
        return t;

    /* Try release mode variant */
    char rel_name[128];
    written = snprintf(rel_name, sizeof(rel_name), "%s_rel", pergyra_type_name);
    if (written >= 0 && (size_t)written < sizeof(rel_name)) {
        t = abi_type_lookup_by_name(rel_name);
        if (t != NULL)
            return t;
    }

    /* Try to find by runtime function name pattern */
    /* e.g. "Slot<Int>" → look for any type with "pgy_claim_Int" */
    const char *suffix = mir_extract_inner_type_suffix(pergyra_type_name);
    if (suffix != NULL) {
        char fn_prefix[128];

        /* Check if it's a Slot type */
        if (strncmp(pergyra_type_name, "Slot<", 5) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_claim_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Option */
        if (strncmp(pergyra_type_name, "Option<", 7) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_option_some_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
        /* Result */
        if (strncmp(pergyra_type_name, "Result<", 7) == 0) {
            written = snprintf(fn_prefix, sizeof(fn_prefix), "pgy_result_ok_%s", suffix);
            if (written >= 0 && (size_t)written < sizeof(fn_prefix)) {
                t = abi_type_lookup_by_runtime_fn(fn_prefix);
                if (t != NULL)
                    return t;
            }
        }
    }

    return NULL;  /* unknown type — backend should handle gracefully */
}

void
mir_abi_table_init(void)
{
    abi_type_table_init();
}
