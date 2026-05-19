#ifndef PGY_SRC_CODEGEN_TRANSPILER_TYPE_RESULT_MAPPING_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_TYPE_RESULT_MAPPING_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

bool transpiler_result_suffix_from_type_name(const char *type_name,
                                             char *out,
                                             size_t out_size);
bool transpiler_result_suffix_from_context(TranspilerCtx *ctx,
                                           char *out,
                                           size_t out_size);
void ensure_result_specialization_from_type_name_to(TranspilerCtx *ctx,
                                                    CodeBuf *dst,
                                                    const char *type_name);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_TYPE_RESULT_MAPPING_HELPERS_H */
