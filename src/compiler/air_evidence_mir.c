#include <stdint.h>

#include "air_internal.h"

static bool
air_collect_mir_requires_routine_provider(const MIRRoutine *routine,
                                          char **error_message)
{
    if (air_mir_routine_provider_name(routine) != NULL)
        return true;
    air_set_error(error_message,
                  "AIR MIR evidence requires routine name or owner provenance");
    return false;
}

static bool
air_collect_mir_requires_routine_inventory(const MIRRoutine *routine,
                                           size_t index,
                                           char **error_message)
{
    if (routine == NULL) {
        air_set_error(error_message,
                      "AIR MIR evidence has invalid routine inventory row[%zu]",
                      index);
        return false;
    }
    if (routine->block_count > 0 && routine->blocks == NULL) {
        air_set_error(error_message,
                      "AIR MIR evidence requires block inventory for routine '%s'",
                      routine->name != NULL ? routine->name : "(anonymous)");
        return false;
    }
    return true;
}

bool
air_collect_mir_evidence(AIRProgram *air,
                         const MIRProgram *mir,
                         char **error_message)
{
    if (air == NULL || mir == NULL)
        return true;

    air_mark_mir_input(air);

    MIRRoutineInventory inventory;
    mir_routine_inventory_from_program(mir, &inventory);

    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (!air_collect_mir_requires_routine_inventory(routine, i,
                                                        error_message)) {
            return false;
        }
        const char *routine_name = air_mir_routine_provider_name(routine);
        /* Bucket C: accumulate this routine's unproven retains (lifecycle CHECK
           guards at ambiguous joins) into the program total. */
        air->unproven_retain_count +=
            air_mir_routine_unproven_retain_fact_count(routine);
        /* Bucket A: accumulate inherent concurrency retains (parallel/channel). */
        air->inherent_concurrency_count +=
            air_mir_routine_inherent_concurrency_fact_count(routine);
        /* Bucket B: capability-bearing slot operations retained by policy. */
        air->slot_capability_retain_count +=
            air_mir_routine_slot_capability_retain_fact_count(routine);
        /* ...and capture their identity sites so AIR owns slot identity. */
        (void)air_collect_slot_sites(air, routine, routine_name);
        size_t cleanup_fact_count = air_mir_routine_cleanup_fact_count(routine);
        size_t terminator_fact_count =
            air_mir_routine_terminator_fact_count(routine);
        size_t select_receive_fact_count =
            air_mir_routine_select_receive_fact_count(routine);
        AIREvidenceKind cleanup_kind = air_mir_cleanup_evidence_kind();
        AIREvidenceKind terminator_kind = air_mir_terminator_evidence_kind();
        AIREvidenceKind select_receive_kind =
            air_mir_select_receive_evidence_kind();
        bool had_terminator_evidence;
        bool had_select_receive_evidence;
        bool had_cleanup_evidence;

        if ((terminator_fact_count > 0
             || select_receive_fact_count > 0
             || cleanup_fact_count > 0)
            && !air_collect_mir_requires_routine_provider(routine,
                                                          error_message)) {
            return false;
        }
        if (terminator_fact_count > 0) {
            had_terminator_evidence =
                air_has_global_evidence_provider_subject(
                    air,
                    terminator_kind,
                    routine_name,
                    "cfg-terminator");
            if (!air_append_evidence_node_ex(air,
                                             terminator_kind,
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
                    terminator_kind)) {
                air_set_error(error_message,
                              "AIR MIR terminator evidence counter overflow");
                return false;
            }
        }
        if (select_receive_fact_count > 0) {
            had_select_receive_evidence =
                air_has_global_evidence_provider_subject(
                    air,
                    select_receive_kind,
                    routine_name,
                    "select-receive");
            if (!air_append_evidence_node_ex(air,
                                             select_receive_kind,
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
                    select_receive_kind)) {
                air_set_error(error_message,
                              "AIR MIR select receive evidence counter overflow");
                return false;
            }
        }
        if (cleanup_fact_count == 0)
            continue;
        had_cleanup_evidence = air_has_global_evidence_provider_subject(
            air,
            cleanup_kind,
            routine_name,
            "cleanup-block");
        if (!air_append_evidence_node_ex(air,
                                         cleanup_kind,
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
                cleanup_kind)) {
            air_set_error(error_message,
                          "AIR MIR cleanup evidence counter overflow");
            return false;
        }
    }

    for (size_t i = 0; i < inventory.count; i++) {
        const MIRRoutine *routine = mir_routine_inventory_get(&inventory, i);
        if (!air_collect_mir_requires_routine_inventory(routine, i,
                                                        error_message)) {
            return false;
        }
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
