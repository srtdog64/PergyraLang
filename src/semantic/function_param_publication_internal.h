/* Private implementation seams of the single parameter publication owner. */
#ifndef FUNCTION_PARAM_PUBLICATION_INTERNAL_H
#define FUNCTION_PARAM_PUBLICATION_INTERNAL_H

#include "function_param_publication_owner.h"

bool publication_generation_ready(const SemanticContext *ctx);
bool publication_physical_scalar(const Type *type);
PublishedNode *publication_node(FunctionParamPublicationStore *store,
    const ASTNode *node, bool insert);
bool publication_call_ready(const PublishedNode *row);
PublishedNode *publication_current_value(const SemanticContext *ctx,
    const ASTNode *value);

#endif
