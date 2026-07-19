#ifndef PERGYRA_COMPILER_MIR_JSON_GENERIC_METHOD_SPECIALIZATION_H
#define PERGYRA_COMPILER_MIR_JSON_GENERIC_METHOD_SPECIALIZATION_H

#include "mir.h"

#include <stdio.h>

void mir_json_emit_generic_method_specializations(FILE *out,
                                                  const MIRProgram *mir);

#endif
