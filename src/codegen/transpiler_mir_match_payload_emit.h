#ifndef PGY_TRANSPILER_MIR_MATCH_PAYLOAD_EMIT_H
#define PGY_TRANSPILER_MIR_MATCH_PAYLOAD_EMIT_H

#include "transpiler.h"

void transpiler_mir_emit_match_payload_binding(CodeBuf *buf,
                                               TranspilerCtx *ctx,
                                               ASTNode *subject_node,
                                               const char *subject,
                                               const char *kind,
                                               const char *binding,
                                               const char *emitted_name);
bool transpiler_mir_declare_guard_payload_binding(CodeBuf *buf,
                                                  TranspilerCtx *ctx,
                                                  ASTNode *subject_node,
                                                  const char *kind,
                                                  const char *binding,
                                                  const char *emitted_name);

#endif /* PGY_TRANSPILER_MIR_MATCH_PAYLOAD_EMIT_H */
