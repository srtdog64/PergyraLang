#ifndef PERGYRA_TYPE_CHECKER_BUILTINS_SLOTOPS_H
#define PERGYRA_TYPE_CHECKER_BUILTINS_SLOTOPS_H

#include <stdbool.h>
#include "../parser/ast.h"
#include "type_system.h"

typedef struct SemanticContext SemanticContext;

Type *type_check_claim_slot(ASTNode *call, SemanticContext *ctx);
Type *type_check_claim_device_slot(ASTNode *call, SemanticContext *ctx);
Type *type_check_view_read(ASTNode *call, SemanticContext *ctx);
Type *type_check_view_write(ASTNode *call, SemanticContext *ctx);
Type *type_check_move_token(ASTNode *call, SemanticContext *ctx);
bool type_check_write_slot(ASTNode *call, SemanticContext *ctx);
Type *type_check_read_slot(ASTNode *call, SemanticContext *ctx);
bool type_check_release_slot(ASTNode *call, SemanticContext *ctx);
Type *type_check_device_handle_arg(ASTNode *expr,
                                   SemanticContext *ctx,
                                   const char *builtin_name,
                                   bool allow_released);

#endif /* PERGYRA_TYPE_CHECKER_BUILTINS_SLOTOPS_H */
