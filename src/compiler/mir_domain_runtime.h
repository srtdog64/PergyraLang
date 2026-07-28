#ifndef PERGYRA_MIR_DOMAIN_RUNTIME_H
#define PERGYRA_MIR_DOMAIN_RUNTIME_H

#include <stdbool.h>

#include "../semantic/domain_runtime_fact.h"

typedef struct DIRProgram DIRProgram;
typedef struct MIRProgram MIRProgram;

/*
 * Lossless MIR carrier for semantic-owned domain runtime assignment facts.
 * Domain topology remains a separate owner; neither carrier reconstructs the
 * other's facts from names or declaration order.
 */
bool mir_domain_runtime_project_from_dir(MIRProgram *mir,
                                         const DIRProgram *dir,
                                         char **error_message);
bool mir_domain_runtime_validate(const MIRProgram *mir,
                                 char **error_message);
void mir_domain_runtime_clear(MIRProgram *mir);

const char *mir_domain_participant_role_name(
    PgyDomainParticipantRole role);
const char *mir_domain_projection_operation_name(
    PgyDomainProjectionOperation operation);

#endif /* PERGYRA_MIR_DOMAIN_RUNTIME_H */
