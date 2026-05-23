#include "air_internal.h"

#include "../runtime/pgy_frontier_policy.h"
#include "../runtime/pgy_runtime_observability_schema.h"

#include <stdint.h>

static bool
air_collect_singleton_global_evidence(AIRProgram *air,
                                      AIREvidenceKind kind,
                                      const char *provider_name,
                                      const char *subject_name,
                                      size_t fact_count,
                                      char **error_message)
{
    const AIREvidenceNode *existing;

    if (air == NULL)
        return true;
    existing = air_global_evidence_node_provider_subject(air,
                                                         kind,
                                                         provider_name,
                                                         subject_name);
    if (existing != NULL) {
        if (existing->fact_count != fact_count
            || existing->fallback_count != 0) {
            air_set_error(error_message,
                          "AIR singleton global evidence has conflicting counts");
            return false;
        }
        return true;
    }
    if (!air_append_evidence_node_ex(air,
                                     kind,
                                     SIZE_MAX,
                                     provider_name,
                                     subject_name,
                                     fact_count,
                                     0,
                                     error_message)) {
        return false;
    }
    if (!air_increment_evidence_summary_count(air, kind)) {
        air_set_error(error_message,
                      "AIR runtime evidence counter overflow");
        return false;
    }
    return true;
}

static bool
air_collect_singleton_runtime_evidence(AIRProgram *air,
                                       AIREvidenceKind kind,
                                       const char *provider_name,
                                       const char *subject_name,
                                       size_t fact_count,
                                       char **error_message)
{
    return air_collect_singleton_global_evidence(air,
                                                kind,
                                                provider_name,
                                                subject_name,
                                                fact_count,
                                                error_message);
}

bool
air_collect_observability_schema_evidence(AIRProgram *air, char **error_message)
{
    return air_collect_singleton_runtime_evidence(
        air,
        AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
        "runtime-observability-schema",
        PGY_OBSERVABILITY_ABI_SCHEMA,
        PGY_OBSERVABILITY_SCHEMA_FACT_COUNT,
        error_message);
}

bool
air_collect_runtime_frontier_policy_evidence(AIRProgram *air,
                                             char **error_message)
{
    return air_collect_singleton_runtime_evidence(
        air,
        AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
        PGY_FRONTIER_POLICY_SCHEMA,
        PGY_FRONTIER_POLICY_SUBJECT,
        PGY_FRONTIER_POLICY_FACT_COUNT,
        error_message);
}
