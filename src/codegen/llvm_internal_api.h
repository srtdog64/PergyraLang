/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend private API declarations.
 *
 * This header is intentionally included from llvm_internal.h after the private
 * LLVMGenCtx and registry entry types are defined. Keep implementation bodies
 * out of this file; it is an owner boundary for declarations only.
 */

#ifndef PGY_LLVM_INTERNAL_API_H
#define PGY_LLVM_INTERNAL_API_H

/* =================================================================
 * Context lifecycle (llvm_backend.c)
 * ================================================================= */
LLVMGenCtx *llvm_ctx_create(const char *module_name);
void         llvm_ctx_destroy(LLVMGenCtx *ctx);
bool         llvm_machine_layer_projection_is_bound(const LLVMGenCtx *ctx);

/* =================================================================
 * Scope management (llvm_backend.c)
 * ================================================================= */
void          llvm_scope_push(LLVMGenCtx *ctx);
void          llvm_scope_pop(LLVMGenCtx *ctx);
void          llvm_scope_declare(LLVMGenCtx *ctx, const char *name,
                                  LLVMValueRef alloca, LLVMTypeRef type);
/* Scope entries are owned by llvm_registry.c; callers must snapshot because
 * push/pop/declare can invalidate frame storage and shadowing caches. */
bool          llvm_scope_contains(LLVMGenCtx *ctx, const char *name);
bool          llvm_scope_lookup_snapshot(LLVMGenCtx *ctx, const char *name,
                                         LLVMVarEntry *out);
bool          llvm_scope_frame_entry_is_current(LLVMGenCtx *ctx,
                                                LLVMScopeFrame *frame,
                                                int index);
LLVMLexicalRegistrySnapshot llvm_lexical_registry_snapshot(LLVMGenCtx *ctx);
void          llvm_lexical_registry_restore(
                  LLVMGenCtx *ctx, LLVMLexicalRegistrySnapshot snapshot);
void          llvm_defer_scope_push(LLVMGenCtx *ctx);
void          llvm_defer_scope_pop(LLVMGenCtx *ctx);
void          llvm_register_defer(ASTNode *body, LLVMGenCtx *ctx);
void          llvm_emit_defers_from(LLVMGenCtx *ctx, int from_depth);
void llvm_register_list_var(LLVMGenCtx *ctx, const char *var_name,
                            const char *inner_type);
void llvm_register_list_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                    LLVMValueRef binding,
                                    const char *inner_type);
const char *llvm_lookup_list_inner(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_set_var(LLVMGenCtx *ctx, const char *var_name,
                           const char *inner_type);
void llvm_register_set_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                   LLVMValueRef binding,
                                   const char *inner_type);
const char *llvm_lookup_set_inner(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_queue_var(LLVMGenCtx *ctx, const char *var_name,
                             const char *inner_type);
void llvm_register_queue_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                     LLVMValueRef binding,
                                     const char *inner_type);
const char *llvm_lookup_queue_inner(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_map_var(LLVMGenCtx *ctx, const char *var_name,
                      const char *key_type, const char *value_type);
void llvm_register_map_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                   LLVMValueRef binding,
                                   const char *key_type,
                                   const char *value_type);
const char *llvm_lookup_map_key(LLVMGenCtx *ctx, const char *var_name);
const char *llvm_lookup_map_value(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_callable_var(LLVMGenCtx *ctx, const char *var_name,
                                ASTNode *type_node);
ASTNode *llvm_lookup_callable_var(LLVMGenCtx *ctx, const char *var_name);
void llvm_register_callable_signature(LLVMGenCtx *ctx, const char *var_name,
                                      size_t param_count,
                                      ASTNode *const *param_types,
                                      ASTNode *return_type);
void llvm_register_callable_signature_names(LLVMGenCtx *ctx,
                                            const char *var_name,
                                            size_t param_count,
                                            const char *const *param_type_names,
                                            const char *return_type_name);
void llvm_register_callable_mir_signature(
    LLVMGenCtx *ctx, const char *var_name, size_t param_count,
    const char *const *param_type_names,
    const MIRCallableSig *const *param_callable_sigs,
    /* A NULL return name/signature is the carried MIR void return. */
    const char *return_type_name,
    const MIRCallableSig *return_callable_sig);
void llvm_register_callable_mir_value(LLVMGenCtx *ctx, const char *var_name,
                                      const MIRCallableSig *callable_sig);
LLVMCallableVarEntry *llvm_lookup_callable_entry(LLVMGenCtx *ctx,
                                                 const char *var_name);
void llvm_register_typed_var(LLVMGenCtx *ctx, const char *var_name,
                             ASTNode *type_node);
void llvm_register_typed_var_binding(LLVMGenCtx *ctx, const char *var_name,
                                     LLVMValueRef binding,
                                     ASTNode *type_node);
void llvm_register_typed_var_abi_binding(LLVMGenCtx *ctx,
                                         const char *var_name,
                                         LLVMValueRef binding,
                                         const char *abi_type_name);
/* =================================================================
 * Function registry (llvm_backend.c)
 * ================================================================= */
void           llvm_register_function(LLVMGenCtx *ctx, const char *name,
                                       LLVMValueRef fn, LLVMTypeRef fn_type,
                                       LLVMTypeRef ret_type);
void           llvm_set_function_flags(LLVMGenCtx *ctx, const char *name,
                                       bool is_action, bool action_self_only);
LLVMFuncEntry *llvm_lookup_function(LLVMGenCtx *ctx, const char *name);
LLVMFuncEntry *llvm_lookup_or_declare_function(LLVMGenCtx *ctx, const char *name,
                                               LLVMTypeRef fn_type,
                                               LLVMTypeRef ret_type);
void           llvm_mark_function_as_used(LLVMGenCtx *ctx, const char *name);
LLVMFuncEntry *llvm_required_runtime_function(LLVMGenCtx *ctx,
                                               ASTNode *node,
                                               const char *family_name,
                                               const char *callee_name,
                                               const char *function_name);
bool           llvm_runtime_aggregate_return_is_sret_function(
                                               const char *runtime_name);
bool           llvm_runtime_aggregate_return_apply_decl_shape(
                                               LLVMGenCtx *ctx,
                                               const char *runtime_name,
                                               LLVMTypeRef *ret_type,
                                               LLVMTypeRef params[],
                                               unsigned *param_count,
                                               unsigned param_capacity);
LLVMValueRef   llvm_emit_runtime_aggregate_return_call(
                                               ASTNode *node,
                                               LLVMGenCtx *ctx,
                                               const char *family_name,
                                               const char *callee_name,
                                               const char *runtime_name,
                                               size_t source_arg_count);

/* =================================================================
 * Slot tracking (llvm_backend.c)
 * ================================================================= */
void          llvm_register_slot_var(LLVMGenCtx *ctx, const char *var_name,
                                     const char *inner_type,
                                     bool is_secure);
void          llvm_register_slot_var_binding(LLVMGenCtx *ctx,
                                             const char *var_name,
                                             LLVMValueRef binding,
                                             const char *inner_type,
                                             bool is_secure);
const char   *llvm_lookup_slot_inner(LLVMGenCtx *ctx, const char *var_name);
bool          llvm_lookup_slot_is_secure(LLVMGenCtx *ctx, const char *var_name);
void          llvm_mark_slot_released(LLVMGenCtx *ctx, const char *var_name);
void          llvm_register_view_var(LLVMGenCtx *ctx, const char *var_name,
                                     const char *source_slot,
                                     const char *inner_type,
                                     bool is_move_token);
void          llvm_register_view_var_binding(LLVMGenCtx *ctx,
                                             const char *var_name,
                                             LLVMValueRef binding,
                                             const char *source_slot,
                                             const char *inner_type,
                                             bool is_move_token);
LLVMViewVarEntry *llvm_lookup_view_var(LLVMGenCtx *ctx, const char *var_name);
LLVMTypeRef   llvm_slot_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef   llvm_device_slot_struct_type(LLVMGenCtx *ctx,
                                           const char *inner);
LLVMTypeRef   llvm_pinned_slot_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef   llvm_secure_slot_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef   llvm_pinned_secure_slot_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef   llvm_secure_token_type(LLVMGenCtx *ctx, const char *inner);
void          llvm_register_device_slot_var(LLVMGenCtx *ctx, const char *var_name,
                                             const char *inner_type);
void          llvm_register_device_slot_var_binding(LLVMGenCtx *ctx,
                                                     const char *var_name,
                                                     LLVMValueRef binding,
                                                     const char *inner_type);
const char   *llvm_lookup_device_slot_inner(LLVMGenCtx *ctx,
                                             const char *var_name);
void          llvm_mark_device_slot_released(LLVMGenCtx *ctx,
                                              const char *var_name);
bool          llvm_lookup_secure_token_var(LLVMGenCtx *ctx,
                                           const char *slot_name,
                                           LLVMVarEntry *out);
void          llvm_register_future_var(LLVMGenCtx *ctx, const char *var_name,
                                        const char *inner_type,
                                        bool is_remote);
void          llvm_register_future_var_binding(LLVMGenCtx *ctx,
                                                const char *var_name,
                                                LLVMValueRef binding,
                                                const char *inner_type,
                                                bool is_remote);
const char   *llvm_lookup_future_inner(LLVMGenCtx *ctx, const char *var_name);
bool          llvm_lookup_future_is_remote(LLVMGenCtx *ctx,
                                            const char *var_name);
void          llvm_register_channel_var(LLVMGenCtx *ctx, const char *var_name,
                                        const char *inner_type);
void          llvm_register_channel_var_binding(LLVMGenCtx *ctx,
                                                 const char *var_name,
                                                 LLVMValueRef binding,
                                                 const char *inner_type);
const char   *llvm_lookup_channel_inner(LLVMGenCtx *ctx, const char *var_name);
bool          llvm_resolve_channel_target(LLVMGenCtx *ctx, ASTNode *node,
                                          ASTNode *channel,
                                          const char *operation_name,
                                          LLVMChannelTarget *out);
const char   *llvm_resolve_channel_target_inner(LLVMGenCtx *ctx,
                                                 ASTNode *node,
                                                 ASTNode *channel,
                                                 const char *operation_name);
void          llvm_register_rc_var(LLVMGenCtx *ctx, const char *var_name,
                                   const char *inner_type);
void          llvm_register_rc_var_binding(LLVMGenCtx *ctx,
                                           const char *var_name,
                                           LLVMValueRef binding,
                                           const char *inner_type);
const char   *llvm_lookup_rc_inner(LLVMGenCtx *ctx, const char *var_name);
void          llvm_register_weak_var(LLVMGenCtx *ctx, const char *var_name,
                                     const char *inner_type);
void          llvm_register_weak_var_binding(LLVMGenCtx *ctx,
                                             const char *var_name,
                                             LLVMValueRef binding,
                                             const char *inner_type);
const char   *llvm_lookup_weak_inner(LLVMGenCtx *ctx, const char *var_name);

/* =================================================================
 * Class type registry (llvm_backend.c)
 * ================================================================= */
LLVMClassTypeEntry *llvm_register_class(LLVMGenCtx *ctx, const char *class_name,
                                          LLVMTypeRef struct_type,
                                          bool is_subject,
                                          bool is_pointer_self_host);
void                llvm_class_add_field(LLVMClassTypeEntry *entry,
                                          const char *field_name,
                                          LLVMTypeRef field_type, int index);
void                llvm_class_add_field_ex(LLVMClassTypeEntry *entry,
                                            const char *field_name,
                                            LLVMTypeRef field_type, int index,
                                            bool is_subject_slot);
LLVMClassTypeEntry *llvm_lookup_class(LLVMGenCtx *ctx, const char *class_name);
LLVMClassTypeEntry *llvm_lookup_class_by_struct_type(LLVMGenCtx *ctx,
                                                     LLVMTypeRef struct_type);
LLVMClassTypeEntry *llvm_lookup_vtable_class_with_method(LLVMGenCtx *ctx,
                                                         const char *method_name,
                                                         int *out_method_index);
int                 llvm_class_field_index(LLVMClassTypeEntry *entry,
                                            const char *field_name);
LLVMTypeRef         llvm_class_field_type_at_index(LLVMClassTypeEntry *entry,
                                                    int struct_index);
int                 llvm_class_field_count(LLVMClassTypeEntry *entry);
const char         *llvm_class_field_name_at(LLVMClassTypeEntry *entry,
                                             int ordinal);
LLVMTypeRef         llvm_class_field_type_at(LLVMClassTypeEntry *entry,
                                             int ordinal);
int                 llvm_class_field_struct_index_at(LLVMClassTypeEntry *entry,
                                                     int ordinal);
bool                llvm_class_field_is_subject_slot_at(
                        LLVMClassTypeEntry *entry,
                        int ordinal);
void                llvm_register_var_class(LLVMGenCtx *ctx, const char *var_name,
                                            const char *class_name);
const char         *llvm_lookup_var_class(LLVMGenCtx *ctx, const char *var_name);
void                llvm_register_projection_borrow(LLVMGenCtx *ctx,
                                                    const char *var_name,
                                                    const char *class_name,
                                                    const char *source_name);
LLVMProjectionBorrowEntry *llvm_lookup_projection_borrow(LLVMGenCtx *ctx,
                                                         const char *var_name);
void                llvm_register_array_var(LLVMGenCtx *ctx, const char *var_name,
                                             LLVMTypeRef elem_type,
                                             const char *elem_name,
                                             int64_t length);
void                llvm_register_array_var_binding(LLVMGenCtx *ctx,
                                                     const char *var_name,
                                                     LLVMValueRef binding,
                                                     LLVMTypeRef elem_type,
                                                     const char *elem_name,
                                                     int64_t length);
LLVMArrayVarEntry  *llvm_lookup_array_var(LLVMGenCtx *ctx, const char *var_name);
LLVMArrayVarEntry  *llvm_lookup_array_var_binding(LLVMGenCtx *ctx,
                                                   const char *var_name,
                                                   LLVMValueRef binding);
const char         *llvm_array_access_element_class_name(LLVMGenCtx *ctx,
                                                         ASTNode *array_access);
void                llvm_register_enum_variant(LLVMGenCtx *ctx,
                                                const char *enum_name,
                                                const char *variant_name,
                                                int value);
bool                llvm_enum_type_exists(LLVMGenCtx *ctx,
                                          const char *enum_name);
LLVMEnumVariantEntry *llvm_lookup_enum_variant(LLVMGenCtx *ctx,
                                                const char *variant_name);
LLVMEnumVariantEntry *llvm_lookup_enum_variant_qualified(LLVMGenCtx *ctx,
                                                          const char *enum_name,
                                                          const char *variant_name);

/* =================================================================
 * Event type registry (llvm_backend.c)
 * ================================================================= */
LLVMEventTypeEntry *llvm_lookup_event(LLVMGenCtx *ctx, const char *name);
LLVMEventTypeEntry *llvm_register_event(LLVMGenCtx *ctx, const char *name,
                                          LLVMTypeRef struct_type,
                                          int param_count, LLVMTypeRef *param_types);
int llvm_event_type_count(const LLVMGenCtx *ctx);
LLVMEventTypeEntry *llvm_event_type_at(LLVMGenCtx *ctx, int index);

/* =================================================================
 * Type helpers (llvm_backend.c)
 * ================================================================= */
LLVMTypeRef   pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);
LLVMTypeRef   ast_type_to_llvm(LLVMGenCtx *ctx, ASTNode *type_node);
LLVMTypeRef   llvm_resolve_inner_type(LLVMGenCtx *ctx, const char *type_name);
bool          llvm_constructed_arg_name_copy(const char *type_name,
                                             int arg_index,
                                             char *out,
                                             size_t out_size);
const char   *llvm_tmp_name(LLVMGenCtx *ctx);
LLVMValueRef  llvm_create_entry_alloca(LLVMGenCtx *ctx, LLVMTypeRef type,
                                        const char *name);
char         *llvm_stmt_render_type_arg(GenericParam *param);
char         *llvm_stmt_render_type_arg_scratch(GenericParam *param,
                                                PgyArena *arena);
const char   *llvm_keep_rendered_persistent(LLVMGenCtx *ctx, char *rendered,
                                            const char *oom_context);
ASTNode      *llvm_stmt_find_function_decl_by_name(LLVMGenCtx *ctx,
                                                   const char *name);
bool          llvm_mir_base_name_from_versioned(const char *mir_name,
                                                char *base_out,
                                                size_t base_out_size);
bool          llvm_mir_declare_recv_target(const char *target_name,
                                           ASTNode *recv_expr,
                                           LLVMGenCtx *ctx);
bool          llvm_mir_emit_channel_receive_def(const MIRInstruction *inst,
                                                LLVMGenCtx *ctx,
                                                LLVMValueRef mir_alloca);
void          llvm_mir_emit_with_claim_only(const MIRInstruction *inst,
                                            LLVMGenCtx *ctx);
void          llvm_mir_emit_with_release_only(const MIRInstruction *inst,
                                              LLVMGenCtx *ctx);
bool          llvm_mir_emit_borrow_view_alias(const MIRInstruction *inst,
                                              LLVMGenCtx *ctx);
bool          llvm_mir_emit_pin_enter(const MIRBasicBlock *block,
                                      LLVMGenCtx *ctx);
bool          llvm_mir_emit_pin_exit(const MIRBasicBlock *block,
                                     LLVMGenCtx *ctx);
bool          llvm_mir_emit_for_loop_init(const MIRInstruction *inst,
                                          LLVMGenCtx *ctx);
LLVMValueRef  llvm_mir_emit_for_loop_condition(const MIRInstruction *inst,
                                                LLVMGenCtx *ctx);
bool          llvm_mir_emit_for_in_loop_init(const MIRInstruction *inst,
                                             LLVMGenCtx *ctx);
LLVMValueRef  llvm_mir_emit_for_in_loop_condition(const MIRInstruction *inst,
                                                   LLVMGenCtx *ctx);
bool          llvm_mir_emit_for_in_body_binding(const MIRRoutine *routine,
                                                const MIRBasicBlock *block,
                                                LLVMGenCtx *ctx);
bool          llvm_mir_emit_for_in_loop_increment(const MIRInstruction *inst,
                                                  LLVMGenCtx *ctx);
bool          llvm_mir_emit_for_loop_body_binding(const MIRRoutine *routine,
                                                  const MIRBasicBlock *block,
                                                  LLVMGenCtx *ctx);
bool          llvm_mir_emit_loop_backedge_increment(const MIRRoutine *routine,
                                                    const MIRBasicBlock *mir_block,
                                                    LLVMGenCtx *ctx);
LLVMValueRef  llvm_mir_emit_select_dispatch_condition(const MIRInstruction *inst,
                                                       const MIRRoutine *routine,
                                                       size_t target_block,
                                                       LLVMGenCtx *ctx);
LLVMValueRef  llvm_mir_emit_match_case_condition(const MIRInstruction *inst,
                                                 LLVMGenCtx *ctx);
bool          llvm_mir_emit_match_case_body_binding(
                                                  const MIRRoutine *routine,
                                                  const MIRBasicBlock *block,
                                                  LLVMGenCtx *ctx);
bool          llvm_mir_remap_active_match_bindings(
                                                  const MIRRoutine *routine,
                                                  const MIRBasicBlock *block,
                                                  LLVMGenCtx *ctx);
void          llvm_mir_emit_owner_sync_exit(LLVMGenCtx *ctx,
                                            LLVMClassTypeEntry *owner_cls,
                                            LLVMFuncEntry *owner_sync,
                                            const char *owner_name);
void          llvm_mir_region_scope_begin(LLVMGenCtx *ctx,
                                          const MIRRoutine *routine);
void          llvm_mir_region_scope_destroy(LLVMGenCtx *ctx);
void          llvm_mir_region_scope_end(LLVMGenCtx *ctx);
LLVMTypeRef   llvm_stmt_infer_expr_type(LLVMGenCtx *ctx, ASTNode *expr);
bool          llvm_stmt_require_non_void_value(LLVMGenCtx *ctx,
                                               ASTNode *expr,
                                               const char *message);
LLVMTypeRef   llvm_stmt_resolve_array_elem_type(LLVMGenCtx *ctx, ASTNode *expr,
                                                LLVMValueRef data_ptr);
void          llvm_emit_let_destructure_stmt(ASTNode *node, LLVMGenCtx *ctx);
void          llvm_emit_mir_destructure_inst(const MIRInstruction *inst,
                                             LLVMGenCtx *ctx);
LLVMClassTypeEntry *llvm_stmt_lookup_class_by_type(LLVMGenCtx *ctx,
                                                   LLVMTypeRef type);
const char   *llvm_stmt_infer_nominal_name_from_init(LLVMGenCtx *ctx,
                                                     ASTNode *init);

/* Generic monomorphization helpers (llvm_backend_generic.c). */
bool        llvm_mono_already_emitted(LLVMGenCtx *ctx, const char *mangled);
void        llvm_register_mono(LLVMGenCtx *ctx, const char *mangled);
void        llvm_type_subst_restore_owned(LLVMGenCtx *ctx, int saved_count);
const char *llvm_type_to_suffix(LLVMGenCtx *ctx, LLVMTypeRef ty);

/* Error reporting helpers (llvm_backend.c). */
void llvm_set_error(LLVMGenCtx *ctx, const char *fmt, ...);
void llvm_set_error_at(LLVMGenCtx *ctx, ASTNode *node, const char *fmt, ...);
void llvm_set_error_with_code(LLVMGenCtx *ctx, const char *code,
                              const char *fmt, ...);
void llvm_set_error_at_with_code(LLVMGenCtx *ctx, ASTNode *node,
                                  const char *code, const char *fmt, ...);
void llvm_set_error_with_hints(LLVMGenCtx *ctx, const char *code,
                                const char *cause_ir,
                                const char *fix_source,
                                const char *fmt, ...);
void llvm_set_error_at_with_hints(LLVMGenCtx *ctx, ASTNode *node,
                                   const char *code,
                                   const char *cause_ir,
                                   const char *fix_source,
                                   const char *fmt, ...);
void llvm_set_mir_inventory_missing(LLVMGenCtx *ctx, const char *fmt, ...);
void llvm_set_mir_topology_invalid(LLVMGenCtx *ctx, const char *fmt, ...);
void llvm_set_mir_intent_carrier_missing(LLVMGenCtx *ctx, const char *fmt, ...);
void llvm_set_mir_memory_exhausted(LLVMGenCtx *ctx, const char *fmt, ...);

/* =================================================================
 * Result helpers (llvm_backend.c)
 * ================================================================= */
LLVMGenResult *llvm_result_error(const char *message);
LLVMGenResult *llvm_result_error_with_hints(const char *message,
                                            const char *code,
                                            const char *cause_ir,
                                            const char *fix_source);
LLVMGenResult *llvm_result_error_fmt(const char *fmt, ...);
LLVMGenResult *llvm_result_error_fmt_with_hints(const char *code,
                                                const char *cause_ir,
                                                const char *fix_source,
                                                const char *fmt, ...);
LLVMGenResult *llvm_result_success(char *ir_text);

/* =================================================================
 * Pipeline helpers (llvm_backend.c / llvm_api.c)
 * ================================================================= */
LLVMGenResult *llvm_validate_mir_for_codegen(const MIRProgram *mir);
bool llvm_emit_program_from_mir(const MIRProgram *mir, LLVMGenCtx *ctx);
void llvm_declare_runtime(LLVMGenCtx *ctx);
bool llvm_can_forward_declare_func_early(LLVMGenCtx *ctx, ASTNode *func);
const MIRRoutine *llvm_active_function_routine_by_name(
    const LLVMGenCtx *ctx,
    const char *name);
bool llvm_nominal_uses_immutable_projection_storage(NominalDeclKind kind);
bool llvm_nominal_is_boundary_transfer_contract(NominalDeclKind kind);
void llvm_forward_declare_intent(ASTNode *node, LLVMGenCtx *ctx);
void llvm_emit_intent_decl(ASTNode *node, LLVMGenCtx *ctx);
void llvm_forward_declare_intent_routines_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory);
void llvm_emit_intent_routines_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory);
void llvm_emit_main_wrapper(LLVMGenCtx *ctx);
bool llvm_emit_class_method_bodies_from_inventory(LLVMGenCtx *ctx);
void llvm_register_enum_decl(LLVMGenCtx *ctx, ASTNode *stmt);
void llvm_register_nominal_decl(LLVMGenCtx *ctx, ASTNode *stmt);
void llvm_register_active_nominal_types(LLVMGenCtx *ctx);
void llvm_register_active_nominal_methods(LLVMGenCtx *ctx);
void llvm_register_active_extern_prototypes(LLVMGenCtx *ctx);
bool llvm_type_name_uses_pointer_self(LLVMGenCtx *ctx, const char *type_name);
const char *llvm_current_zone_slot_type_name(LLVMGenCtx *ctx,
                                             const char *slot_name);
const char *llvm_current_field_class_name(LLVMGenCtx *ctx,
                                          const char *field_name);
bool llvm_ast_type_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *type_node);

#include "llvm_internal_emit_api.h"

#endif /* PGY_LLVM_INTERNAL_API_H */
