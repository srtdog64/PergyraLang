#ifndef PERGYRA_TYPE_CHECKER_OWNERSHIP_LET_INTERNAL_H
#define PERGYRA_TYPE_CHECKER_OWNERSHIP_LET_INTERNAL_H

#include <stdbool.h>

#include "type_checker_internal.h"

Type *ownership_let_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx);
Type *ownership_let_resolve_first_call_type_arg(ASTNode *call, SemanticContext *ctx);
bool  ownership_let_try_claim_slot_decl(ASTNode *node,
                                        SemanticContext *ctx,
                                        const char *name,
                                        ASTNode *init,
                                        ASTNode *ann,
                                        bool *handled);
bool  ownership_let_view_init_info(ASTNode *init,
                                   const char **source_slot,
                                   bool *is_write_view);
bool  ownership_let_find_conflicting_view(Scope *scope,
                                          const char *source_slot,
                                          bool new_write_view,
                                          const char **existing_name,
                                          const char **existing_kind);
bool  ownership_let_try_declare_view_binding(ASTNode *node,
                                             SemanticContext *ctx,
                                             const char *name,
                                             Type *decl_type,
                                             ASTNode *init,
                                             bool *handled);
bool  ownership_let_is_unresolved_none_option(const Type *type);
bool  ownership_let_is_unresolved_empty_array(const Type *type);
bool  ownership_let_is_unresolved_empty_set(const Type *type);
bool  ownership_let_is_unresolved_device_slot(const Type *type);
bool  ownership_let_validate_builtin_owner_binding(ASTNode *node,
                                                    SemanticContext *ctx,
                                                    Type *decl_type,
                                                    ASTNode *init);

#endif
