/*
 * Copyright (c) 2026 Pergyra Language Project
 * Generic-class specialization naming policy for the C backend.
 */

#include "transpiler_generic_class_naming.h"

#include <stdio.h>
#include <string.h>

#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_mangled_name.h"

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"

bool
transpiler_generic_class_copy_name(char *out, size_t out_size,
                                   const char *name)
{
    size_t len;

    if (out == NULL || out_size == 0 || name == NULL)
        return false;
    len = strlen(name);
    if (len >= out_size)
        return false;
    memcpy(out, name, len + 1);
    return true;
}

bool
transpiler_generic_class_method_name(char *out, size_t out_size,
                                     const char *class_name,
                                     const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0 || class_name == NULL
        || method_name == NULL) {
        return false;
    }
    written = snprintf(out, out_size, "%s_%s", class_name, method_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
transpiler_generic_class_surface_desc(char *out, size_t out_size,
                                      const char *surface_kind,
                                      const char *class_name,
                                      const char *method_name,
                                      const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0 || surface_kind == NULL)
        return false;

    if (param_name != NULL) {
        written = snprintf(out, out_size, "%s '%s.%s(%s)'",
            surface_kind,
            class_name != NULL ? class_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)",
            param_name);
    } else {
        written = snprintf(out, out_size, "%s '%s.%s'",
            surface_kind,
            class_name != NULL ? class_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)");
    }

    return written >= 0 && (size_t)written < out_size;
}

void
transpiler_generic_class_format_too_long(TranspilerCtx *ctx,
                                         const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s is too long for C generic class specialization emission",
        surface_kind != NULL ? surface_kind : "generic class generated name");
}

char *
transpiler_generic_class_specialization_name(TranspilerCtx *ctx,
                                             ASTNode *class_decl,
                                             ASTNode *ann,
                                             bool *has_effective_args)
{
    GenericParams *gp;
    GenericParams *ga;
    const char *base_class_name;
    CodeBuf *nbuf;
    char *result;
    size_t formal_count;

    if (has_effective_args != NULL)
        *has_effective_args = false;
    if (class_decl == NULL || ann == NULL)
        return NULL;

    gp = ast_class_generic_params(class_decl);
    ga = ast_type_generic_args(ann);
    base_class_name = transpiler_decl_name_local(class_decl);
    if (gp == NULL || base_class_name == NULL)
        return NULL;

    nbuf = codebuf_create();
    if (nbuf == NULL)
        return NULL;
    codebuf_write(nbuf, "%s", base_class_name);
    formal_count = ast_generic_param_count(gp);
    for (size_t i = 0; i < formal_count; i++) {
        GenericParam *formal = ast_generic_param_at(gp, i);
        GenericParam *garg = ast_generic_param_at(ga, i);
        char *effective_name =
            transpiler_generic_param_effective_arg_name_in_ctx(
                ctx, formal, garg);
        if (effective_name == NULL) {
            if (has_effective_args != NULL)
                *has_effective_args = false;
            codebuf_destroy(nbuf);
            return NULL;
        }

        if (has_effective_args != NULL)
            *has_effective_args = true;
        codebuf_write(nbuf, "_");
        append_mangled_type_name(nbuf, effective_name);
        free(effective_name);
    }

    if (has_effective_args != NULL && !*has_effective_args) {
        codebuf_destroy(nbuf);
        return NULL;
    }

    result = pergyra_strdup(nbuf->data);
    codebuf_destroy(nbuf);
    return result;
}
