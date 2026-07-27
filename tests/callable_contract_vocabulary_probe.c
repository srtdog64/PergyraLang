#include <stdio.h>
#include <string.h>

#include "semantic/callable_contract_vocabulary.h"

int
main(void)
{
    size_t index;

    if (!pgy_callable_contract_vocabulary_ready() ||
        pgy_callable_contract_vocabulary_count() != 18 ||
        pgy_callable_contract_vocabulary_axis_count(
            PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY) != 9 ||
        pgy_callable_contract_vocabulary_axis_count(
            PGY_CALLABLE_CONTRACT_AXIS_EFFECT) != 9 ||
        pgy_callable_contract_vocabulary_known_mask(
            PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY) != 0x1ffu ||
        pgy_callable_contract_vocabulary_known_mask(
            PGY_CALLABLE_CONTRACT_AXIS_EFFECT) != 0xffu) {
        return 1;
    }

    for (index = 0; index < pgy_callable_contract_vocabulary_count(); index++) {
        const PgyCallableContractWordSpec *spec =
            pgy_callable_contract_vocabulary_at(index);
        const PgyCallableContractWordSpec *by_text;
        const PgyCallableContractWordSpec *by_rank;

        if (spec == NULL ||
            pgy_callable_contract_vocabulary_find_id(spec->id) != spec) {
            return 2;
        }
        by_text = pgy_callable_contract_vocabulary_find(
            spec->axis, spec->spelling);
        by_rank = pgy_callable_contract_vocabulary_at_rank(
            spec->axis, spec->canonical_rank);
        if (by_text != spec || by_rank != spec)
            return 3;
        if (spec->zero_policy == PGY_CALLABLE_CONTRACT_ZERO_EXCLUSIVE &&
            (strcmp(spec->spelling, "local") != 0 || spec->mask != 0)) {
            return 4;
        }
    }

    puts("callable contract vocabulary probe: ok");
    return 0;
}
