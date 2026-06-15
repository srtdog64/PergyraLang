#ifndef PGY_LLVM_INTENT_INTERNAL_H
#define PGY_LLVM_INTENT_INTERNAL_H

#include "intent_binding_metadata_view.h"
#include "llvm_internal.h"

typedef struct LLVMIntentStepContext {
    ASTNode *pre_expr;
    ASTNode *guard_expr;
    ASTNode *post_expr;
    ASTNode *expect_expr;
    ASTNode *invariant_pre_expr;
    ASTNode *invariant_post_expr;
    ASTNode **on_exprs;
    size_t on_expr_count;
    ASTNode *subintent_expr;
    const char *zone_type_name;
    const char *zone_alias;
    const char *from_alias;
    const char *causes_effect;
    const char **who_aliases;
    size_t who_alias_count;
    const char **authorized_aliases;
    size_t authorized_alias_count;
    const char **dispatch_aliases;
    size_t dispatch_alias_count;
} LLVMIntentStepContext;

const char *llvm_intent_involves_type_name(ASTNode *involves);
const char *llvm_intent_step_effective_zone_alias(ASTNode *step);
ASTNode   **llvm_build_mir_intent_step_sources(ASTNode *intent,
                                               const char **step_names,
                                               size_t step_count,
                                               LLVMGenCtx *ctx);
bool        llvm_intent_action_function_name(LLVMGenCtx *ctx,
                                             char *out,
                                             size_t out_size,
                                             const char *subject_name,
                                             const char *step_name);
bool        llvm_intent_reason_name(LLVMGenCtx *ctx,
                                    char *out,
                                    size_t out_size,
                                    const char *prefix,
                                    const char *step_name);
bool        llvm_emit_intent_predicate_check(LLVMGenCtx *ctx,
                                             LLVMValueRef fn,
                                             LLVMBasicBlockRef fail_bb,
                                             LLVMValueRef fail_reason_alloca,
                                             ASTNode *expr,
                                             const char *reason_prefix,
                                             const char *step_name,
                                             const char *ok_block_name);
bool        llvm_intent_step_context_load(LLVMGenCtx *ctx,
                                          ASTNode *intent,
                                          const MIRRoutine *mir_routine,
                                          ASTNode *step,
                                          const char *step_name,
                                          bool mir_only_intent,
                                          LLVMIntentStepContext *out);
void        llvm_emit_intent_entry_bindings(LLVMGenCtx *ctx,
                                            ASTNode *node,
                                            LLVMValueRef fn,
                                            const IntentBindingMetadataView *bindings_view,
                                            size_t param_count,
                                            bool mir_only_intent,
                                            LLVMValueRef *subjects_ptr_out,
                                            size_t *subject_count_out);
bool        llvm_emit_intent_cleanup_tail(LLVMGenCtx *ctx,
                                          ASTNode *node,
                                          const MIRRoutine *mir_routine,
                                          ASTNode **step_nodes,
                                          const char **mir_step_names,
                                          LLVMValueRef *completed_allocas,
                                          size_t step_count,
                                          bool mir_only_intent,
                                          LLVMValueRef handle_alloca,
                                          LLVMValueRef failed_alloca,
                                          LLVMBasicBlockRef compensate_bb,
                                          LLVMBasicBlockRef maybe_exit_bb,
                                          LLVMBasicBlockRef do_exit_bb,
                                          LLVMBasicBlockRef ret_bb,
                                          LLVMFuncEntry *exit_fn);
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
size_t      llvm_collect_mir_intent_bindings(
                const MIRRoutine *routine,
                LLVMGenCtx *ctx,
                IntentBindingMetadataView *bindings_out);

const char *llvm_intent_zone_binding_type_name(LLVMGenCtx *ctx,
                                               const char *alias);
bool        llvm_intent_zone_sync_name(char *out,
                                       size_t out_size,
                                       const char *zone_type_name);
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
void        llvm_emit_intent_trace_step(LLVMGenCtx *ctx,
                                        LLVMFuncEntry *trace_step_fn,
                                        LLVMValueRef handle_alloca,
                                        const char *step_name,
                                        const char *zone_type_name);
void        llvm_emit_intent_trace_bindings(LLVMGenCtx *ctx,
                                            ASTNode *intent,
                                            LLVMFuncEntry *trace_bind_fn,
                                            LLVMValueRef handle_alloca,
                                            const LLVMIntentStepContext *step_ctx);
void        llvm_emit_intent_trace_step_ok(LLVMGenCtx *ctx,
                                           LLVMFuncEntry *trace_step_ok_fn,
                                           LLVMValueRef handle_alloca,
                                           const char *step_name);
void        llvm_emit_intent_trace_failure(LLVMGenCtx *ctx,
                                           LLVMFuncEntry *trace_fail_fn,
                                           LLVMValueRef handle_alloca,
                                           LLVMValueRef fail_reason_alloca);
const MIRRoutine *llvm_find_mir_intent_routine(const LLVMGenCtx *ctx,
                                               ASTNode *intent_decl);
void        llvm_emit_mir_resource_hook(LLVMGenCtx *ctx,
                                        const MIRInstruction *inst,
                                        LLVMValueRef handle,
                                        bool cleanup_hook);
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
