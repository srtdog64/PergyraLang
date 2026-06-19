#ifndef PGY_LLVM_DOMAIN_STRUCT_FIELDS_H
#define PGY_LLVM_DOMAIN_STRUCT_FIELDS_H

#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"

LLVMTypeRef llvm_zone_effect_pool_struct_type(LLVMGenCtx *ctx,
                                              LLVMTypeRef effect_ty,
                                              int capacity);
LLVMTypeRef llvm_domain_required_ast_type(LLVMGenCtx *ctx,
                                          ASTNode *field_node,
                                          ASTNode *type_node,
                                          const char *field_kind);
LLVMTypeRef llvm_domain_required_class_struct_type(LLVMGenCtx *ctx,
                                                   ASTNode *field_node,
                                                   const char *type_name,
                                                   const char *field_kind);
bool        llvm_domain_add_projection_state_fields(LLVMGenCtx *ctx,
                                                    LLVMClassTypeEntry *entry,
                                                    LLVMTypeRef *ftypes,
                                                    int *field_index,
                                                    const LLVMHostedDomainSlotView *slot_view,
                                                    ASTNode **refreshes,
                                                    size_t refresh_count);
bool        llvm_domain_add_projection_state_fields_from_zone_refresh_view(
                                                    LLVMGenCtx *ctx,
                                                    LLVMClassTypeEntry *entry,
                                                    LLVMTypeRef *ftypes,
                                                    int *field_index,
                                                    const LLVMHostedDomainSlotView *slot_view,
                                                    const LLVMHostedZoneRefreshView *refresh_view);

#endif
