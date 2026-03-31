#ifndef PERGYRA_TYPE_CHECKER_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_INTERNAL_H

#include "type_checker.h"

bool type_is_constructed_named(const Type *type, const char *name);
bool type_is_qubit(const Type *type);
bool type_is_slot_handle(const Type *type);
bool type_is_resource_handle(const Type *type);
bool type_is_anchored_resource_handle(const Type *type);
bool type_is_movable_resource_handle(const Type *type);

bool consume_qubit_value(ASTNode *expr, SemanticContext *ctx,
                         const char *action);
Type *type_check_qubit_use(ASTNode *expr, SemanticContext *ctx);

/* Resource-handle compile-time state tracking.
 * Today the richer semantic state machine is QubitSlot-specific. */
QubitSemanticState get_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx);
bool set_qubit_semantic_state(ASTNode *expr, SemanticContext *ctx,
                              QubitSemanticState new_state);
const char *qubit_state_name(QubitSemanticState state);
Type *type_get_constructed_arg(const Type *type, size_t index);
Type *wrap_constructed(Type *constructor, Type *inner);

Type *type_check_stdlib_call(ASTNode *expr, const char *name,
                             SemanticContext *ctx);

#endif
