#ifndef PERGYRA_INDEXED_STRING_BORROW_OWNER_H
#define PERGYRA_INDEXED_STRING_BORROW_OWNER_H

#include <stdbool.h>

typedef struct ASTNode ASTNode;
typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;

void semantic_indexed_string_borrow_initialize_binding(
    Symbol *binding, ASTNode *initializer, SemanticContext *ctx);
void semantic_indexed_string_borrow_record_assignment(
    Symbol *binding, ASTNode *value, ASTNode *assignment,
    SemanticContext *ctx);
void semantic_indexed_string_borrow_invalidate_deep_drop(
    ASTNode *receiver, SemanticContext *ctx);
bool semantic_indexed_string_borrow_validate_use(
    Symbol *binding, ASTNode *use, SemanticContext *ctx);

#endif
