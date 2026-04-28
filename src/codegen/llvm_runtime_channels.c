#ifdef PGY_LLVM_ENABLED
#include "llvm_runtime_internal.h"

void
llvm_declare_runtime_channels(LLVMGenCtx *ctx)
{
    struct {
        const char *suffix;
        LLVMTypeRef val_type;
    } chan_types[] = {
        { "Int",    ctx->type_i32 },
        { "String", ctx->type_i8ptr },
    };

    for (size_t ci = 0; ci < sizeof(chan_types) / sizeof(chan_types[0]); ci++) {
        const char *suf = chan_types[ci].suffix;
        LLVMTypeRef vt = chan_types[ci].val_type;
        char fname[128];

        { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_init_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_send_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_try_send_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_send_timeout_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_recv_val_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_try_recv_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0), ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_recv_timeout_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_ready_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_length_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_capacity_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_space_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_full_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_closed_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_close_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_channel_destroy_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }
}

#endif /* PGY_LLVM_ENABLED */
