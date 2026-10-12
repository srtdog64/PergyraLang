/* Validation-only nested Clone runtime transport; no module is emitted or executed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic/type_system.h"
#include "codegen/llvm_internal.h"
#include "codegen/llvm_internal_api.h"
#include "codegen/llvm_expr_array_calls.h"

static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; fprintf(stderr, "LLVM receiver fault line %d: %s\n", __LINE__, #x); } } while (0)

static unsigned clone_call_count(LLVMBasicBlockRef block)
{
    unsigned count = 0;
    for (LLVMValueRef instruction = LLVMGetFirstInstruction(block); instruction != NULL;
         instruction = LLVMGetNextInstruction(instruction)) {
        if (LLVMGetInstructionOpcode(instruction) == LLVMCall) count++;
    }
    return count;
}

static void nested_clone_transport_faults(void)
{
    const char *names[] = {"pgy_array_clone_Array_Int_into", "pgy_array_clone_Array_String_into"};
    for (unsigned family = 0; family < 2; family++) {
        for (unsigned fault = 0; fault < 13; fault++) {
            LLVMGenCtx *ctx = llvm_ctx_create("nested-clone-transport-admission");
            CHECK(ctx != NULL);
            if (ctx == NULL) continue;
            llvm_declare_runtime(ctx);
            LLVMTypeRef array = family == 0 ? ctx->array_type_Array_Int : ctx->array_type_Array_String;
            LLVMTypeRef entry_type = LLVMFunctionType(ctx->type_void, NULL, 0, false);
            LLVMValueRef probe = LLVMAddFunction(ctx->module, "transport_probe", entry_type);
            ctx->current_function = probe;
            LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(ctx->context, probe, "entry");
            LLVMPositionBuilderAtEnd(ctx->builder, block);
            LLVMFuncEntry *entry = llvm_lookup_function(ctx, names[family]);
            CHECK(entry != NULL && array != NULL && !ctx->has_error);
            if (entry == NULL || array == NULL || ctx->has_error) { llvm_ctx_destroy(ctx); continue; }
            LLVMValueRef value = LLVMConstNull(array), out = NULL;
            CHECK(llvm_emit_array_clone_value(NULL, ctx, value, &out));
            CHECK(out != NULL && LLVMTypeOf(out) == array && !ctx->has_error);
            CHECK(clone_call_count(block) == 1);
            LLVMFuncEntry original = *entry;
            LLVMTypeRef pointer = LLVMPointerType(array, 0);
            LLVMTypeRef parameters[] = {pointer, pointer, pointer};
            if (fault == 0) entry->name = "removed_clone_registration";
            if (fault == 1) entry->fn = NULL;
            if (fault == 2) entry->fn_type = NULL;
            if (fault == 3) entry->ret_type = ctx->type_i32;
            if (fault == 4) entry->fn_type = LLVMFunctionType(ctx->type_i32, parameters, 2, false);
            if (fault == 5) entry->fn_type = LLVMFunctionType(ctx->type_void, parameters, 1, false);
            if (fault == 6) entry->fn_type = LLVMFunctionType(ctx->type_void, parameters, 3, false);
            if (fault == 7) {
                parameters[0] = ctx->type_i32;
                entry->fn_type = LLVMFunctionType(ctx->type_void, parameters, 2, false);
            }
            if (fault == 8) {
                parameters[1] = ctx->type_i32;
                entry->fn_type = LLVMFunctionType(ctx->type_void, parameters, 2, false);
            }
            if (fault == 9) entry->fn = LLVMAddFunction(ctx->module, "wrong_actual_return",
                LLVMFunctionType(ctx->type_i32, parameters, 2, false));
            if (fault == 10) entry->fn = LLVMAddFunction(ctx->module, "foreign_same_abi", original.fn_type);
            if (fault == 11) entry->runtime_entrypoint = false;
            if (fault == 12) entry->fn = LLVMConstNull(pointer);
            out = value; /* Failure must clear a non-NULL sentinel. */
            CHECK(llvm_emit_array_clone_value(NULL, ctx, value, &out));
            CHECK(ctx->has_error && out == NULL && ctx->error_code != NULL &&
                strcmp(ctx->error_code, "PGY_LLVM_TYPE_UNSUPPORTED") == 0);
            CHECK(clone_call_count(block) == 1);
            *entry = original;
            /* Test-only diagnostic reset after restoring the original registry.
               This is not a production recovery path or artifact permission. */
            ctx->has_error = false;
            memset(ctx->error_msg, 0, sizeof(ctx->error_msg));
            ctx->error_code = ctx->error_cause_ir = ctx->error_fix_source = NULL;
            ctx->error_line = ctx->error_column = 0;
            out = NULL;
            CHECK(llvm_emit_array_clone_value(NULL, ctx, value, &out));
            CHECK(!ctx->has_error && out != NULL && LLVMTypeOf(out) == array);
            CHECK(clone_call_count(block) == 2);
            /* Only helper validation/in-memory instructions are observed. No
               malformed or restored module is verified, emitted or executed. */
            llvm_ctx_destroy(ctx);
        }
    }
}

int main(void)
{
    type_system_init();
    nested_clone_transport_faults();
    type_system_cleanup();
    printf("LLVM NESTED CLONE TRANSPORT: %u checks, %u failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
