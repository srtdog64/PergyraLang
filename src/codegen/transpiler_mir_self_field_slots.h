#ifndef PERGYRA_TRANSPILER_MIR_SELF_FIELD_SLOTS_H
#define PERGYRA_TRANSPILER_MIR_SELF_FIELD_SLOTS_H

#include "../parser/ast.h"
#include "transpiler_context.h"

void transpiler_mir_register_class_field_slots(TranspilerCtx *ctx,
                                               ASTNode *host);

#endif /* PERGYRA_TRANSPILER_MIR_SELF_FIELD_SLOTS_H */
