#include "air_internal.h"

#include "../semantic/semantic.h"

bool
air_collect_dag_evidence(AIRProgram *air, const SemanticResult *sem,
                         char **error_message)
{
    const size_t dead_end_count =
        semantic_result_type_resolution_metadata_dead_ends(sem);
    const size_t metadata_fact_count =
        semantic_result_type_resolution_metadata_entries(sem);
    const size_t generic_fact_count =
        semantic_result_dag_generic_contract_evidence_count(sem);
    const size_t ability_fact_count =
        semantic_result_dag_ability_consumer_evidence_count(sem);

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
        if (!air_increment_evidence_summary_count(
                air, AIR_EVIDENCE_DAG_METADATA)) {
            air_set_error(error_message,
                          "AIR DAG metadata evidence counter overflow");
            return false;
        }
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
        if (!air_increment_evidence_summary_count(
                air, AIR_EVIDENCE_DAG_GENERIC)) {
            air_set_error(error_message,
                          "AIR DAG generic evidence counter overflow");
            return false;
        }
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
        if (!air_increment_evidence_summary_count(
                air, AIR_EVIDENCE_DAG_ABILITY)) {
            air_set_error(error_message,
                          "AIR DAG ability evidence counter overflow");
            return false;
        }
    }
    return true;
}
