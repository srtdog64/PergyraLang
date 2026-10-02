#ifndef PERGYRA_ARRAY_STORAGE_RELEASE_OWNER_H
#define PERGYRA_ARRAY_STORAGE_RELEASE_OWNER_H

#include "symbol_table.h"

typedef struct SemanticContext SemanticContext;

void semantic_array_storage_initialize(Symbol *binding, const ASTNode *initializer,
                                       SemanticContext *ctx);
void semantic_array_storage_escape(ASTNode *source, const Type *type,
                                   SemanticContext *ctx);
void semantic_array_storage_call_argument(ASTNode *source, const Type *type,
    const Type *result_type, ParamMode mode, bool constructor, SemanticContext *ctx);
void semantic_array_storage_assignment(ASTNode *target, const Type *target_type,
    ASTNode *value, const Type *value_type, SemanticContext *ctx);
bool semantic_array_storage_admit_drop(ASTNode *receiver, Type *array_type,
                                      SemanticContext *ctx);

#endif
