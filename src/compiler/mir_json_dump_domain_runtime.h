#ifndef PERGYRA_COMPILER_MIR_JSON_DUMP_DOMAIN_RUNTIME_H
#define PERGYRA_COMPILER_MIR_JSON_DUMP_DOMAIN_RUNTIME_H

#include <stdio.h>

#include "mir.h"

/* Optional semantic assignment carrier for schema pgy.mir.v1. */
void mir_json_emit_domain_runtime_assignments(
    FILE *out, const MIRProgram *mir);

#endif /* PERGYRA_COMPILER_MIR_JSON_DUMP_DOMAIN_RUNTIME_H */
