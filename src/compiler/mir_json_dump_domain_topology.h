#ifndef PERGYRA_COMPILER_MIR_JSON_DUMP_DOMAIN_TOPOLOGY_H
#define PERGYRA_COMPILER_MIR_JSON_DUMP_DOMAIN_TOPOLOGY_H

#include <stdio.h>

#include "mir.h"

/* Optional program-global DIR topology carrier for schema pgy.mir.v1. */
void mir_json_emit_domain_topology(FILE *out, const MIRProgram *mir);

#endif
