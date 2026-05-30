#include "mir_public_surface.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_analysis.h"
#include "../parser/ast_api.h"
#include "mir_base_helpers.h"
#include "mir_cfg_contract_validate.h"
#include "mir_dce.h"
#include "mir_fact_validate.h"
#include "mir_validation.h"

void
mir_instruction_record_surface_usage(MIRInstruction *inst)
{
    ASTNode *source_payload = NULL;

    if (inst == NULL)
        return;
    source_payload = mir_instruction_source_payload(inst);
    if (source_payload != NULL) {
        inst->has_source_location = true;
        inst->source_line = source_payload->line;
        inst->source_column = source_payload->column;
        inst->source_ast_type = source_payload->type;
    } else {
        inst->has_source_location = false;
        inst->source_line = 0;
        inst->source_column = 0;
        inst->source_ast_type = 0;
    }
    inst->has_surface_usage_facts = true;
    inst->uses_thread_pool_surface =
        ast_uses_thread_pool_surface(source_payload)
        || ast_uses_thread_pool_surface(inst->expr0)
        || ast_uses_thread_pool_surface(inst->expr1);
    inst->uses_intent_observability_surface =
        ast_uses_intent_observability_surface(source_payload)
        || ast_uses_intent_observability_surface(inst->expr0)
        || ast_uses_intent_observability_surface(inst->expr1);
}

bool
mir_instruction_is_with_slot_claim(const MIRInstruction *inst)
{
    return mir_instruction_source_is_with_slot_claim(inst);
}

void
mir_refresh_non_cfg_body_fallback_inventory(MIRProgram *mir)
{
    size_t fallback_total = 0;
    size_t fallback_routines = 0;

    if (mir == NULL)
        return;

    mir_count_non_cfg_body_fallback_inventory(mir,
                                              &fallback_total,
                                              &fallback_routines);
    mir->has_non_cfg_body_fallback_inventory = true;
    mir->non_cfg_body_fallback_total = fallback_total;
    mir->non_cfg_body_fallback_routine_count = fallback_routines;
}

void
mir_count_non_cfg_body_fallback_inventory(const MIRProgram *mir,
                                          size_t *fallback_total,
                                          size_t *fallback_routines)
{
    MIRRoutineInventory inventory;
    size_t total = 0;
    size_t routines = 0;

    mir_routine_inventory_from_program(mir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (routine == NULL)
            continue;
        total += routine->non_cfg_body_fallback_count;
        if (routine->used_non_cfg_body_fallback
            || routine->non_cfg_body_fallback_count > 0) {
            routines++;
        }
    }

    if (fallback_total != NULL)
        *fallback_total = total;
    if (fallback_routines != NULL)
        *fallback_routines = routines;
}

ASTNode *
mir_find_function_decl(const MIRProgram *mir, const char *name)
{
    if (mir == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < mir->function_count; i++) {
        ASTNode *fn = mir->functions[i];
        const char *fn_name = ast_declaration_name(fn);
        if (fn == NULL || fn->type != AST_FUNC_DECL
            || fn_name == NULL) {
            continue;
        }
        if (strcmp(fn_name, name) == 0)
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
    MIRMutableRoutineInventory inventory;

    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }
    mir_mutable_routine_inventory_from_program(mir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        MIRRoutine *routine = mir_mutable_routine_inventory_get(&inventory, i);
        if (routine == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("MIR liveness pass has invalid routine inventory");
            return false;
        }
        if (!mir_recompute_analysis(routine)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "failed to compute liveness for MIR routine '%s'",
                    routine->name != NULL
                        ? routine->name
                        : "(anonymous)");
            }
            return false;
        }
    }
    return true;
}

bool
mir_run_dce_pass(MIRProgram *mir, char **error_message)
{
    MIRMutableRoutineInventory inventory;

    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR program is null");
        return false;
    }

    mir_mutable_routine_inventory_from_program(mir, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        MIRRoutine *routine = mir_mutable_routine_inventory_get(&inventory, i);
        bool changed = false;

        if (routine == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("MIR DCE pass has invalid routine inventory");
            return false;
        }

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

bool
mir_validate_emission_contract(const MIRRoutine *routine,
                               bool require_cleanup,
                               bool require_cleanup_source_mapping,
                               char **error_message)
{
    char *contract_error = NULL;

    if (error_message != NULL)
        *error_message = NULL;

    if (!mir_validate_emission_topology(routine,
                                        require_cleanup,
                                        require_cleanup_source_mapping,
                                        &contract_error)) {
        if (error_message != NULL)
            *error_message = contract_error != NULL
                ? contract_error
                : pergyra_strdup("MIR emission topology validation failed");
        else
            free(contract_error);
        return false;
    }
    free(contract_error);
    contract_error = NULL;

    if (!mir_validate_routine_emission_facts(routine, &contract_error)) {
        if (error_message != NULL)
            *error_message = contract_error != NULL
                ? contract_error
                : pergyra_strdup("MIR emission fact validation failed");
        else
            free(contract_error);
        return false;
    }
    free(contract_error);
    return true;
}
