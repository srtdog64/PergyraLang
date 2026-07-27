#include "mir.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../common/arena.h"
#include "../runtime/pgy_abi_spec.h"
#include "../parser/ast_api.h"
#include "mir_hir_block_projection.h"
#include "mir_lower_population.h"
#include "mir_parallel_capture_facts.h"
#include "mir_region_escape_facts.h"
#include "mir_generic_method_specialization.h"
#include "mir_public_surface.h"
#include "mir_signature_metadata.h"
#include "dir.h"
#include "mir_source_local_types.h"
#include "mir_destructure_type_facts.h"
#include "mir_domain_topology.h"
#include "mir_branch_source_facts.h"
#include "mir_speculation_facts.h"

#include "mir_base_helpers.h"
#include "mir_cleanup.h"
#include "mir_intent.h"
#include "mir_hir_fact_transfer.h"
#include "mir_machine_layer.h"
#include "mir_surface_usage.h"
#include "mir_stmt_population.h"
#include "mir_timing.h"
#include "mir_validation.h"

#include "mir_ssa_rename.h"

#include "mir_liveness_dce.h"
#include "mir_dce.h"

#include "mir_fact_validate.h"

#include "mir_decl_headers.h"
#include "mir_cfg_contract_validate.h"
#include "mir_abi_layout.h"

void
mir_lower_request_init(MIRLowerRequest *request,
                        const HIRProgram *hir,
                        const RIRProgram *rir,
                        const SemanticResult *semantic)
{
    if (request == NULL)
        return;
    request->protocol_id = PGY_MIR_LOWER_PROTOCOL_ID;
    request->protocol_version = PGY_MIR_LOWER_PROTOCOL_VERSION;
    request->hir = hir;
    request->dir = NULL;
    request->rir = rir;
    request->semantic = semantic;
}

void
mir_lower_request_bind_dir(MIRLowerRequest *request, const DIRProgram *dir)
{
    if (request != NULL)
        request->dir = dir;
}

MIRProgram *
mir_lower(const MIRLowerRequest *request, char **error_message)
{
    const char *debug_mir_lower;
    const HIRProgram *hir;
    const DIRProgram *dir;
    const RIRProgram *rir;
    const SemanticResult *semantic;
    MIRProgram *mir;
    if (error_message != NULL)
        *error_message = NULL;
    if (request == NULL
        || request->protocol_id == NULL
        || strcmp(request->protocol_id, PGY_MIR_LOWER_PROTOCOL_ID) != 0
        || request->protocol_version != PGY_MIR_LOWER_PROTOCOL_VERSION) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR lowering request has an unsupported protocol id/version");
        return NULL;
    }
    hir = request->hir;
    dir = request->dir;
    rir = request->rir;
    semantic = request->semantic;
    if (hir == NULL || semantic == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR lowering requires HIR and semantic facts");
        return NULL;
    }
    if ((hir->relation_count != 0 || hir->effect_count != 0
         || hir->zone_count != 0)
        && dir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR domain lowering requires DIR-owned topology facts");
        return NULL;
    }
    if (dir != NULL
        && dir->source_program_syntax_id
            != hir->source_program_syntax_id) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR lowering received DIR facts from a different source program");
        return NULL;
    }

    debug_mir_lower = getenv("PGY_DEBUG_MIR_LOWER");

    mir_abi_table_init();

    mir = calloc(1, sizeof(MIRProgram));
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return NULL;
    }
    mir->has_function_param_flow_facts = hir->has_function_param_flow_facts;
    mir->has_resource_flow_facts = hir->has_resource_flow_facts;
    mir->has_loop_flow_facts = hir->has_loop_flow_facts;
    if (!mir_import_parallel_capture_facts(mir, semantic, error_message)) {
        mir_destroy(mir);
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
    mir->main_function_name = NULL;
    for (size_t i = 0; i < mir->function_count; i++) {
        ASTNode *fn = mir->functions[i];
        const char *fn_name = ast_declaration_name(fn);
        if (fn == NULL || fn->type != AST_FUNC_DECL
            || fn_name == NULL) {
            continue;
        }
        if (strcmp(fn_name, "__pgy_top_level_exec") == 0)
            mir->has_top_level_exec = true;
        if (strcmp(fn_name, "Main") == 0) {
            mir->has_main_function = true;
            mir->main_function_name = fn_name;
        } else if (strcmp(fn_name, "main") == 0) {
            mir->has_main_function = true;
            if (mir->main_function_name == NULL)
                mir->main_function_name = fn_name;
        }
    }
    mir_program_record_inventory_surface_usage(mir);

    if (dir != NULL
        && !mir_domain_topology_project_from_dir(
            mir, dir, error_message)) {
        mir_destroy(mir);
        return NULL;
    }

#undef MIR_COPY_AST_LIST

    for (size_t i = 0; i < hir->function_count; i++) {
        if (!mir_record_decl_header(mir, hir->functions[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->type_count; i++) {
        if (!mir_record_decl_header(mir, hir->types[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->ability_count; i++) {
        if (!mir_record_decl_header(mir, hir->abilities[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }
    for (size_t i = 0; i < hir->intent_count; i++) {
        if (!mir_record_decl_header(mir, hir->intents[i])) {
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
    for (size_t i = 0; i < hir->event_count; i++) {
        if (!mir_record_decl_header(mir, hir->events[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }

    HIRRoutineInventory hir_inventory;
    hir_routine_inventory_from_program(hir, &hir_inventory);
    for (size_t i = 0; i < hir_inventory.count; i++) {
        const HIRRoutine *hir_routine =
            hir_routine_inventory_get(&hir_inventory, i);
        MIRRoutine routine;
        const HIRBasicBlock *cfg_blocks_before = NULL;
        size_t cfg_block_count_before = 0;
        if (hir_routine == NULL) {
            if (error_message != NULL)
                *error_message =
                    pergyra_strdup("invalid HIR routine inventory");
            mir_destroy(mir);
            return NULL;
        }
        memset(&routine, 0, sizeof(routine));
        pgy_arena_init_named(&routine.scratch, 0, "mir-routine-scratch");
        routine.id = mir->routine_count;
        routine.kind = mir_scope_kind_from_hir(hir_routine);
        routine.name = hir_routine->name;
        routine.ast = hir_routine->ast;
        routine.is_action_like = hir_routine->is_action_like;
        routine.hir_routine = hir_routine;
        routine.source_syntax_id = hir_routine->source_syntax_id;
        {
            double t0 = mir_timing_now();
            routine.rir_scope = mir_find_matching_rir_scope(rir, hir_routine);
            mir_timing_add(MIR_TIMING_RIR_MATCH, mir_timing_now() - t0);
        }
        routine.owner_name = routine.rir_scope != NULL
            ? routine.rir_scope->owner_name
            : hir_routine->owner_name;
        routine.owner_ast_type = hir_routine->owner_ast_type;
        if (routine.ast != NULL && routine.ast->type == AST_FUNC_DECL) {
            routine.generic_param_count = ast_generic_param_count(
                ast_declaration_generic_params(routine.ast));
            routine.params =
                ast_func_params(routine.ast, &routine.param_count);
            routine.return_type = ast_func_return_type(routine.ast);
            routine.within_zone = ast_func_within_zone(routine.ast);
            routine.has_signature = true;
            double t_sig = mir_timing_now();
            bool sig_ok = mir_routine_signature_metadata_capture(mir, &routine);
            mir_timing_add(MIR_TIMING_SIGNATURE, mir_timing_now() - t_sig);
            if (!sig_ok) {
                mir_routine_signature_metadata_clear(&routine);
                pgy_arena_destroy(&routine.scratch);
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "source-local type fact capture failed");
                mir_destroy(mir);
                return NULL;
            }
            if (!mir_copy_iteration_type_facts(&routine, hir_routine,
                                               error_message)) {
                mir_routine_signature_metadata_clear(&routine);
                mir_free_iteration_type_facts(&routine);
                pgy_arena_destroy(&routine.scratch);
                mir_destroy(mir);
                return NULL;
            }
            if (!mir_copy_destructure_type_facts(&routine, hir_routine,
                                                 error_message)) {
                mir_routine_signature_metadata_clear(&routine);
                mir_free_iteration_type_facts(&routine);
                pgy_arena_destroy(&routine.scratch);
                mir_destroy(mir);
                return NULL;
            }
            if (!mir_copy_match_binding_type_facts(
                    &routine, hir_routine, error_message)) {
                mir_routine_signature_metadata_clear(&routine);
                mir_free_iteration_type_facts(&routine);
                mir_free_destructure_type_facts(&routine);
                pgy_arena_destroy(&routine.scratch);
                mir_destroy(mir);
                return NULL;
            }
            double t_loc = mir_timing_now();
            bool loc_ok = mir_routine_source_local_type_names_capture(mir, &routine);
            mir_timing_add(MIR_TIMING_SOURCE_LOCAL_TYPES,
                           mir_timing_now() - t_loc);
            if (!loc_ok) {
                mir_routine_source_local_type_names_clear(&routine);
                mir_free_iteration_type_facts(&routine);
                mir_free_destructure_type_facts(&routine);
                mir_free_match_binding_type_facts(&routine);
                mir_routine_signature_metadata_clear(&routine);
                pgy_arena_destroy(&routine.scratch);
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "missing or invalid source-local type fact");
                mir_destroy(mir);
                return NULL;
            }
        }
        if (!mir_copy_resource_flow_symbols(
                &routine, hir_routine, error_message)) {
            mir_routine_signature_metadata_clear(&routine);
            mir_routine_source_local_type_names_clear(&routine);
            mir_free_iteration_type_facts(&routine);
            mir_free_destructure_type_facts(&routine);
            mir_free_match_binding_type_facts(&routine);
            pgy_arena_destroy(&routine.scratch);
            mir_free_resource_flow_symbols(&routine);
            mir_destroy(mir);
            return NULL;
        }
        if (!mir_copy_function_param_flow_summaries(
                &routine, hir_routine, error_message)) {
            mir_routine_signature_metadata_clear(&routine);
            mir_routine_source_local_type_names_clear(&routine);
            mir_free_iteration_type_facts(&routine);
            mir_free_destructure_type_facts(&routine);
            mir_free_match_binding_type_facts(&routine);
            pgy_arena_destroy(&routine.scratch);
            mir_free_resource_flow_symbols(&routine);
            mir_destroy(mir);
            return NULL;
        }
        if (!mir_copy_loop_flow_facts(&routine, hir_routine, error_message)) {
            free(routine.function_param_flow_summaries);
            mir_routine_signature_metadata_clear(&routine);
            mir_routine_source_local_type_names_clear(&routine);
            mir_free_iteration_type_facts(&routine);
            mir_free_destructure_type_facts(&routine);
            mir_free_match_binding_type_facts(&routine);
            pgy_arena_destroy(&routine.scratch);
            mir_free_resource_flow_symbols(&routine);
            mir_destroy(mir);
            return NULL;
        }
        cfg_blocks_before =
            hir_routine->has_cfg ? hir_routine->cfg.blocks : NULL;
        cfg_block_count_before =
            hir_routine->has_cfg ? hir_routine->cfg.block_count : 0;

        bool routine_ok = true;
        double t_step;
#define MIR_TIMED_STEP(slot, call) \
        do { \
            if (routine_ok) { \
                t_step = mir_timing_now(); \
                routine_ok = (call); \
                mir_timing_add((slot), mir_timing_now() - t_step); \
            } \
        } while (0)
        MIR_TIMED_STEP(MIR_TIMING_BUILD_BLOCKS,
                       mir_build_blocks_from_hir(&routine, hir_routine));
        MIR_TIMED_STEP(MIR_TIMING_CLEANUP_BLOCK,
                       mir_append_cleanup_block(&routine, routine.rir_scope));
        MIR_TIMED_STEP(MIR_TIMING_POPULATE_INSTS,
                       mir_populate_instructions(&routine));
        MIR_TIMED_STEP(MIR_TIMING_SSA_RENAME,
                       mir_apply_ssa_rename(&routine));
        MIR_TIMED_STEP(MIR_TIMING_STMT_INSTS,
                       mir_populate_stmt_instructions(&routine));
        MIR_TIMED_STEP(MIR_TIMING_STMT_INSTS,
                       mir_enrich_machine_layer_facts(&routine));
        MIR_TIMED_STEP(MIR_TIMING_STMT_INSTS,
                       mir_link_resource_runtime_facts(&routine));
        MIR_TIMED_STEP(MIR_TIMING_SPECULATION,
                       mir_capture_speculation_facts(&routine));
        MIR_TIMED_STEP(MIR_TIMING_USE_EDGES,
                       mir_populate_use_edges(&routine));
        MIR_TIMED_STEP(MIR_TIMING_CLEANUP_EDGES,
                       mir_materialize_cleanup_edges(&routine));
        MIR_TIMED_STEP(MIR_TIMING_RECOMPUTE,
                       mir_recompute_analysis(&routine));
#undef MIR_TIMED_STEP
        if (getenv("PGY_DEBUG_MIR_TIMING") != NULL) {
            double routine_total = mir_timing_total();
            static double prev_total;
            if (routine_total - prev_total > 0.05) {
                fprintf(stderr,
                    "[mir timing]   routine '%s': +%.3fs (blocks=%zu)\n",
                    routine.name != NULL ? routine.name : "(anonymous)",
                    routine_total - prev_total, routine.block_count);
            }
            prev_total = routine_total;
        }
        if (!routine_ok || !append_routine(mir, routine)) {
            free(routine.function_param_flow_summaries);
            free(routine.loop_flow_summaries);
            free(routine.loop_flow_states);
            mir_free_iteration_type_facts(&routine);
            mir_free_destructure_type_facts(&routine);
            mir_free_match_binding_type_facts(&routine);
            mir_free_resource_flow_symbols(&routine);
            mir_routine_signature_metadata_clear(&routine);
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

        if (debug_mir_lower != NULL && debug_mir_lower[0] != '\0' && routine.kind == MIR_SCOPE_INTENT) {
            fprintf(stdout,
                "[MIR LOWER] Intent '%s' after build: has_cleanup=%d, blocks=%zu\n",
                routine.name ? routine.name : "(null)",
                routine.has_cleanup_block, routine.block_count);
            for (size_t b = 0; b < routine.block_count; b++) {
                fprintf(stdout,
                    "  block[%zu] has_cleanup_succ=%d has_rollback_succ=%d has_invalidation_succ=%d\n",
                    b, routine.blocks[b].has_cleanup_succ,
                    routine.blocks[b].has_rollback_succ,
                    routine.blocks[b].has_invalidation_succ);
            }
        }
    }

    if (!mir_import_region_escape_facts(mir, hir, error_message)) {
        mir_destroy(mir);
        return NULL;
    }

    mir_link_decl_method_routines(mir);
    if (!mir_generic_method_specializations_capture(mir, error_message)) {
        mir_destroy(mir);
        return NULL;
    }

    {
        double t0 = mir_timing_now();
        bool dce_ok = mir_run_dce_pass(mir, error_message);
        mir_timing_add(MIR_TIMING_DCE, mir_timing_now() - t0);
        if (!dce_ok) {
            mir_destroy(mir);
            return NULL;
        }
    }
    mir_refresh_non_cfg_body_fallback_inventory(mir);

    if (getenv("PGY_DEBUG_MIR_TIMING") != NULL)
        mir_timing_report();

    return mir;
}
