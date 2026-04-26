
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
