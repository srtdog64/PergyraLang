/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend match destructor recognition and payload binding emission.
 */

#include "transpiler_match_bindings.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

bool
transpiler_match_is_result_destructor(ASTNode *pat,
                                      const char **kind,
                                      const char **binding)
{
    ASTNode *callee;
    ASTNode *payload;

    if (pat == NULL || pat->type != AST_CALL)
        return false;
    callee = ast_call_callee(pat);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    const char *name = ast_identifier_name(callee);
    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(name);
    if (!pgy_codegen_match_variant_is_result(variant))
        return false;
    *kind = pgy_codegen_match_variant_name(variant);
    payload = ast_call_argument(pat, 0);
    if (ast_call_arg_count(pat) > 0
        && payload != NULL
        && payload->type == AST_IDENTIFIER) {
        *binding = ast_identifier_name(payload);
    } else {
        *binding = NULL;
    }
    return true;
}

bool
transpiler_match_is_option_destructor(ASTNode *pat,
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
        PgyCodegenMatchVariantKind variant =
            pgy_codegen_match_variant_lookup(name);
        if (variant == PGY_MATCH_VARIANT_NONE_CTOR) {
            *kind = pgy_codegen_match_variant_name(variant);
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
    PgyCodegenMatchVariantKind variant =
        pgy_codegen_match_variant_lookup(name);
    if (name == NULL)
        return false;

    if (variant == PGY_MATCH_VARIANT_NONE_CTOR && arg_count == 0) {
        *kind = pgy_codegen_match_variant_name(variant);
        return true;
    }
    if (variant == PGY_MATCH_VARIANT_SOME && arg_count == 1) {
        *kind = pgy_codegen_match_variant_name(variant);
        payload = ast_call_argument(pat, 0);
        if (payload != NULL && payload->type == AST_IDENTIFIER)
            *binding = ast_identifier_name(payload);
        return true;
    }

    return false;
}

bool
transpiler_match_is_enum_variant_destructor(
    ASTNode *pat,
    TranspilerCtx *ctx,
    const char **variant_name_out,
    const char **enum_name_out,
    const char ***bindings_out,
    ASTNode ***binding_types_out,
    size_t *binding_count_out,
    const char **bindings_buf,
    ASTNode **binding_types_buf,
    size_t binding_cap)
{
    const char *name = NULL;
    size_t argc = 0;
    ASTNode *callee;

    if (pat == NULL)
        return false;

    callee = ast_call_callee(pat);
    if (pat->type == AST_CALL
        && callee != NULL
        && callee->type == AST_IDENTIFIER) {
        name = ast_identifier_name(callee);
        argc = ast_call_arg_count(pat);
    } else if (pat->type == AST_IDENTIFIER) {
        name = ast_identifier_name(pat);
        argc = 0;
    } else {
        return false;
    }

    if (name == NULL)
        return false;
    if (bindings_buf == NULL || binding_types_buf == NULL || binding_cap == 0)
        return false;

    size_t type_count = 0;
    ASTNode **types = NULL;
    transpiler_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL)
        return false;

    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
        size_t variant_count = 0;
        char **variants;
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        bool has_data = false;
        variants = ast_enum_variants(stmt, &variant_count);
        for (size_t j = 0; j < variant_count; j++) {
            if (ast_enum_variant_param_count(stmt, j) > 0) {
                has_data = true;
                break;
            }
        }
        if (!has_data)
            continue;

        for (size_t j = 0; j < variant_count; j++) {
            const char *variant = variants != NULL ? variants[j] : NULL;
            if (variant != NULL && strcmp(variant, name) == 0) {
                const char *enum_name = transpiler_decl_name_local(stmt);
                if (enum_name == NULL)
                    return false;
                *variant_name_out = name;
                *enum_name_out = enum_name;
                *binding_count_out = 0;
                for (size_t k = 0; k < argc && k < binding_cap; k++) {
                    ASTNode *arg = ast_call_argument(pat, k);
                    bindings_buf[k] = (arg != NULL
                        && arg->type == AST_IDENTIFIER)
                        ? ast_identifier_name(arg)
                        : NULL;
                    binding_types_buf[k] =
                        (k < ast_enum_variant_param_count(stmt, j))
                        ? ast_enum_variant_param(stmt, j, k)
                        : NULL;
                    (*binding_count_out)++;
                }
                *bindings_out = bindings_buf;
                if (binding_types_out != NULL)
                    *binding_types_out = binding_types_buf;
                return true;
            }
        }
    }
    return false;
}

void
transpiler_emit_builtin_match_binding(ASTNode *pattern_node,
                                      const char *kind,
                                      const char *binding,
                                      const char *subject_type,
                                      bool subject_is_option,
                                      int tmp_id,
                                      TranspilerCtx *ctx)
{
    PgyCodegenMatchVariantKind match_variant =
        pgy_codegen_match_variant_lookup(kind);
    write_indent(ctx);

    if (match_variant == PGY_MATCH_VARIANT_SOME) {
        char inner_buf[128];
        const char *inner = NULL;
        if (subject_is_option
            && slot_inner_type_name_copy(subject_type, inner_buf,
                sizeof(inner_buf))) {
            inner = inner_buf;
        }
        if (inner == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C match lowering cannot bind Some(%s) without Option<T> subject type",
                binding);
            return;
        }
        char inner_c_type_buf[256];
        const char *inner_c_type = NULL;
        if (transpiler_require_type_name_c_type_copy(ctx, inner,
                "Some match payload", inner_c_type_buf,
                sizeof(inner_c_type_buf))) {
            inner_c_type = inner_c_type_buf;
        }
        if (inner_c_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C match lowering cannot render Some(%s) payload type '%s'",
                binding, inner);
            return;
        }
        codebuf_write(ctx->out, "%s %s = __match_%d.value;\n",
            inner_c_type, binding, tmp_id);
        register_typed_var(ctx, binding, inner);
    } else if (match_variant == PGY_MATCH_VARIANT_OK) {
        char ok_type_buf[128];
        const char *ok_type = NULL;
        if (transpiler_type_name_is_result(subject_type)) {
            copy_constructed_arg_name_at(subject_type, 0,
                ok_type_buf, sizeof(ok_type_buf));
            ok_type = ok_type_buf[0] != '\0' ? ok_type_buf : NULL;
        }
        if (ok_type == NULL || strcmp(ok_type, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C match lowering cannot bind Ok(%s) without Result<T,E> subject type",
                binding);
            return;
        }
        char ok_c_type_buf[256];
        const char *ok_c_type = NULL;
        if (transpiler_require_type_name_c_type_copy(ctx, ok_type,
                "Ok match payload", ok_c_type_buf,
                sizeof(ok_c_type_buf))) {
            ok_c_type = ok_c_type_buf;
        }
        if (ok_c_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C match lowering cannot render Ok(%s) payload type '%s'",
                binding, ok_type);
            return;
        }
        codebuf_write(ctx->out, "%s %s = __match_%d.ok;\n",
            ok_c_type, binding, tmp_id);
        register_typed_var(ctx, binding, ok_type);
    } else if (match_variant == PGY_MATCH_VARIANT_ERR) {
        char err_type_buf[128];
        char result_inner_buf[128];
        const char *err_type = "PgyError";
        if (!transpiler_type_name_is_result(subject_type)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C match lowering cannot bind Err(%s) without Result<T,E> subject type",
                binding);
            return;
        }
        if (slot_inner_type_name_copy(subject_type,
                result_inner_buf, sizeof(result_inner_buf))
            && strchr(result_inner_buf, ',') != NULL) {
            copy_constructed_arg_name_at(subject_type, 1,
                err_type_buf, sizeof(err_type_buf));
            err_type = err_type_buf[0] != '\0' ? err_type_buf : NULL;
        }
        if (err_type == NULL || strcmp(err_type, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C match lowering cannot bind Err(%s) without concrete Result error type",
                binding);
            return;
        }
        char err_c_type_buf[256];
        const char *err_c_type = NULL;
        if (transpiler_require_type_name_c_type_copy(ctx, err_type,
                "Err match payload", err_c_type_buf,
                sizeof(err_c_type_buf))) {
            err_c_type = err_c_type_buf;
        }
        if (err_c_type == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C match lowering cannot render Err(%s) payload type '%s'",
                binding, err_type);
            return;
        }
        codebuf_write(ctx->out, "%s %s = __match_%d.err;\n",
            err_c_type, binding, tmp_id);
        register_typed_var(ctx, binding, err_type);
    }

    (void)pattern_node;
}

void
transpiler_emit_enum_match_bindings(ASTNode *pattern_node,
                                    const char *kind,
                                    int tmp_id,
                                    TranspilerCtx *ctx)
{
    if (kind == NULL
        || pgy_codegen_match_variant_is_builtin(
            pgy_codegen_match_variant_lookup(kind))) {
        return;
    }

    const char *vn2 = NULL, *en2 = NULL;
    const char **bs2 = NULL;
    ASTNode **bt2 = NULL;
    size_t bc2 = 0;
    const char *bindings_buf[8];
    ASTNode *binding_types_buf[8];
    if (!transpiler_match_is_enum_variant_destructor(pattern_node, ctx,
            &vn2, &en2, &bs2, &bt2, &bc2,
            bindings_buf, binding_types_buf,
            sizeof(bindings_buf) / sizeof(bindings_buf[0]))) {
        return;
    }

    for (size_t b = 0; b < bc2; b++) {
        if (bs2[b] == NULL)
            continue;
        char bt_buf[256];
        const char *bt_c_type = "int32_t";
        if (bt2 != NULL && bt2[b] != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx, bt2[b],
                bt_buf, sizeof(bt_buf))) {
            bt_c_type = bt_buf;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = __match_%d.%s._%zu;\n",
            bt_c_type, bs2[b], tmp_id, vn2, b);
    }
}
