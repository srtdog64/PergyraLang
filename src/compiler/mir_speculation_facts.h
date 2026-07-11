#ifndef PERGYRA_MIR_SPECULATION_FACTS_H
#define PERGYRA_MIR_SPECULATION_FACTS_H

#include "mir.h"

bool mir_capture_speculation_facts(MIRRoutine *routine);
bool mir_validate_speculation_facts(const MIRRoutine *routine,
                                    char **error_message);

#endif /* PERGYRA_MIR_SPECULATION_FACTS_H */
