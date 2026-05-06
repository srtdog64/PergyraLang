#ifndef PGY_MIR_LOWER_PUBLIC_API_H
#define PGY_MIR_LOWER_PUBLIC_API_H


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
    mir_program_record_inventory_surface_usage(mir);

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
    for (size_t i = 0; i < hir->role_count; i++) {
        if (!mir_record_decl_header(mir, hir->roles[i])) {
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
            || !mir_populate_stmt_instructions(&routine)
            || !mir_populate_use_edges(&routine)
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
                    "HIR CFG storage changed during MIR lowering for routine '%s' (before_count=%zu after_count=%zu)",
                    routine.name != NULL ? routine.name : "(anonymous)",
                    cfg_block_count_before,
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

void
mir_active_inventory(const MIRProgram *mir,
                     ASTNodeType decl_type,
                     ASTNode ***nodes_out,
                     size_t *count_out)
{
    ASTNode **nodes = NULL;
    size_t count = 0;

    if (mir != NULL) {
        switch (decl_type) {
        case AST_ABILITY_DECL:
            nodes = mir->abilities;
            count = mir->ability_count;
            break;
        case AST_FUNC_DECL:
            nodes = mir->functions;
            count = mir->function_count;
            break;
        case AST_INTENT_DECL:
            nodes = mir->intents;
            count = mir->intent_count;
            break;
        case AST_ROLE_DECL:
            nodes = mir->roles;
            count = mir->role_count;
            break;
        case AST_PARTY_DECL:
            nodes = mir->parties;
            count = mir->party_count;
            break;
        case AST_ROSTER_DECL:
            nodes = mir->rosters;
            count = mir->roster_count;
            break;
        case AST_WORLD_DECL:
            nodes = mir->worlds;
            count = mir->world_count;
            break;
        case AST_RELATION_DECL:
            nodes = mir->relations;
            count = mir->relation_count;
            break;
        case AST_EFFECT_DECL:
            nodes = mir->effects;
            count = mir->effect_count;
            break;
        case AST_ZONE_DECL:
            nodes = mir->zones;
            count = mir->zone_count;
            break;
        case AST_EVENT_DECL:
            nodes = mir->events;
            count = mir->event_count;
            break;
        case AST_EXTERN_BLOCK:
            nodes = mir->externs;
            count = mir->extern_count;
            break;
        case AST_CLASS_DECL:
        case AST_ENUM_DECL:
        case AST_TYPE_ALIAS:
            nodes = mir->types;
            count = mir->type_count;
            break;
        default:
            break;
        }
    }

    if (nodes_out != NULL)
        *nodes_out = nodes;
    if (count_out != NULL)
        *count_out = count;
}

void
mir_active_externs(const MIRProgram *mir,
                   ASTNode ***nodes_out,
                   size_t *count_out)
{
    mir_active_inventory(mir, AST_EXTERN_BLOCK, nodes_out, count_out);
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

#endif /* PGY_MIR_LOWER_PUBLIC_API_H */
