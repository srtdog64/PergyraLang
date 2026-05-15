#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "parser/ast_api.h"

static bool
llvm_match_is_option_destructor(ASTNode *pat,
                                const char **kind,
                                const char **binding)
{
    ASTNode *callee;
    ASTNode *payload;
    size_t arg_count;

    *kind = NULL;
    *binding = NULL;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = ast_identifier_name(pat);
        if (name != NULL && strcmp(name, "None") == 0) {
            *kind = "None";
            return true;
        }
        return false;
    }

    callee = ast_call_callee(pat);
    arg_count = ast_call_arg_count(pat);
    if (pat->type != AST_CALL
        || callee == NULL
        || callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = ast_identifier_name(callee);
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && arg_count == 0) {
        *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && arg_count == 1) {
        *kind = "Some";
        payload = ast_call_argument(pat, 0);
        if (payload != NULL && payload->type == AST_IDENTIFIER)
            *binding = ast_identifier_name(payload);
        return true;
    }

    return false;
}

static bool
llvm_match_is_result_destructor(ASTNode *pat,
                                const char **kind,
                                const char **binding)
{
    ASTNode *callee;
    ASTNode *payload;
    size_t arg_count;

    *kind = NULL;
    *binding = NULL;

    callee = ast_call_callee(pat);
    arg_count = ast_call_arg_count(pat);
    if (pat == NULL || pat->type != AST_CALL
        || callee == NULL
        || callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = ast_identifier_name(callee);
    if (name == NULL)
        return false;

    if ((strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0)
        && arg_count == 1) {
        *kind = name;
        payload = ast_call_argument(pat, 0);
        if (payload != NULL && payload->type == AST_IDENTIFIER)
            *binding = ast_identifier_name(payload);
        return true;
    }

    return false;
}

void
llvm_emit_match_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMValueRef subject = llvm_emit_expression(ast_match_subject(node), ctx);
    if (subject == NULL)
        return;

    LLVMValueRef fn = ctx->current_function;
    LLVMBasicBlockRef merge_bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "match.end");

    for (size_t i = 0; i < ast_match_case_count(node); i++) {
        ASTNode *mc = ast_match_case_at(node, i);
        if (mc == NULL || mc->type != AST_MATCH_CASE)
            continue;

        const char *option_kind = NULL;
        const char *option_binding = NULL;
        const char *result_kind = NULL;
        const char *result_binding = NULL;
        LLVMValueRef cmp = NULL;

        if (ast_match_case_patterns(mc, NULL) != NULL
            && ast_match_case_pattern_count(mc) > 1) {
            for (size_t p = 0; p < ast_match_case_pattern_count(mc); p++) {
                LLVMValueRef pattern = llvm_emit_expression(
                    ast_match_case_pattern_at(mc, p), ctx);
                LLVMValueRef alt_cmp;
                if (pattern == NULL)
                    continue;
                alt_cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                                        subject, pattern,
                                        llvm_tmp_name(ctx));
                cmp = (cmp == NULL)
                    ? alt_cmp
                    : LLVMBuildOr(ctx->builder, cmp, alt_cmp,
                                  llvm_tmp_name(ctx));
            }
            if (cmp == NULL)
                continue;
        } else if (llvm_match_is_option_destructor(
                       ast_match_case_pattern(mc),
                       &option_kind,
                       &option_binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    strcmp(option_kind, "Some") == 0 ? 0 : 1, 0),
                llvm_tmp_name(ctx));
        } else if (llvm_match_is_result_destructor(
                       ast_match_case_pattern(mc),
                       &result_kind,
                       &result_binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    strcmp(result_kind, "Ok") == 0 ? 0 : 1, 0),
                llvm_tmp_name(ctx));
        } else {
            LLVMValueRef pattern = llvm_emit_expression(
                ast_match_case_pattern(mc), ctx);
            if (pattern == NULL)
                continue;
            cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ,
                                subject, pattern,
                                llvm_tmp_name(ctx));
        }

        LLVMBasicBlockRef case_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.case");
        LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(
            ctx->context, fn, "match.next");

        LLVMBuildCondBr(ctx->builder, cmp, case_bb, next_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, case_bb);
        llvm_scope_push(ctx);
        if (option_binding != NULL) {
            LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder, subject,
                1, llvm_tmp_name(ctx));
            LLVMTypeRef payload_ty = LLVMTypeOf(payload);
            LLVMValueRef payload_alloca = llvm_create_entry_alloca(ctx,
                payload_ty, option_binding);
            LLVMBuildStore(ctx->builder, payload, payload_alloca);
            llvm_scope_declare(ctx, pergyra_strdup(option_binding),
                payload_alloca, payload_ty);
        }
        if (result_binding != NULL) {
            unsigned payload_index =
                (result_kind != NULL && strcmp(result_kind, "Err") == 0) ? 2 : 1;
            LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder, subject,
                payload_index, llvm_tmp_name(ctx));
            LLVMTypeRef payload_ty = LLVMTypeOf(payload);
            LLVMValueRef payload_alloca = llvm_create_entry_alloca(ctx,
                payload_ty, result_binding);
            LLVMBuildStore(ctx->builder, payload, payload_alloca);
            llvm_scope_declare(ctx, pergyra_strdup(result_binding),
                payload_alloca, payload_ty);
        }
        if (ast_match_case_body(mc) != NULL)
            llvm_emit_statement(ast_match_case_body(mc), ctx);
        llvm_scope_pop(ctx);
        if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
            LLVMBuildBr(ctx->builder, merge_bb);

        LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
    }

    if (ast_match_default_body(node) != NULL)
        llvm_emit_statement(ast_match_default_body(node), ctx);
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
        LLVMBuildBr(ctx->builder, merge_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, merge_bb);
}

#endif /* PGY_LLVM_ENABLED */
