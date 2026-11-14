#ifndef PGY_LLVM_MIR_LOCAL_EMIT_H
#define PGY_LLVM_MIR_LOCAL_EMIT_H

#include "llvm_internal.h"
#include "llvm_mir_vars.h"

void llvm_emit_mir_local_allocas(const MIRRoutine *routine,
                                 LLVMGenCtx *ctx,
                                 LLVMMirVar **vars_ptr,
                                 size_t *var_capacity_ptr,
                                 size_t *var_count_ptr);
void llvm_emit_mir_param_allocas(const MIRRoutine *routine,
                                 ASTNode *func_decl,
                                 LLVMValueRef fn,
                                 LLVMGenCtx *ctx,
                                 bool is_intent,
                                 bool is_method,
                                 LLVMClassTypeEntry *owner_cls,
                                 const char *owner_name,
                                 size_t param_count);
void llvm_register_mir_param_ssa_aliases(const MIRRoutine *routine,
                                         LLVMGenCtx *ctx,
                                         LLVMMirVar **vars_ptr,
                                         size_t *var_capacity_ptr,
                                         size_t *var_count_ptr);
void llvm_register_class_field_slots(LLVMGenCtx *ctx, const char *owner_name);
bool llvm_emit_mir_mut_ref_writebacks(const MIRRoutine *routine,
                                      const MIRBasicBlock *block,
                                      LLVMMirVar *vars,
                                      size_t var_count,
                                      LLVMGenCtx *ctx);

#endif
