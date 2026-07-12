#ifndef PERGYRA_MIR_PARALLEL_CAPTURE_FACTS_H
#define PERGYRA_MIR_PARALLEL_CAPTURE_FACTS_H

#include "mir.h"

bool mir_import_parallel_capture_facts(MIRProgram *mir,
                                       const SemanticResult *semantic,
                                       char **error_message);
bool mir_validate_parallel_capture_facts(const MIRProgram *mir,
                                         char **error_message);
void mir_parallel_capture_facts_clear(MIRProgram *mir);

#endif
