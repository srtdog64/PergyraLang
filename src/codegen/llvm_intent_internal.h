#ifndef PGY_LLVM_INTENT_INTERNAL_H
#define PGY_LLVM_INTENT_INTERNAL_H

#include "llvm_internal.h"

const char *llvm_find_mir_intent_meta_arg(const MIRRoutine *routine,
                                          const char *step_name,
                                          const char *inst_name);
bool        llvm_mir_intent_has_stmt(const MIRRoutine *routine,
                                     const char *step_name,
                                     const char *inst_name,
                                     const char *arg0);
size_t      llvm_collect_mir_intent_who_aliases(const MIRRoutine *routine,
                                                LLVMGenCtx *ctx,
                                                const char *step_name,
                                                const char ***aliases_out);
size_t      llvm_collect_mir_intent_authorized_aliases(const MIRRoutine *routine,
                                                       LLVMGenCtx *ctx,
                                                       const char *step_name,
                                                       const char ***aliases_out);
size_t      llvm_collect_mir_intent_participants(const MIRRoutine *routine,
                                                 LLVMGenCtx *ctx,
                                                 const char ***aliases_out,
                                                 const char ***types_out);

const char *llvm_intent_zone_binding_type_name(LLVMGenCtx *ctx,
                                               const char *alias);
const char *llvm_resolve_intent_zone_slot_name_for_zone(LLVMGenCtx *ctx,
                                                        ASTNode *intent,
                                                        const char *zone_type_name,
                                                        const char *alias);
void        llvm_emit_intent_step_bind_bound_zone(LLVMGenCtx *ctx,
                                                  ASTNode *intent,
                                                  const char *zone_type_name,
                                                  const char *zone_alias,
                                                  const char *from_alias,
                                                  const char **who_aliases,
                                                  size_t who_alias_count);
bool        llvm_emit_intent_step_rebind_bound_zone_aliases(LLVMGenCtx *ctx,
                                                            ASTNode *intent,
                                                            const char *zone_type_name,
                                                            const char *zone_alias,
                                                            const char **who_aliases,
                                                            size_t who_alias_count,
                                                            LLVMValueRef *saved_allocas);
void        llvm_emit_intent_step_dirty_zone_projections(LLVMGenCtx *ctx,
                                                         const char *zone_type_name,
                                                         const char *zone_alias);
void        llvm_emit_intent_step_sync_effective_zone(LLVMGenCtx *ctx,
                                                      const char *zone_type_name,
                                                      const char *zone_alias);
void        llvm_emit_intent_step_restore_bound_zone_aliases(LLVMGenCtx *ctx,
                                                             ASTNode *intent,
                                                             const char *zone_type_name,
                                                             const char **who_aliases,
                                                             size_t who_alias_count,
                                                             LLVMValueRef *saved_allocas);
void        llvm_emit_intent_step_mark_caused_effect(LLVMGenCtx *ctx,
                                                     const char *zone_type_name,
                                                     const char *zone_alias,
                                                     const char *causes_effect);
const char *llvm_infer_intent_step_causes_from_on_exprs(LLVMGenCtx *ctx,
                                                        ASTNode **on_exprs,
                                                        size_t on_expr_count);
const MIRRoutine *llvm_find_mir_intent_routine(const LLVMGenCtx *ctx,
                                               ASTNode *intent_decl);
void        llvm_emit_mir_resource_hook(LLVMGenCtx *ctx,
                                        const MIRInstruction *inst,
                                        LLVMValueRef handle,
                                        bool cleanup_hook);
size_t      llvm_collect_mir_intent_steps(const MIRRoutine *routine,
                                          LLVMGenCtx *ctx,
                                          ASTNode ***steps_out);
size_t      llvm_collect_mir_intent_step_names(const MIRRoutine *routine,
                                               LLVMGenCtx *ctx,
                                               const char ***names_out);
ASTNode    *llvm_find_mir_intent_check_expr(const MIRRoutine *routine,
                                            const char *step_name,
                                            const char *phase_name);
size_t      llvm_collect_mir_intent_eval_exprs(const MIRRoutine *routine,
                                               LLVMGenCtx *ctx,
                                               const char *step_name,
                                               const char *phase_name,
                                               ASTNode ***exprs_out);
ASTNode    *llvm_find_mir_intent_eval_expr(const MIRRoutine *routine,
                                           LLVMGenCtx *ctx,
                                           const char *step_name,
                                           const char *phase_name);
size_t      llvm_collect_mir_intent_dispatch_aliases(const MIRRoutine *routine,
                                                     LLVMGenCtx *ctx,
                                                     const char *step_name,
                                                     const char ***aliases_out);
void        llvm_emit_intent_step_validate_authority(LLVMGenCtx *ctx,
                                                     LLVMValueRef fn,
                                                     LLVMBasicBlockRef fail_bb,
                                                     LLVMValueRef fail_reason_alloca,
                                                     const char *step_name,
                                                     const char *zone_type_name,
                                                     const char *zone_alias,
                                                     const char **authorized_aliases,
                                                     size_t authorized_alias_count);

#endif
