#include "air_internal.h"

#include "../semantic/semantic.h"

bool
air_collect_dag_evidence(AIRProgram *air, const SemanticResult *sem,
                         char **error_message)
{
    const size_t dead_end_count = sem != NULL
        ? sem->type_resolution_metadata_dead_ends
        : 0;
    const size_t metadata_fact_count = sem != NULL
        ? sem->type_resolution_metadata_entries
        : 0;
    const size_t generic_fact_count = sem != NULL
        ? sem->type_resolution_dag_generic_contract_evidence_count
        : 0;
    const size_t ability_fact_count = sem != NULL
        ? sem->type_resolution_dag_ability_consumer_evidence_count
        : 0;

    if (air == NULL || sem == NULL)
        return true;

    if (metadata_fact_count > 0 || dead_end_count > 0) {
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_DAG_METADATA,
                                         SIZE_MAX,
                                         "type-resolution-dag",
                                         "metadata-inventory",
                                         metadata_fact_count,
                                         dead_end_count,
                                         error_message)) {
            return false;
        }
        air->dag_metadata_evidence_count++;
    }

    if (generic_fact_count > 0 || dead_end_count > 0) {
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_DAG_GENERIC,
                                         SIZE_MAX,
                                         "type-resolution-dag",
                                         "generic-contracts",
                                         generic_fact_count,
                                         dead_end_count,
                                         error_message)) {
            return false;
        }
        air->dag_generic_evidence_count++;
    }

    if (ability_fact_count > 0 || dead_end_count > 0) {
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_DAG_ABILITY,
                                         SIZE_MAX,
                                         "type-resolution-dag",
                                         "ability-consumers",
                                         ability_fact_count,
                                         dead_end_count,
                                         error_message)) {
            return false;
        }
        air->dag_ability_evidence_count++;
    }
    return true;
}
