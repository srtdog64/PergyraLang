#ifndef PGY_TRANSPILER_MIR_MATCH_PATTERN_EMIT_H
#define PGY_TRANSPILER_MIR_MATCH_PATTERN_EMIT_H

#include <stdint.h>

#include "transpiler.h"

void transpiler_mir_match_binding_name(uint32_t case_stable_id,
                                       const char *binding,
                                       char *buf,
                                       size_t buf_size);
bool transpiler_mir_is_option_destructor(ASTNode *pat,
                                         const char **kind,
                                         const char **binding);
bool transpiler_mir_is_result_destructor(ASTNode *pat,
                                         const char **kind,
                                         const char **binding);
const char *transpiler_mir_match_payload_field(const char *kind);

#endif /* PGY_TRANSPILER_MIR_MATCH_PATTERN_EMIT_H */
