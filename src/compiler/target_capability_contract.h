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

/* One compilation target (src/common/target_profile.def). */
typedef struct PgyTargetProfile
{
    const char *name;
    const char *llvm_triple;
    const char *c_target;
    const char *object_format;
    unsigned pointer_bits;
    unsigned pointer_align;
    bool little_endian;
} PgyTargetProfile;

/* The row of the host this compiler was built for, or NULL when the host has
 * none; pgy_target_profile_matches_host() refuses NULL and any row whose
 * pointer width, pointer alignment or byte order differs from the host's. */
const PgyTargetProfile *pgy_target_profile_active(void);
const PgyTargetProfile *pgy_target_profile_named(const char *name);
bool pgy_target_profile_matches_host(const PgyTargetProfile *profile,
                                     const char **error_out);

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
