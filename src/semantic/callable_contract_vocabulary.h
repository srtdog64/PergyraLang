#ifndef PGY_CALLABLE_CONTRACT_VOCABULARY_H
#define PGY_CALLABLE_CONTRACT_VOCABULARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum PgyCallableContractAxis {
    PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY = 1,
    PGY_CALLABLE_CONTRACT_AXIS_EFFECT = 2
} PgyCallableContractAxis;

typedef enum PgyCallableContractZeroPolicy {
    PGY_CALLABLE_CONTRACT_ZERO_DISALLOWED = 0,
    PGY_CALLABLE_CONTRACT_ZERO_EXCLUSIVE = 1
} PgyCallableContractZeroPolicy;

typedef enum PgyCallableContractWordId {
#define PGY_CALLABLE_CONTRACT_CAPABILITY(identity, stable_id, spelling,       \
                                         mask_symbol, canonical_rank,         \
                                         zero_policy, external_name)          \
    PGY_CALLABLE_CONTRACT_WORD_##identity = stable_id,
#define PGY_CALLABLE_CONTRACT_EFFECT(identity, stable_id, spelling, mask_symbol, \
                                     canonical_rank, zero_policy, external_name) \
    PGY_CALLABLE_CONTRACT_WORD_##identity = stable_id,
#include "callable_contract_vocabulary.def"
#undef PGY_CALLABLE_CONTRACT_EFFECT
#undef PGY_CALLABLE_CONTRACT_CAPABILITY
    PGY_CALLABLE_CONTRACT_WORD_COUNT = 18,
    PGY_CALLABLE_CONTRACT_WORD_INVALID = -1
} PgyCallableContractWordId;

typedef struct PgyCallableContractWordSpec {
    PgyCallableContractWordId id;
    PgyCallableContractAxis axis;
    const char *spelling;
    uint32_t mask;
    size_t canonical_rank;
    PgyCallableContractZeroPolicy zero_policy;
    const char *external_name;
} PgyCallableContractWordSpec;

size_t pgy_callable_contract_vocabulary_count(void);
size_t pgy_callable_contract_vocabulary_axis_count(PgyCallableContractAxis axis);
const PgyCallableContractWordSpec *pgy_callable_contract_vocabulary_at(
    size_t index);
const PgyCallableContractWordSpec *pgy_callable_contract_vocabulary_at_rank(
    PgyCallableContractAxis axis, size_t canonical_rank);
const PgyCallableContractWordSpec *pgy_callable_contract_vocabulary_find(
    PgyCallableContractAxis axis, const char *spelling);
const PgyCallableContractWordSpec *pgy_callable_contract_vocabulary_find_id(
    PgyCallableContractWordId id);
uint32_t pgy_callable_contract_vocabulary_known_mask(
    PgyCallableContractAxis axis);
bool pgy_callable_contract_vocabulary_ready(void);

#endif /* PGY_CALLABLE_CONTRACT_VOCABULARY_H */
