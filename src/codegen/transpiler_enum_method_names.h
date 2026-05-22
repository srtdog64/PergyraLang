#ifndef PGY_TRANSPILER_ENUM_METHOD_NAMES_H
#define PGY_TRANSPILER_ENUM_METHOD_NAMES_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

bool transpiler_enum_method_emit_name(char *out,
                                      size_t out_size,
                                      const char *enum_name,
                                      const char *method_name);
bool transpiler_enum_method_surface_desc(char *out,
                                         size_t out_size,
                                         const char *enum_name,
                                         const char *method_name,
                                         const char *param_name);
void transpiler_enum_format_too_long(TranspilerCtx *ctx,
                                     const char *surface_kind);

#endif /* PGY_TRANSPILER_ENUM_METHOD_NAMES_H */
