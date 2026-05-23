#include "air_internal.h"

#include "../semantic/semantic.h"

static bool
air_publish_dag_evidence(AIRProgram *air,
                         AIREvidenceKind kind,
                         const char *subject_name,
                         size_t fact_count,
                         size_t dead_end_count,
                         const char *counter_error,
                         char **error_message)
{
    if (!air_append_evidence_node_ex(air,
                                     kind,
                                     SIZE_MAX,
                                     "type-resolution-dag",
                                     subject_name,
                                     fact_count,
                                     dead_end_count,
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

    if (metadata_hit_count > 0 && metadata_fact_count == 0) {
        air_set_error(error_message,
                      "AIR DAG evidence saw metadata hits without metadata inventory");
        return false;
    }

    if (metadata_fact_count > 0 || dead_end_count > 0) {
        if (!air_publish_dag_evidence(
                air, AIR_EVIDENCE_DAG_METADATA,
                "metadata-inventory", metadata_fact_count, dead_end_count,
                "AIR DAG metadata evidence counter overflow",
                error_message)) {
            return false;
        }
    }

    if (generic_fact_count > 0 || dead_end_count > 0) {
        if (!air_publish_dag_evidence(
                air, AIR_EVIDENCE_DAG_GENERIC,
                "generic-contracts", generic_fact_count, dead_end_count,
                "AIR DAG generic evidence counter overflow",
                error_message)) {
            return false;
        }
    }

    if (ability_fact_count > 0 || dead_end_count > 0) {
        if (!air_publish_dag_evidence(
                air, AIR_EVIDENCE_DAG_ABILITY,
                "ability-consumers", ability_fact_count, dead_end_count,
                "AIR DAG ability evidence counter overflow",
                error_message)) {
            return false;
        }
    }
    return true;
}
