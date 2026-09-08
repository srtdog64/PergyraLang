#ifndef PERGYRA_MIR_TYPE_HELPERS_H
#define PERGYRA_MIR_TYPE_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#include "../parser/ast.h"

char *mir_claim_abi_type_name_from_ast(const ASTNode *ast);
char *mir_capture_type_name(ASTNode *type_node, const char *type_name);
char *mir_render_type_name(ASTNode *type_node);
char *mir_render_substituted_type_name(ASTNode *type_node,
                                       char *const *generic_param_names,
                                       char *const *actual_type_names,
                                       size_t binding_count);
char *mir_substitute_type_name_text(const char *type_name,
                                   const char *const *formal_names,
                                   const char *const *actual_names,
                                   size_t binding_count);

#endif
