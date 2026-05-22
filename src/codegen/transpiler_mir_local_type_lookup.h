#ifndef PGY_TRANSPILER_MIR_LOCAL_TYPE_LOOKUP_H
#define PGY_TRANSPILER_MIR_LOCAL_TYPE_LOOKUP_H

#include "../parser/ast.h"
#include "transpiler.h"

const char *transpiler_mir_arena_copy_type_name(TranspilerCtx *ctx,
                                                const char *type_name);
const char *transpiler_mir_arena_render_type_name(TranspilerCtx *ctx,
                                                  const char *prefix,
                                                  const char *inner);
const char *transpiler_infer_local_type_name_from_expr(TranspilerCtx *ctx,
                                                       const ASTNode *func_decl,
                                                       ASTNode *expr);
const char *transpiler_find_local_type_name(TranspilerCtx *ctx,
                                            const ASTNode *func_decl,
                                            const char *base_name);

#endif /* PGY_TRANSPILER_MIR_LOCAL_TYPE_LOOKUP_H */
