#ifndef PGY_TRANSPILER_WORLD_SELECT_EVENT_EMIT_INTERNAL_H
#define PGY_TRANSPILER_WORLD_SELECT_EVENT_EMIT_INTERNAL_H

#include "transpiler_world_select_event_emit.h"

void transpiler_emit_world_decl_impl(ASTNode *node,
                                     const MIRDeclHeader *header,
                                     const char *name,
                                     TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_WORLD_SELECT_EVENT_EMIT_INTERNAL_H */
