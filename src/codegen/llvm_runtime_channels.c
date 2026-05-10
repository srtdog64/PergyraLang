#ifdef PGY_LLVM_ENABLED
#include "llvm_runtime_internal.h"

static bool
llvm_runtime_channel_name(char *out,
    size_t out_size,
    const char *op,
    const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || op == NULL || suffix == NULL)
        return false;

    written = snprintf(out, out_size, "pgy_channel_%s_%s", op, suffix);
    return written >= 0 && (size_t)written < out_size;
}

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
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "init", suf)) {
              llvm_set_error(ctx, "channel init runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "send", suf)) {
              llvm_set_error(ctx, "channel send runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "try_send", suf)) {
              llvm_set_error(ctx, "channel try-send runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "send_timeout", suf)) {
              llvm_set_error(ctx, "channel send-timeout runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "recv_val", suf)) {
              llvm_set_error(ctx, "channel recv-val runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "try_recv", suf)) {
              llvm_set_error(ctx, "channel try-recv runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0), ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "recv_timeout", suf)) {
              llvm_set_error(ctx, "channel recv-timeout runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "ready", suf)) {
              llvm_set_error(ctx, "channel ready runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "length", suf)) {
              llvm_set_error(ctx, "channel length runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "capacity", suf)) {
              llvm_set_error(ctx, "channel capacity runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "space", suf)) {
              llvm_set_error(ctx, "channel space runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "full", suf)) {
              llvm_set_error(ctx, "channel full runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "closed", suf)) {
              llvm_set_error(ctx, "channel closed runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "close", suf)) {
              llvm_set_error(ctx, "channel close runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_channel_name(fname, sizeof(fname), "destroy", suf)) {
              llvm_set_error(ctx, "channel destroy runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }
}

#endif /* PGY_LLVM_ENABLED */
