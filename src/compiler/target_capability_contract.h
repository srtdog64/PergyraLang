#ifndef PERGYRA_TARGET_CAPABILITY_CONTRACT_H
#define PERGYRA_TARGET_CAPABILITY_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Projection adapter for the self-hosted target-capability owner. */
typedef struct PgyTargetCapabilityEnvelope
{
    const char *schema;
    const char *const *projections;
    size_t projection_count;
    const char *const *facts;
    size_t fact_count;
    const char *const *fallback_reasons;
    size_t fallback_reason_count;
} PgyTargetCapabilityEnvelope;

const PgyTargetCapabilityEnvelope *pgy_target_capability_envelope(void);
uint64_t pgy_target_capability_fingerprint(
    const PgyTargetCapabilityEnvelope *envelope);
bool pgy_target_capability_validate(
    const PgyTargetCapabilityEnvelope *envelope,
    const char *projection,
    const char **error_out);
bool pgy_target_capability_ready_for_projection(
    const char *projection,
    uint64_t *fingerprint_out,
    const char **error_out);

#endif /* PERGYRA_TARGET_CAPABILITY_CONTRACT_H */
