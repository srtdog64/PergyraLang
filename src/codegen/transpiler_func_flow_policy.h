#ifndef PGY_TRANSPILER_FUNC_FLOW_POLICY_H
#define PGY_TRANSPILER_FUNC_FLOW_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler_context.h"

typedef enum TranspilerReturnOptionCtorOp {
    TRANS_RETURN_OPTION_CTOR_NONE = 0,
    TRANS_RETURN_OPTION_CTOR_NONE_VALUE,
    TRANS_RETURN_OPTION_CTOR_SOME,
} TranspilerReturnOptionCtorOp;

TranspilerReturnOptionCtorOp
transpiler_return_option_ctor_lookup(const char *callee_name);

bool transpiler_func_copy_current_return_type(TranspilerCtx *ctx,
                                              const char *type_name);

ASTNode *transpiler_func_current_return_callable_type(TranspilerCtx *ctx);

bool transpiler_func_parameter_surface_desc(char *out, size_t out_size,
                                            const char *param_name,
                                            const char *func_name);

void transpiler_func_format_too_long(TranspilerCtx *ctx,
                                     const char *surface_kind);

#endif /* PGY_TRANSPILER_FUNC_FLOW_POLICY_H */
