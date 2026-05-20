#include <stdint.h>

#include "air_internal.h"

bool
air_collect_mir_evidence(AIRProgram *air,
                         const MIRProgram *mir,
                         char **error_message)
{
    if (air == NULL || mir == NULL)
        return true;

    air_mark_mir_input(air);

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        const char *routine_name = air_mir_routine_provider_name(routine);
        size_t cleanup_fact_count = air_mir_routine_cleanup_fact_count(routine);
        size_t terminator_fact_count =
            air_mir_routine_terminator_fact_count(routine);
        size_t select_receive_fact_count =
            air_mir_routine_select_receive_fact_count(routine);
        bool had_terminator_evidence;
        bool had_select_receive_evidence;
        bool had_cleanup_evidence;

        if ((terminator_fact_count > 0
             || select_receive_fact_count > 0
             || cleanup_fact_count > 0)
            && !air_require_mir_routine_provider(routine, error_message)) {
            return false;
        }
        if (terminator_fact_count > 0) {
            had_terminator_evidence =
                air_has_global_evidence_provider_subject(
                    air,
                    AIR_EVIDENCE_MIR_TERMINATOR,
                    routine_name,
                    "cfg-terminator");
            if (!air_append_evidence_node_ex(air,
                                             AIR_EVIDENCE_MIR_TERMINATOR,
                                             SIZE_MAX,
                                             routine_name,
                                             "cfg-terminator",
                                             terminator_fact_count,
                                             0,
                                             error_message)) {
                return false;
            }
            if (!had_terminator_evidence
                && !air_increment_evidence_summary_count(
                    air,
                    AIR_EVIDENCE_MIR_TERMINATOR)) {
                air_set_error(error_message,
                              "AIR MIR terminator evidence counter overflow");
                return false;
            }
        }
        if (select_receive_fact_count > 0) {
            had_select_receive_evidence =
                air_has_global_evidence_provider_subject(
                    air,
                    AIR_EVIDENCE_MIR_SELECT_RECEIVE,
                    routine_name,
                    "select-receive");
            if (!air_append_evidence_node_ex(air,
                                             AIR_EVIDENCE_MIR_SELECT_RECEIVE,
                                             SIZE_MAX,
                                             routine_name,
                                             "select-receive",
                                             select_receive_fact_count,
                                             0,
                                             error_message)) {
                return false;
            }
            if (!had_select_receive_evidence
                && !air_increment_evidence_summary_count(
                    air,
                    AIR_EVIDENCE_MIR_SELECT_RECEIVE)) {
                air_set_error(error_message,
                              "AIR MIR select receive evidence counter overflow");
                return false;
            }
        }
        if (cleanup_fact_count == 0)
            continue;
        had_cleanup_evidence = air_has_global_evidence_provider_subject(
            air,
            AIR_EVIDENCE_MIR_CLEANUP,
            routine_name,
            "cleanup-block");
        if (!air_append_evidence_node_ex(air,
                                         AIR_EVIDENCE_MIR_CLEANUP,
                                         SIZE_MAX,
                                         routine_name,
                                         "cleanup-block",
                                         cleanup_fact_count,
                                         0,
                                         error_message)) {
            return false;
        }
        if (!had_cleanup_evidence
            && !air_increment_evidence_summary_count(
                air,
                AIR_EVIDENCE_MIR_CLEANUP)) {
            air_set_error(error_message,
                          "AIR MIR cleanup evidence counter overflow");
            return false;
        }
    }

    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        const char *routine_name = air_mir_routine_provider_name(routine);
        for (size_t j = 0; j < routine->block_count; j++) {
            if (!air_collect_mir_pin_block_evidence(air, routine,
                                                    &routine->blocks[j],
                                                    routine_name,
                                                    error_message)) {
                return false;
            }
        }
    }
    return true;
}
