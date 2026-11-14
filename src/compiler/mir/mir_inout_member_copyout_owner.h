#ifndef PERGYRA_MIR_INOUT_MEMBER_COPYOUT_OWNER_H
#define PERGYRA_MIR_INOUT_MEMBER_COPYOUT_OWNER_H

#include "../mir.h"

bool mir_instruction_is_inout_member_copyout(
    const MIRInstruction *instruction);

bool mir_synthesize_inout_member_copyouts(
    MIRRoutine *routine,
    char **error_message);

bool mir_validate_inout_member_copyouts(
    const MIRRoutine *routine,
    char **error_message);

#endif /* PERGYRA_MIR_INOUT_MEMBER_COPYOUT_OWNER_H */
