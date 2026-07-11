#ifndef PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H
#define PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H

#include "../semantic/builtin_kind.h"
#include "transpiler.h"

char *emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx);
char *emit_builtin_text_builder(ASTNode *call, BuiltinKind kind,
                                TranspilerCtx *ctx);
char *emit_builtin_rc(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx);
char *emit_builtin_box(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_EXPR_CORE_BUILTINS_EMIT_H */
