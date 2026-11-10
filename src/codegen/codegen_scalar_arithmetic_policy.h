#ifndef PERGYRA_CODEGEN_SCALAR_ARITHMETIC_POLICY_H
#define PERGYRA_CODEGEN_SCALAR_ARITHMETIC_POLICY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;

bool pgy_codegen_ast_number_is_safe_divisor_i32_literal(const ASTNode *node);
bool pgy_codegen_i32_divisor_is_safe(int64_t value);

#endif /* PERGYRA_CODEGEN_SCALAR_ARITHMETIC_POLICY_H */
