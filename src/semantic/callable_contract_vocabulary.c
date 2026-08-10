#include "callable_contract_vocabulary.h"

#include <string.h>

#include "../runtime/pgy_runtime_capability.h"
#include "type_system.h"

#define PGY_CALLABLE_CONTRACT_CAPABILITY(identity, stable_id, spelling_value, \
                                         mask_symbol, rank_value, zero_value,  \
                                         external_value)                      \
    { PGY_CALLABLE_CONTRACT_WORD_##identity,                                  \
      PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY, spelling_value,                  \
      (uint32_t)(mask_symbol), rank_value, zero_value, external_value },
#define PGY_CALLABLE_CONTRACT_EFFECT(identity, stable_id, spelling_value,      \
                                     mask_symbol, rank_value, zero_value,      \
                                     external_value)                          \
    { PGY_CALLABLE_CONTRACT_WORD_##identity,                                  \
      PGY_CALLABLE_CONTRACT_AXIS_EFFECT, spelling_value,                      \
      (uint32_t)(mask_symbol), rank_value, zero_value, external_value },

static const PgyCallableContractWordSpec kCallableContractVocabulary[] = {
#include "callable_contract_vocabulary.def"
};

#undef PGY_CALLABLE_CONTRACT_EFFECT
#undef PGY_CALLABLE_CONTRACT_CAPABILITY

#define PGY_CALLABLE_CONTRACT_ARRAY_COUNT(array_value) \
    (sizeof(array_value) / sizeof((array_value)[0]))

size_t
pgy_callable_contract_vocabulary_count(void)
{
    return PGY_CALLABLE_CONTRACT_ARRAY_COUNT(kCallableContractVocabulary);
}

size_t
pgy_callable_contract_vocabulary_axis_count(PgyCallableContractAxis axis)
{
    size_t count = 0;
    size_t index;

    for (index = 0; index < pgy_callable_contract_vocabulary_count(); index++) {
        if (kCallableContractVocabulary[index].axis == axis)
            count++;
    }
    return count;
}

const PgyCallableContractWordSpec *
pgy_callable_contract_vocabulary_at(size_t index)
{
    if (index >= pgy_callable_contract_vocabulary_count())
        return NULL;
    return &kCallableContractVocabulary[index];
}

const PgyCallableContractWordSpec *
pgy_callable_contract_vocabulary_at_rank(PgyCallableContractAxis axis,
                                         size_t canonical_rank)
{
    size_t index;

    for (index = 0; index < pgy_callable_contract_vocabulary_count(); index++) {
        const PgyCallableContractWordSpec *spec =
            &kCallableContractVocabulary[index];
        if (spec->axis == axis && spec->canonical_rank == canonical_rank)
            return spec;
    }
    return NULL;
}

const PgyCallableContractWordSpec *
pgy_callable_contract_vocabulary_find(PgyCallableContractAxis axis,
                                      const char *spelling)
{
    size_t index;

    if (spelling == NULL)
        return NULL;
    for (index = 0; index < pgy_callable_contract_vocabulary_count(); index++) {
        const PgyCallableContractWordSpec *spec =
            &kCallableContractVocabulary[index];
        if (spec->axis == axis && strcmp(spec->spelling, spelling) == 0)
            return spec;
    }
    return NULL;
}

const PgyCallableContractWordSpec *
pgy_callable_contract_vocabulary_find_id(PgyCallableContractWordId id)
{
    size_t index;

    for (index = 0; index < pgy_callable_contract_vocabulary_count(); index++) {
        if (kCallableContractVocabulary[index].id == id)
            return &kCallableContractVocabulary[index];
    }
    return NULL;
}

uint32_t
pgy_callable_contract_vocabulary_known_mask(PgyCallableContractAxis axis)
{
    uint32_t mask = 0;
    size_t index;

    for (index = 0; index < pgy_callable_contract_vocabulary_count(); index++) {
        if (kCallableContractVocabulary[index].axis == axis)
            mask |= kCallableContractVocabulary[index].mask;
    }
    return mask;
}

bool
pgy_callable_contract_vocabulary_ready(void)
{
    const size_t count = pgy_callable_contract_vocabulary_count();
    size_t zero_exclusive_count = 0;
    size_t index;

    if (count != PGY_CALLABLE_CONTRACT_WORD_COUNT ||
        pgy_callable_contract_vocabulary_axis_count(
            PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY) != 9 ||
        pgy_callable_contract_vocabulary_axis_count(
            PGY_CALLABLE_CONTRACT_AXIS_EFFECT) != 9)
        return false;

    for (index = 0; index < count; index++) {
        const PgyCallableContractWordSpec *spec =
            &kCallableContractVocabulary[index];
        size_t other;

        if ((size_t)spec->id != index || spec->spelling == NULL ||
            spec->spelling[0] == '\0' || spec->external_name == NULL)
            return false;
        if (spec->axis != PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY &&
            spec->axis != PGY_CALLABLE_CONTRACT_AXIS_EFFECT)
            return false;
        if (spec->axis == PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY &&
            (spec->external_name[0] == '\0' ||
             spec->mask != (uint32_t)(1u << spec->canonical_rank)))
            return false;
        if (spec->axis == PGY_CALLABLE_CONTRACT_AXIS_EFFECT &&
            spec->external_name[0] != '\0')
            return false;

        if (spec->zero_policy == PGY_CALLABLE_CONTRACT_ZERO_EXCLUSIVE) {
            zero_exclusive_count++;
            if (spec->axis != PGY_CALLABLE_CONTRACT_AXIS_EFFECT ||
                spec->mask != 0 || strcmp(spec->spelling, "local") != 0)
                return false;
        } else if (spec->zero_policy !=
                       PGY_CALLABLE_CONTRACT_ZERO_DISALLOWED ||
                   spec->mask == 0) {
            return false;
        }

        if (pgy_callable_contract_vocabulary_at_rank(
                spec->axis, spec->canonical_rank) != spec)
            return false;
        for (other = index + 1; other < count; other++) {
            const PgyCallableContractWordSpec *other_spec =
                &kCallableContractVocabulary[other];
            if (spec->id == other_spec->id ||
                strcmp(spec->spelling, other_spec->spelling) == 0 ||
                (spec->axis == other_spec->axis &&
                 spec->canonical_rank == other_spec->canonical_rank))
                return false;
        }
    }

    if (zero_exclusive_count != 1)
        return false;
    for (index = 0; index < 9; index++) {
        if (pgy_callable_contract_vocabulary_at_rank(
                PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY, index) == NULL ||
            pgy_callable_contract_vocabulary_at_rank(
                PGY_CALLABLE_CONTRACT_AXIS_EFFECT, index) == NULL)
            return false;
    }
    return true;
}

#undef PGY_CALLABLE_CONTRACT_ARRAY_COUNT
