#ifndef PERGYRA_CODEGEN_SCALAR_ARITHMETIC_POLICY_H
#define PERGYRA_CODEGEN_SCALAR_ARITHMETIC_POLICY_H

#include <stdbool.h>

typedef struct ASTNode ASTNode;

bool pgy_codegen_ast_number_is_safe_divisor_i32_literal(const ASTNode *node);

#endif /* PERGYRA_CODEGEN_SCALAR_ARITHMETIC_POLICY_H */
