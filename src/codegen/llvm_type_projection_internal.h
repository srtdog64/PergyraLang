/* LLVM aggregate type and size projection declarations. */
#ifndef PGY_LLVM_TYPE_PROJECTION_INTERNAL_H
#define PGY_LLVM_TYPE_PROJECTION_INTERNAL_H

LLVMTypeRef llvm_array_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_nested_array_struct_type(LLVMGenCtx *ctx, const char *inner);
const char *llvm_scalar_array_elem_suffix(LLVMGenCtx *ctx,
                                          LLVMTypeRef elem_type);
LLVMTypeRef llvm_scalar_array_struct_element_type(LLVMGenCtx *ctx,
                                                  LLVMTypeRef type);
LLVMTypeRef llvm_slice_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_list_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_set_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_queue_struct_type(LLVMGenCtx *ctx, const char *inner);
LLVMTypeRef llvm_hashmap_struct_type(LLVMGenCtx *ctx, const char *value);
LLVMValueRef llvm_sizeof_type_i64(LLVMGenCtx *ctx, LLVMTypeRef type);

#endif
