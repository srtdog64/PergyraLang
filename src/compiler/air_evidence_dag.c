#include "air_internal.h"

#include "../semantic/semantic.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void
air_copy_lifecycle_name(char *dst, const char *src)
{
    if (dst == NULL)
        return;
    snprintf(dst, AIR_LIFECYCLE_NAME_LEN, "%s", src != NULL ? src : "");
}

static bool
air_collect_lifecycle_state_spaces(AIRProgram *air,
                                   const SemanticResult *sem,
                                   char **error_message)
{
    const size_t count = semantic_result_lifecycle_state_space_count(sem);

    if (count == 0)
        return true;
    if (air->lifecycle_state_spaces != NULL
        || air->lifecycle_state_space_count != 0) {
        air_set_error(error_message,
                      "AIR lifecycle state-space inventory collected twice");
        return false;
    }
    if (count > SIZE_MAX / sizeof(AIRLifecycleStateSpace)) {
        air_set_error(error_message,
                      "AIR lifecycle state-space inventory allocation overflow");
        return false;
    }

    air->lifecycle_state_spaces =
        (AIRLifecycleStateSpace *)calloc(count, sizeof(AIRLifecycleStateSpace));
    if (air->lifecycle_state_spaces == NULL) {
        air_set_error(error_message,
                      "AIR lifecycle state-space inventory allocation failed");
        return false;
    }
    air->lifecycle_state_space_count = count;

    for (size_t i = 0; i < count; i++) {
        const LcSpec *spec = semantic_result_lifecycle_state_space_at(sem, i);
        AIRLifecycleStateSpace *space = &air->lifecycle_state_spaces[i];
        LcMachine machine;

        if (spec == NULL) {
            air_set_error(error_message,
                          "AIR lifecycle state-space fact %zu is missing",
                          i);
            return false;
        }

        air_copy_lifecycle_name(space->subject, spec->subject);
        if (space->subject[0] == '\0') {
            air_set_error(error_message,
                          "AIR lifecycle state-space fact %zu has empty subject",
                          i);
            return false;
        }

        space->state_count = spec->state_count > 0
            ? (size_t)spec->state_count
            : 0;
        space->op_count = spec->op_count > 0
            ? (size_t)spec->op_count
            : 0;
        if (space->state_count > AIR_LIFECYCLE_MAX_STATES
            || space->op_count > AIR_LIFECYCLE_MAX_OPS) {
            air_set_error(error_message,
                          "AIR lifecycle state-space fact %zu exceeds AIR bounds",
                          i);
            return false;
        }

        for (size_t s = 0; s < space->state_count; s++)
            air_copy_lifecycle_name(space->states[s], spec->state_names[s]);

        machine = lc_spec_machine(spec);
        for (size_t o = 0; o < space->op_count; o++) {
            air_copy_lifecycle_name(space->ops[o].name, spec->op_names[o]);
            space->ops[o].valid_from_mask =
                lc_op_valid_from_mask(&machine, (int)o);
        }
    }

    return true;
}

static bool
air_publish_dag_evidence(AIRProgram *air,
                         AIREvidenceKind kind,
                         const char *subject_name,
                         size_t fact_count,
                         const char *counter_error,
                         char **error_message)
{
    if (!air_append_evidence_node_ex(air,
                                     kind,
                                     SIZE_MAX,
                                     "type-resolution-dag",
                                     subject_name,
                                     fact_count,
                                     0,
                                     error_message)) {
        return false;
    }
    if (!air_increment_evidence_summary_count(air, kind)) {
        air_set_error(error_message, counter_error);
        return false;
    }
    return true;
}

bool
air_collect_dag_evidence(AIRProgram *air, const SemanticResult *sem,
                         char **error_message)
{
    const size_t dead_end_count =
        semantic_result_type_resolution_metadata_dead_ends(sem);
    const size_t metadata_fact_count =
        semantic_result_type_resolution_metadata_entries(sem);
    const size_t metadata_hit_count =
        semantic_result_type_resolution_metadata_hits(sem);
    const size_t generic_fact_count =
        semantic_result_dag_generic_contract_evidence_count(sem);
    const size_t ability_fact_count =
        semantic_result_dag_ability_consumer_evidence_count(sem);

    if (air == NULL || sem == NULL)
        return true;

    /* Capture the program-wide capability mask so AIR owns the capability fact
       (previously only the separate --capability-manifest pipeline had it). */
    air->program_capabilities = sem->program_capabilities;

    if (!air_collect_lifecycle_state_spaces(air, sem, error_message))
        return false;

    if (metadata_hit_count > 0 && metadata_fact_count == 0) {
        air_set_error(error_message,
                      "AIR DAG evidence saw metadata hits without metadata inventory");
        return false;
    }
    if (dead_end_count > 0) {
        air_set_error(error_message,
                      "AIR DAG evidence contains unresolved metadata dead-end facts");
        return false;
    }

    if (metadata_fact_count > 0) {
        if (!air_publish_dag_evidence(
                air, AIR_EVIDENCE_DAG_METADATA,
                "metadata-inventory", metadata_fact_count,
                "AIR DAG metadata evidence counter overflow",
                error_message)) {
            return false;
        }
    }

    if (generic_fact_count > 0) {
        if (!air_publish_dag_evidence(
                air, AIR_EVIDENCE_DAG_GENERIC,
                "generic-contracts", generic_fact_count,
                "AIR DAG generic evidence counter overflow",
                error_message)) {
            return false;
        }
    }

    if (ability_fact_count > 0) {
        if (!air_publish_dag_evidence(
                air, AIR_EVIDENCE_DAG_ABILITY,
                "ability-consumers", ability_fact_count,
                "AIR DAG ability evidence counter overflow",
                error_message)) {
            return false;
        }
    }
    return true;
}
