#include <stdio.h>

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

    puts("target capability contract probe: clean C/LLVM rows and missing-fact fail-closed path ok");
    return 0;
}
