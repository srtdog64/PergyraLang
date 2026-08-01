#ifndef PERGYRA_COMPILER_MIR_JSON_DUMP_INTERNAL_H
#define PERGYRA_COMPILER_MIR_JSON_DUMP_INTERNAL_H

#include <stdio.h>

#include "mir_abi.h"

void mir_json_emit_str(FILE *out, const char *s);
void mir_json_emit_str_or_null(FILE *out, const char *s);
void mir_json_emit_abi_layout_value(FILE *out,
                                    const MIRTypeLayout *layout);

#endif
