#ifndef PERGYRA_COLLECTION_OWNED_ELEMENT_REQUIREMENT_OWNER_H
#define PERGYRA_COLLECTION_OWNED_ELEMENT_REQUIREMENT_OWNER_H

#include <stdbool.h>
#include <stddef.h>
#include "../parser/ast.h"

typedef struct SemanticContext SemanticContext;
typedef struct Type Type;
typedef struct CollectionOwnedElementRequirementStore
    CollectionOwnedElementRequirementStore;

/* Bootstrap refusal ratchet, not an element-ownership permission source.
 * One analyzed program owns the store; no rows escape to HIR/MIR. */
bool semantic_collection_owned_element_requirements_begin(
    ASTNode *program, SemanticContext *ctx);
bool semantic_collection_owned_element_requirement_record_deep_drop(
    ASTNode *call, ASTNode *receiver, const Type *array_type,
    SemanticContext *ctx);
bool semantic_collection_owned_element_requirement_record_storage_drop(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx);
bool semantic_collection_owned_element_requirement_record_argument(
    ASTNode *call, ASTNode *callee_decl, size_t argument_index,
    ASTNode *argument, const Type *argument_type, ParamMode parameter_mode,
    SemanticContext *ctx);
bool semantic_collection_owned_element_requirements_finalize(
    SemanticContext *ctx);
void semantic_collection_owned_element_requirements_destroy(
    CollectionOwnedElementRequirementStore *store);

#endif
