#ifdef PGY_LLVM_ENABLED
#include "codegen_channel_runtime_abi.h"
#include "llvm_runtime_internal.h"

static LLVMTypeRef
llvm_runtime_channel_payload_type(LLVMGenCtx *ctx,
                                  PgyChannelRuntimePayloadKind kind)
{
    switch (kind) {
    case PGY_CHANNEL_RUNTIME_PAYLOAD_INT:
        return ctx->type_i32;
    case PGY_CHANNEL_RUNTIME_PAYLOAD_STRING:
        return ctx->type_i8ptr;
    case PGY_CHANNEL_RUNTIME_PAYLOAD_INVALID:
    default:
        return NULL;
    }
}

static LLVMTypeRef
llvm_runtime_channel_result_type(LLVMGenCtx *ctx,
                                 PgyChannelRuntimePayloadKind kind)
{
    LLVMTypeRef payload = llvm_runtime_channel_payload_type(ctx, kind);
    LLVMTypeRef i8 = LLVMInt8TypeInContext(ctx->context);
    LLVMTypeRef tag_pad = LLVMArrayType(i8, 4);
    LLVMTypeRef union_pad;
    LLVMTypeRef fields[4];

    if (payload == NULL)
        return NULL;

    switch (kind) {
    case PGY_CHANNEL_RUNTIME_PAYLOAD_INT:
        union_pad = LLVMArrayType(i8, 36);
        break;
    case PGY_CHANNEL_RUNTIME_PAYLOAD_STRING:
        union_pad = LLVMArrayType(i8, 32);
        break;
    case PGY_CHANNEL_RUNTIME_PAYLOAD_INVALID:
    default:
        return NULL;
    }

    fields[0] = ctx->type_i32;
    fields[1] = tag_pad;
    fields[2] = payload;
    fields[3] = union_pad;
    return LLVMStructTypeInContext(ctx->context, fields, 4, 0);
}

void
llvm_declare_runtime_channels(LLVMGenCtx *ctx)
{
    size_t count = pgy_channel_runtime_payload_abi_count();

    for (size_t ci = 0; ci < count; ci++) {
        const PgyChannelRuntimePayloadAbi *abi =
            pgy_channel_runtime_payload_abi_at(ci);
        const char *suf = abi != NULL ? abi->suffix : NULL;
        LLVMTypeRef vt = abi != NULL
            ? llvm_runtime_channel_payload_type(ctx, abi->kind)
            : NULL;
        char fname[128];

        if (suf == NULL || vt == NULL) {
            llvm_set_error(ctx, "channel runtime payload ABI is invalid");
            return;
        }

        { LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "init", suf)) {
              llvm_set_error(ctx, "channel init runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "send", suf)) {
              llvm_set_error(ctx, "channel send runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "send", suf)) {
              llvm_set_error(ctx, "lane channel send runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "try_send", suf)) {
              llvm_set_error(ctx, "channel try-send runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "try_send", suf)) {
              llvm_set_error(ctx, "lane channel try-send runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 2, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "try_send_status_code", suf)) {
              llvm_set_error(ctx, "channel try-send-status-code runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr, vt };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 3, 0);
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "try_send_status_code", suf)) {
              llvm_set_error(ctx, "lane channel try-send-status-code runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "send_timeout", suf)) {
              llvm_set_error(ctx, "channel send-timeout runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr, vt, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 4, 0);
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "send_timeout", suf)) {
              llvm_set_error(ctx, "lane channel send-timeout runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, vt, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 3, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "send_timeout_status_code", suf)) {
              llvm_set_error(ctx, "channel send-timeout-status-code runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr, vt, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 4, 0);
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "send_timeout_status_code", suf)) {
              llvm_set_error(ctx, "lane channel send-timeout-status-code runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "recv_val", suf)) {
              llvm_set_error(ctx, "channel recv-val runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 2, 0);
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "recv_val", suf)) {
              llvm_set_error(ctx, "lane channel recv-val runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef result_ty = llvm_runtime_channel_result_type(ctx, abi->kind);
          LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = result_ty != NULL
              ? LLVMFunctionType(result_ty, params, 1, 0)
              : NULL;
          if (ft == NULL) {
              llvm_set_error(ctx, "channel result runtime ABI is invalid");
              return;
          }
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "recv_result", suf)) {
              llvm_set_error(ctx, "channel recv-result runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, result_ty); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "try_recv", suf)) {
              llvm_set_error(ctx, "channel try-recv runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr, LLVMPointerType(vt, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "try_recv", suf)) {
              llvm_set_error(ctx, "lane channel try-recv runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef result_ty = llvm_runtime_channel_result_type(ctx, abi->kind);
          LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = result_ty != NULL
              ? LLVMFunctionType(result_ty, params, 1, 0)
              : NULL;
          if (ft == NULL) {
              llvm_set_error(ctx, "channel result runtime ABI is invalid");
              return;
          }
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "try_recv_result", suf)) {
              llvm_set_error(ctx, "channel try-recv-result runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, result_ty); }
        { LLVMTypeRef result_ty = llvm_runtime_channel_result_type(ctx, abi->kind);
          LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr };
          LLVMTypeRef ft = result_ty != NULL
              ? LLVMFunctionType(result_ty, params, 2, 0)
              : NULL;
          if (ft == NULL) {
              llvm_set_error(ctx, "lane channel result runtime ABI is invalid");
              return;
          }
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "try_recv_result", suf)) {
              llvm_set_error(ctx, "lane channel try-recv-result runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, result_ty); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr, LLVMPointerType(vt, 0), ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 3, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "recv_timeout", suf)) {
              llvm_set_error(ctx, "channel recv-timeout runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef result_ty = llvm_runtime_channel_result_type(ctx, abi->kind);
          LLVMTypeRef params[] = { ctx->type_i8ptr, ctx->type_i64 };
          LLVMTypeRef ft = result_ty != NULL
              ? LLVMFunctionType(result_ty, params, 2, 0)
              : NULL;
          if (ft == NULL) {
              llvm_set_error(ctx, "channel result runtime ABI is invalid");
              return;
          }
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "recv_timeout_result", suf)) {
              llvm_set_error(ctx, "channel recv-timeout-result runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, result_ty); }
        { LLVMTypeRef result_ty = llvm_runtime_channel_result_type(ctx, abi->kind);
          LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr, ctx->type_i64 };
          LLVMTypeRef ft = result_ty != NULL
              ? LLVMFunctionType(result_ty, params, 3, 0)
              : NULL;
          if (ft == NULL) {
              llvm_set_error(ctx, "lane channel result runtime ABI is invalid");
              return;
          }
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "recv_timeout_result", suf)) {
              llvm_set_error(ctx, "lane channel recv-timeout-result runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, result_ty); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "ready", suf)) {
              llvm_set_error(ctx, "channel ready runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i32, ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 2, 0);
          if (!pgy_lane_channel_runtime_name(fname, sizeof(fname), "ready", suf)) {
              llvm_set_error(ctx, "lane channel ready runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "length", suf)) {
              llvm_set_error(ctx, "channel length runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "capacity", suf)) {
              llvm_set_error(ctx, "channel capacity runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i32, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "space", suf)) {
              llvm_set_error(ctx, "channel space runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i32); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "full", suf)) {
              llvm_set_error(ctx, "channel full runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i1, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "closed", suf)) {
              llvm_set_error(ctx, "channel closed runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i1); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "close", suf)) {
              llvm_set_error(ctx, "channel close runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!pgy_channel_runtime_name(fname, sizeof(fname), "destroy", suf)) {
              llvm_set_error(ctx, "channel destroy runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }
}

#endif /* PGY_LLVM_ENABLED */
