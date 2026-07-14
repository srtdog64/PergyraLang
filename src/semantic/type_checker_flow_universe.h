#ifndef PERGYRA_TYPE_CHECKER_FLOW_UNIVERSE_H
#define PERGYRA_TYPE_CHECKER_FLOW_UNIVERSE_H

#include "type_checker_internal.h"

#define RESOURCE_FLOW_INDEX_NONE ((size_t)-1)

void resource_flow_universe_begin(SemanticContext *ctx);
void resource_flow_universe_end(SemanticContext *ctx);
size_t resource_flow_universe_bind(SemanticContext *ctx, Symbol *symbol);
Symbol *resource_flow_universe_symbol(SemanticContext *ctx, size_t index);

#endif /* PERGYRA_TYPE_CHECKER_FLOW_UNIVERSE_H */
