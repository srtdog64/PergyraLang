#ifdef PGY_LLVM_ENABLED
#include "llvm_runtime_internal.h"

void
llvm_declare_runtime_raw_collections(LLVMGenCtx *ctx)
{
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64,
                               ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module,
          "pgy_array_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_array_new_raw_export", fn, ft,
          ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr,
                               ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module,
          "pgy_array_push_raw_export", ft);
      llvm_register_function(ctx, "pgy_array_push_raw_export", fn, ft,
          ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64,
                               ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module,
          "pgy_array_get_raw_export", ft);
      llvm_register_function(ctx, "pgy_array_get_raw_export", fn, ft,
          ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64,
                               ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module,
          "pgy_array_set_raw_export", ft);
      llvm_register_function(ctx, "pgy_array_set_raw_export", fn, ft,
          ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module,
          "pgy_array_pop_raw_export", ft);
      llvm_register_function(ctx, "pgy_array_pop_raw_export", fn, ft,
          ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module,
          "pgy_array_drop_storage_raw_export", ft);
      llvm_register_function(ctx, "pgy_array_drop_storage_raw_export", fn, ft,
          ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_new_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_set_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_new_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_new_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_map_new_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_new_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_add_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_add_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_add_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_add_string_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_has_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_has_raw_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_has_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_has_string_raw_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_remove_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_remove_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_remove_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_remove_string_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_size_raw_export", ft);
      llvm_register_function(ctx, "pgy_set_size_raw_export", fn, ft, ctx->type_i32); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_push_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_push_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_push_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_push_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_push_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_push_string_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_queue_push_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_push_string_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_pop_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_pop_string_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_get_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_get_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_list_set_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_set_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_get_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_get_string_raw_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_list_set_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_set_string_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_size_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_size_raw_export", fn, ft, ctx->type_i32);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_size_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_size_raw_export", fn, ft, ctx->type_i32);
      fn = LLVMAddFunction(ctx->module, "pgy_map_size_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_size_raw_export", fn, ft, ctx->type_i32);
      ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
      fn = LLVMAddFunction(ctx->module, "pgy_queue_empty_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_empty_raw_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_remove_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_remove_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_list_remove_string_raw_export", ft);
      llvm_register_function(ctx, "pgy_list_remove_string_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_queue_pop_raw_export", ft);
      llvm_register_function(ctx, "pgy_queue_pop_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_set_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_string_value_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_set_string_value_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_set_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_string_value_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_set_string_value_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_set_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_string_value_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_set_string_value_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_set_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_set_string_value_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_set_string_value_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_get_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_string_value_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_get_string_value_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_get_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_string_value_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_get_string_value_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_get_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_string_value_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_get_string_value_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 4, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_get_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_get_string_value_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_get_string_value_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_has_raw_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_has_raw_i32_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_has_raw_i64_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_has_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_has_raw_bool_export", fn, ft, ctx->type_i1); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_string_value_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_string_value_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i32 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_string_value_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_string_value_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_string_value_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_string_value_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1, ctx->type_i64 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i1 };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_remove_string_value_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_remove_string_value_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_keys_raw_export", ft);
      llvm_register_function(ctx, "pgy_map_keys_raw_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_keys_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_map_keys_raw_i32_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_keys_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_map_keys_raw_i64_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_map_keys_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_map_keys_raw_bool_export", fn, ft, ctx->type_void); }
    { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i8ptr };
      LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
      LLVMValueRef fn = LLVMAddFunction(ctx->module, "pgy_set_values_raw_i32_export", ft);
      llvm_register_function(ctx, "pgy_set_values_raw_i32_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_set_values_raw_i64_export", ft);
      llvm_register_function(ctx, "pgy_set_values_raw_i64_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_set_values_raw_bool_export", ft);
      llvm_register_function(ctx, "pgy_set_values_raw_bool_export", fn, ft, ctx->type_void);
      fn = LLVMAddFunction(ctx->module, "pgy_set_values_raw_string_export", ft);
      llvm_register_function(ctx, "pgy_set_values_raw_string_export", fn, ft, ctx->type_void); }
}

#endif /* PGY_LLVM_ENABLED */
