#include "mir.h"

bool
mir_instruction_has_lifecycle_guard(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->has_lifecycle_guard_fact
        && inst->lifecycle_guard_kind != MIR_LIFECYCLE_GUARD_NONE;
}

MIRLifecycleGuardKind
mir_instruction_lifecycle_guard_kind(const MIRInstruction *inst)
{
    return mir_instruction_has_lifecycle_guard(inst)
        ? inst->lifecycle_guard_kind
        : MIR_LIFECYCLE_GUARD_NONE;
}

uint32_t
mir_instruction_lifecycle_valid_mask(const MIRInstruction *inst)
{
    return mir_instruction_has_lifecycle_guard(inst)
        ? inst->lifecycle_valid_mask
        : 0;
}

int
mir_instruction_lifecycle_to_state(const MIRInstruction *inst)
{
    return mir_instruction_has_lifecycle_guard(inst)
        ? inst->lifecycle_to_state
        : -1;
}

const char *
mir_instruction_lifecycle_op(const MIRInstruction *inst)
{
    return mir_instruction_has_lifecycle_guard(inst)
        ? inst->lifecycle_op
        : NULL;
}

const char *
mir_instruction_lifecycle_subject(const MIRInstruction *inst)
{
    return mir_instruction_has_lifecycle_guard(inst)
        ? inst->lifecycle_subject
        : NULL;
}

const char *
mir_instruction_lifecycle_receiver_name(const MIRInstruction *inst)
{
    return mir_instruction_has_lifecycle_guard(inst)
        ? inst->lifecycle_receiver_name
        : NULL;
}
