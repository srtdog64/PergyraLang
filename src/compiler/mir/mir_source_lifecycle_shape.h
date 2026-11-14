#ifndef PERGYRA_MIR_SOURCE_LIFECYCLE_SHAPE_H
#define PERGYRA_MIR_SOURCE_LIFECYCLE_SHAPE_H

bool        mir_instruction_has_lifecycle_guard(
                const MIRInstruction *inst);
MIRLifecycleGuardKind mir_instruction_lifecycle_guard_kind(
                const MIRInstruction *inst);
uint32_t    mir_instruction_lifecycle_valid_mask(
                const MIRInstruction *inst);
int         mir_instruction_lifecycle_to_state(
                const MIRInstruction *inst);
const char *mir_instruction_lifecycle_op(
                const MIRInstruction *inst);
const char *mir_instruction_lifecycle_subject(
                const MIRInstruction *inst);
const char *mir_instruction_lifecycle_receiver_name(
                const MIRInstruction *inst);

#endif /* PERGYRA_MIR_SOURCE_LIFECYCLE_SHAPE_H */
