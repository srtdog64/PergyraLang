#ifndef PGY_TRANSPILER_DOMAIN_CONSTRUCTOR_EMIT_H
#define PGY_TRANSPILER_DOMAIN_CONSTRUCTOR_EMIT_H

#include "transpiler.h"

char *transpiler_emit_class_constructor_with_type(ASTNode *call,
                                                  ASTNode *class_decl,
                                                  const char *ctor_type,
                                                  TranspilerCtx *ctx);
char *transpiler_emit_domain_constructor_for_decl(ASTNode *call,
                                                  ASTNode *decl,
                                                  const char *type_name,
                                                  TranspilerCtx *ctx);
char *transpiler_emit_enum_variant_constructor(ASTNode *call,
                                               const char *qualified_name,
                                               TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_DOMAIN_CONSTRUCTOR_EMIT_H */
