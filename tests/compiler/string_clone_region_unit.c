/* Fault injection at the actual C/LLVM region-plan consumers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/c/transpiler_clone_value_emit.h"
#include "codegen/c/transpiler_context.h"
#include "compiler/verified_region_plan.h"
#include "parser/ast.h"
#ifdef PGY_LLVM_ENABLED
#include "codegen/llvm/llvm_internal.h"
#include "codegen/llvm/llvm_internal_api.h"
#include "codegen/llvm/llvm_expr_clone_value.h"
#include "codegen/llvm/llvm_expr_scalar_core.h"
#include "lexer/lexer.h"
#endif

static unsigned checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { failures++; fprintf(stderr, "region consumer line %d: %s\n", __LINE__, #x); } } while (0)

static void c_region_receipts(void)
{
    for (unsigned fault = 0; fault < 5; ++fault) {
        TranspilerCtx *ctx = transpiler_ctx_create();
        CHECK(ctx != NULL);
        if (ctx == NULL) continue;
        ASTNode site = { .type = AST_CALL, .stable_id = 41 };
        PgyRegionFactRow row = { 41, PGY_REGION_DISPOSITION_REGION, 3, 17 };
        PgyRegionPlan plan = { PGY_REGION_PLAN_REVISION, &row, 1, true };
        ctx->region_plan = &plan;
        ctx->region_scope_active = true;
        ctx->region_scope_id = 3;
        if (fault == 1) ctx->region_scope_active = false;
        if (fault == 2) ctx->region_scope_id = 4;
        if (fault == 3) ctx->region_plan = NULL;
        if (fault == 4) plan.verified = false;
        char *text = transpiler_emit_clone_value(ctx, &site, "String", "source");
        if (fault == 1 || fault == 2) {
            CHECK(text == NULL && ctx->backend_error != NULL);
        } else {
            CHECK(text != NULL && ctx->backend_error == NULL);
            if (text != NULL)
                CHECK(strcmp(text, fault == 0
                    ? "pgy_region_string_concat(&__pgy_region_3, source, \"\")"
                    : "StringConcat(source, \"\")") == 0);
        }
        free(text);
        ctx->region_plan = NULL;
        transpiler_ctx_destroy(ctx);
    }
}

#ifdef PGY_LLVM_ENABLED
static void llvm_region_receipts(bool concatenation)
{
    for (unsigned fault = 0; fault < 11; ++fault) {
        LLVMGenCtx *ctx = llvm_ctx_create("String-region-receipt");
        CHECK(ctx != NULL);
        if (ctx == NULL) continue;
        llvm_declare_runtime(ctx);
        LLVMTypeRef fn_type = LLVMFunctionType(ctx->type_void, NULL, 0, false);
        LLVMValueRef fn = LLVMAddFunction(ctx->module, "probe", fn_type);
        ctx->current_function = fn;
        LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
        LLVMPositionBuilderAtEnd(ctx->builder, block);
        ASTNode site = { .type = AST_CALL, .stable_id = 41 };
        PgyRegionFactRow row = { 41, PGY_REGION_DISPOSITION_REGION, 3, 17 };
        PgyRegionPlan plan = { PGY_REGION_PLAN_REVISION, &row, 1, true };
        ctx->region_plan = &plan;
        ctx->region_scope_active = true;
        ctx->region_scope_id = 3;
        ctx->region_owner_function = fn;
        ctx->region_alloca = LLVMBuildAlloca(ctx->builder, ctx->type_region, "region");
        if (fault == 1) ctx->region_scope_active = false;
        if (fault == 2) ctx->region_scope_id = 4;
        if (fault == 3) ctx->region_alloca = NULL;
        if (fault == 4) ctx->region_owner_function = NULL;
        if (fault == 5) ctx->region_plan = NULL;
        if (fault == 6) plan.verified = false;
        LLVMFuncEntry *region = llvm_lookup_function(ctx, "pgy_region_string_concat_export");
        CHECK(region != NULL);
        const char *region_name = region != NULL ? region->name : NULL;
        if (fault == 7 && region != NULL) region->name = NULL;
        if (fault == 8) ctx->current_function = LLVMAddFunction(ctx->module, "other", fn_type);
        if (fault == 9) row.disposition = PGY_REGION_DISPOSITION_HEAP;
        if (fault == 10) site.stable_id = 42;
        LLVMValueRef value = LLVMBuildGlobalStringPtr(ctx->builder, "copy", "text");
        ASTNode left = { .type = AST_STRING }, right = { .type = AST_STRING };
        left.data.string.value = "left";
        right.data.string.value = "right";
        if (concatenation) {
            site.type = AST_BINARY;
            site.data.binary.left = &left;
            site.data.binary.right = &right;
            site.data.binary.op.type = TOKEN_PLUS;
        }
        LLVMValueRef copy = concatenation ? llvm_emit_binary(&site, ctx)
            : llvm_emit_clone_value(&site, ctx, "String", value);
        if ((fault >= 1 && fault <= 4) || fault == 7 || fault == 8) {
            CHECK(copy == NULL && ctx->has_error);
            CHECK(LLVMGetInstructionOpcode(LLVMGetLastInstruction(block)) != LLVMCall);
        } else {
            LLVMFuncEntry *expected = llvm_lookup_function(ctx, fault == 0
                ? "pgy_region_string_concat_export" : "StringConcat");
            CHECK(copy != NULL && !ctx->has_error && expected != NULL);
            if (copy != NULL && expected != NULL) {
                CHECK(LLVMGetInstructionOpcode(copy) == LLVMCall);
                CHECK(LLVMGetCalledValue(copy) == expected->fn);
            }
        }
        if (region != NULL) region->name = region_name;
        ctx->region_plan = NULL;
        llvm_ctx_destroy(ctx);
    }
}

static void llvm_nested_copy_does_not_inherit_grant(void)
{
    LLVMGenCtx *ctx = llvm_ctx_create("nested-String-region-receipt");
    CHECK(ctx != NULL);
    if (ctx == NULL) return;
    llvm_declare_runtime(ctx);
    LLVMTypeRef fn_type = LLVMFunctionType(ctx->type_void, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, "probe", fn_type);
    ctx->current_function = fn;
    LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, block);
    ASTNode site = { .type = AST_CALL, .stable_id = 41 };
    PgyRegionFactRow row = { 41, PGY_REGION_DISPOSITION_REGION, 3, 17 };
    PgyRegionPlan plan = { PGY_REGION_PLAN_REVISION, &row, 1, true };
    ctx->region_plan = &plan;
    /* A root receipt is not a grant for the Option's String payload, even
     * when the root's region is deliberately inactive. */
    LLVMTypeRef fields[] = { ctx->type_i32, ctx->type_i8ptr };
    LLVMTypeRef option = LLVMStructTypeInContext(ctx->context, fields, 2, false);
    LLVMValueRef value = LLVMGetUndef(option);
    value = LLVMBuildInsertValue(ctx->builder, value,
        LLVMConstInt(ctx->type_i32, 0, false), 0, "tag");
    value = LLVMBuildInsertValue(ctx->builder, value,
        LLVMBuildGlobalStringPtr(ctx->builder, "nested", "text"), 1, "payload");
    CHECK(llvm_emit_clone_value(&site, ctx, "Option<String>", value) != NULL && !ctx->has_error);
    LLVMFuncEntry *heap = llvm_lookup_function(ctx, "StringConcat");
    LLVMFuncEntry *region = llvm_lookup_function(ctx, "pgy_region_string_concat_export");
    CHECK(heap != NULL && region != NULL);
    unsigned heap_calls = 0, region_calls = 0;
    for (LLVMBasicBlockRef b = LLVMGetFirstBasicBlock(fn); b != NULL; b = LLVMGetNextBasicBlock(b))
        for (LLVMValueRef inst = LLVMGetFirstInstruction(b); inst != NULL; inst = LLVMGetNextInstruction(inst)) {
            if (LLVMGetInstructionOpcode(inst) != LLVMCall) continue;
            if (heap != NULL && LLVMGetCalledValue(inst) == heap->fn) heap_calls++;
            if (region != NULL && LLVMGetCalledValue(inst) == region->fn) region_calls++;
        }
    CHECK(heap_calls == 1 && region_calls == 0);
    ctx->region_plan = NULL;
    llvm_ctx_destroy(ctx);
}
#endif

int main(void)
{
    c_region_receipts();
#ifdef PGY_LLVM_ENABLED
    llvm_region_receipts(false);
    llvm_region_receipts(true);
    llvm_nested_copy_does_not_inherit_grant();
#endif
    if (failures != 0) return 1;
    printf("String region consumers: %u checks; receipt faults refused, missing grants stay heap\n", checks);
    return 0;
}
