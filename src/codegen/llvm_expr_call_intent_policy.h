#ifndef PERGYRA_LLVM_EXPR_CALL_INTENT_POLICY_H
#define PERGYRA_LLVM_EXPR_CALL_INTENT_POLICY_H

#ifdef PGY_LLVM_ENABLED

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"

ASTNode *llvm_intent_call_binding_at(ASTNode *intent_decl, size_t index,
                                     size_t *binding_count_out);
bool llvm_intent_call_arg_can_take_subject_address(ASTNode *arg_node);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_EXPR_CALL_INTENT_POLICY_H */
