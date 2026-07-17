#include "target_capability_contract.h"

#include <stdint.h>
#include <string.h>

static uint64_t
target_capability_hash_bytes(uint64_t hash,
                             const unsigned char *bytes,
                             size_t count)
{
    for (size_t i = 0; i < count; i++) {
        hash ^= (uint64_t)bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t
target_capability_hash_u64(uint64_t hash, uint64_t value)
{
    unsigned char bytes[sizeof(value)];
    for (size_t i = 0; i < sizeof(value); i++)
        bytes[i] = (unsigned char)(value >> (i * 8));
    return target_capability_hash_bytes(hash, bytes, sizeof(bytes));
}

static uint64_t
target_capability_hash_string(uint64_t hash, const char *value)
{
    if (value == NULL)
        return target_capability_hash_u64(hash, UINT64_MAX);
    hash = target_capability_hash_u64(hash, (uint64_t)strlen(value));
    return target_capability_hash_bytes(
        hash, (const unsigned char *)value, strlen(value));
}

static uint64_t
target_capability_hash_list(uint64_t hash,
                            const char *const *values,
                            size_t count)
{
    hash = target_capability_hash_u64(hash, (uint64_t)count);
    for (size_t i = 0; i < count; i++)
        hash = target_capability_hash_string(hash, values != NULL ? values[i] : NULL);
    return hash;
}

static const char *const k_target_projections[] = {
    "cpu-c", "cpu-llvm", "self-hosted",
};

static const char *const k_target_facts[] = {
    "intent_graph", "effect_set", "authority_evidence", "coordination",
    "slot_ownership", "layout_shape", "loss_budget", "materialization_reason",
};

static const char *const k_target_fallback_reasons[] = {
    "unsupported_shape", "forbidden_loss_budget", "retained_effect",
    "missing_authority_evidence", "host_only_slot_boundary",
};

static const PgyTargetCapabilityEnvelope k_target_capability_envelope = {
    "pgy.selfhost.target-capability-envelope.v1",
    k_target_projections,
    sizeof(k_target_projections) / sizeof(k_target_projections[0]),
    k_target_facts,
    sizeof(k_target_facts) / sizeof(k_target_facts[0]),
    k_target_fallback_reasons,
    sizeof(k_target_fallback_reasons) / sizeof(k_target_fallback_reasons[0]),
};

static bool
target_capability_contains(const char *const *values,
                           size_t count,
                           const char *needle)
{
    if (values == NULL || needle == NULL || needle[0] == '\0')
        return false;
    for (size_t i = 0; i < count; i++) {
        if (values[i] != NULL && strcmp(values[i], needle) == 0)
            return true;
    }
    return false;
}

static bool
target_capability_fail(const char **error_out, const char *message)
{
    if (error_out != NULL)
        *error_out = message;
    return false;
}

const PgyTargetCapabilityEnvelope *
pgy_target_capability_envelope(void)
{
    return &k_target_capability_envelope;
}

uint64_t
pgy_target_capability_fingerprint(const PgyTargetCapabilityEnvelope *envelope)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    if (envelope == NULL)
        return 0;
    hash = target_capability_hash_string(hash, envelope->schema);
    hash = target_capability_hash_list(
        hash, envelope->projections, envelope->projection_count);
    hash = target_capability_hash_list(
        hash, envelope->facts, envelope->fact_count);
    hash = target_capability_hash_list(
        hash, envelope->fallback_reasons, envelope->fallback_reason_count);
    return hash;
}

bool
pgy_target_capability_validate(const PgyTargetCapabilityEnvelope *envelope,
                               const char *projection,
                               const char **error_out)
{
    if (error_out != NULL)
        *error_out = NULL;
    if (envelope == NULL)
        return target_capability_fail(error_out,
                                      "target capability: missing envelope");
    if (envelope->schema == NULL
        || strcmp(envelope->schema,
                  "pgy.selfhost.target-capability-envelope.v1") != 0)
        return target_capability_fail(
            error_out, "target capability: unsupported envelope schema");
    if (!target_capability_contains(envelope->projections,
                                    envelope->projection_count,
                                    projection))
        return target_capability_fail(
            error_out, "target capability: projection is not admitted");
    if (envelope->fact_count == 0 || envelope->facts == NULL)
        return target_capability_fail(
            error_out, "target capability: required fact envelope is empty");
    for (size_t i = 0; i < sizeof(k_target_facts) / sizeof(k_target_facts[0]); i++) {
        if (!target_capability_contains(envelope->facts,
                                        envelope->fact_count,
                                        k_target_facts[i]))
            return target_capability_fail(
                error_out, "target capability: required fact is missing");
    }
    if (envelope->fallback_reason_count == 0
        || envelope->fallback_reasons == NULL)
        return target_capability_fail(
            error_out, "target capability: fallback reason envelope is empty");
    for (size_t i = 0;
         i < sizeof(k_target_fallback_reasons)
             / sizeof(k_target_fallback_reasons[0]);
         i++) {
        if (!target_capability_contains(envelope->fallback_reasons,
                                        envelope->fallback_reason_count,
                                        k_target_fallback_reasons[i]))
            return target_capability_fail(
                error_out, "target capability: fallback reason is missing");
    }
    return true;
}

bool
pgy_target_capability_ready_for_projection(const char *projection,
                                           uint64_t *fingerprint_out,
                                           const char **error_out)
{
    const PgyTargetCapabilityEnvelope *envelope =
        pgy_target_capability_envelope();
    uint64_t fingerprint;

    if (fingerprint_out != NULL)
        *fingerprint_out = 0;
    if (!pgy_target_capability_validate(envelope, projection, error_out))
        return false;
    fingerprint = pgy_target_capability_fingerprint(envelope);
    if (fingerprint == 0)
        return target_capability_fail(
            error_out, "target capability: fingerprint is missing");
    if (fingerprint_out != NULL)
        *fingerprint_out = fingerprint;
    return true;
}
