#ifndef PERGYRA_TYPE_CHECKER_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_INTERNAL_H

#include "type_checker.h"

bool type_is_constructed_named(const Type *type, const char *name);
bool type_is_qubit(const Type *type);
bool type_is_slot_handle(const Type *type);
bool type_is_resource_handle(const Type *type);

bool consume_qubit_value(ASTNode *expr, SemanticContext *ctx,
                         const char *action);
Type *type_check_qubit_use(ASTNode *expr, SemanticContext *ctx);
Type *type_get_constructed_arg(const Type *type, size_t index);
Type *wrap_constructed(Type *constructor, Type *inner);

Type *type_check_stdlib_call(ASTNode *expr, const char *name,
                             SemanticContext *ctx);

#endif
