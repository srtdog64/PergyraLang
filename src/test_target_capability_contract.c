#include <stdio.h>
#include <string.h>

#include "compiler/target_capability_contract.h"

int
main(void)
{
    const PgyTargetCapabilityEnvelope *envelope =
        pgy_target_capability_envelope();
    const char *error = NULL;
    PgyTargetCapabilityEnvelope missing;
    const char *mutated_facts[8];
    uint64_t fingerprint;
    uint64_t projection_fingerprint = 0;

    if (!pgy_target_capability_ready_for_projection(
            "cpu-c", &projection_fingerprint, &error)
        || !pgy_target_capability_ready_for_projection(
            "cpu-llvm", &projection_fingerprint, &error)) {
        fprintf(stderr, "clean target capability contract rejected: %s\n",
                error != NULL ? error : "unknown error");
        return 1;
    }
    fingerprint = pgy_target_capability_fingerprint(envelope);
    if (fingerprint == 0) {
        fprintf(stderr, "clean target capability contract has no fingerprint\n");
        return 2;
    }
    for (size_t i = 0; i < envelope->fact_count; i++)
        mutated_facts[i] = envelope->facts[i];
    mutated_facts[0] = "mutated_intent_graph";
    missing = *envelope;
    missing.facts = mutated_facts;
    if (pgy_target_capability_fingerprint(&missing) == fingerprint) {
        fprintf(stderr, "target capability mutation kept the same fingerprint\n");
        return 3;
    }
    missing = *envelope;
    missing.fact_count = 0;
    if (pgy_target_capability_validate(&missing, "cpu-c", &error)) {
        fprintf(stderr, "missing target fact did not fail closed\n");
        return 4;
    }
    if (error == NULL || error[0] == '\0') {
        fprintf(stderr, "missing target fact returned no diagnostic\n");
        return 5;
    }

    /* Target profile: the host's row matches the host; a row that differs in
     * pointer width, alignment, byte order or object format, an empty target
     * and a missing row are refused with a diagnostic. */
    const PgyTargetProfile *active = pgy_target_profile_active();
    if (!pgy_target_profile_matches_host(active, &error) || active == NULL) {
        fprintf(stderr, "active target profile rejected: %s\n", error != NULL ? error : "missing row");
        return 6;
    }
    if (pgy_target_profile_named(active->name) != active || pgy_target_profile_named("NO_SUCH_TARGET") != NULL) {
        fprintf(stderr, "target profile lookup by name is not exact\n");
        return 7;
    }
    PgyTargetProfile forged;
    const char *expected[] = {
        "target profile: pointer width differs from the host",
        "target profile: pointer alignment differs from the host",
        "target profile: byte order differs from the host",
        "target profile: unknown object format",
        "target profile: the row names no target",
    };
    for (int mutation = 0; mutation < 5; mutation++) {
        forged = *active;
        if (mutation == 0) forged.pointer_bits = 32;
        if (mutation == 1) forged.pointer_align = 4;
        if (mutation == 2) forged.little_endian = !forged.little_endian;
        if (mutation == 3) forged.object_format = "wasm";
        if (mutation == 4) forged.llvm_triple = "";
        error = NULL;
        if (pgy_target_profile_matches_host(&forged, &error)
            || error == NULL || strcmp(error, expected[mutation]) != 0) {
            fprintf(stderr, "forged target profile %d was not refused as expected: %s\n",
                    mutation, error != NULL ? error : "admitted");
            return 8;
        }
    }
    error = NULL;
    if (pgy_target_profile_matches_host(NULL, &error) || error == NULL) {
        fprintf(stderr, "a missing target profile did not fail closed\n");
        return 9;
    }
    printf("target profile: %s %s %s %u-bit\n", active->name, active->llvm_triple,
           active->object_format, active->pointer_bits);

    puts("target capability contract probe: clean C/LLVM rows and missing-fact fail-closed path ok");
    return 0;
}
