#ifndef PGY_TRANSPILER_DOMAIN_ROLE_METHODS_EMIT_H
#define PGY_TRANSPILER_DOMAIN_ROLE_METHODS_EMIT_H

#include "../compiler/mir.h"
#include "../parser/ast.h"
#include "transpiler.h"

void emit_role_method_impl(const char *role_name,
                           const MIRDeclMethod *method_meta,
                           const MIRRoutine *mir_method,
                           ASTNode *method,
                           TranspilerCtx *ctx);
void emit_role_vtable_instance(const char *role_name,
                               const char *metadata_role_name,
                               ASTNode *impl,
                               TranspilerCtx *ctx);
void emit_role_operator_aliases(ASTNode *role, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_DOMAIN_ROLE_METHODS_EMIT_H */
