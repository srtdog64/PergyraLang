#ifndef PGY_SRC_CODEGEN_TRANSPILER_CALL_RESULT_OPTION_BUILTIN_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_CALL_RESULT_OPTION_BUILTIN_EMIT_H

#include "transpiler.h"

char *emit_call_result_option_builtin(ASTNode *call,
                                      ASTNode *callee,
                                      TranspilerCtx *ctx);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_CALL_RESULT_OPTION_BUILTIN_EMIT_H */
