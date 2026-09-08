#ifndef PGY_CALLABLE_CAPABILITY_INFERENCE_H
#define PGY_CALLABLE_CAPABILITY_INFERENCE_H

#include "type_checker.h"

/* Per-analysis typed call equations. Source bodies are checked once; the seal
 * substitutes callable identities, never rechecks a callee AST. */
typedef struct CallableCapabilityRoutine CallableCapabilityRoutine;
void callable_capability_program_begin(SemanticContext *ctx, ASTNode *program);
CallableCapabilityRoutine *callable_capability_enter(
    SemanticContext *ctx, ASTNode *decl, Type **params, size_t count);
void callable_capability_leave(SemanticContext *ctx,
    CallableCapabilityRoutine *previous, Type *type, uint32_t direct_mask,
    uint32_t direct_effects);
void callable_capability_set_effect_contract(SemanticContext *ctx,
    uint32_t declared_effects, bool has_contract);
void type_check_function_effect_contract(ASTNode *node, SemanticContext *ctx,
    uint32_t declared_effects, bool has_effect_contract, uint32_t derived_effects);
void callable_capability_record_call(SemanticContext *ctx, ASTNode *call,
    Symbol *callee, Type **actual_types);
void callable_capability_record_binding(SemanticContext *ctx,
    Symbol *binding, ASTNode *value);
void callable_capability_invalidate_binding(SemanticContext *ctx, Symbol *binding);
void callable_capability_record_return(SemanticContext *ctx, ASTNode *value);
void callable_capability_record_subscription(SemanticContext *ctx,
    ASTNode *event, ASTNode *handler);
void callable_capability_record_method_call(SemanticContext *ctx,
    ASTNode *call, Type **params, Type **actual_types);
bool callable_capability_seal(SemanticContext *ctx);
void callable_capability_destroy(SemanticContext *ctx);

#endif
