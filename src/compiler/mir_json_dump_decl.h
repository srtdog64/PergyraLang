#ifndef PERGYRA_COMPILER_MIR_JSON_DUMP_DECL_H
#define PERGYRA_COMPILER_MIR_JSON_DUMP_DECL_H

#include <stdio.h>

#include "mir.h"

/* Declaration-metadata JSON section owner for schema pgy.mir.v1. */
void mir_json_emit_decls(FILE *out, const MIRProgram *mir);

#endif
