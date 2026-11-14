/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM rendered tuple type parsing and materialization.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"

#include <string.h>

#include <llvm-c/Core.h>

LLVMTypeRef
llvm_tuple_type_from_rendered_name(LLVMGenCtx *ctx, const char *type_name)
{
    LLVMTypeRef fields[32];
    size_t count = 0;
    size_t i = 1;
    size_t n;

    if (ctx == NULL || type_name == NULL || type_name[0] != '(')
        return NULL;

    n = strlen(type_name);
    while (i < n && type_name[i] != ')') {
        char elem[256];
        size_t elem_len = 0;
        int depth = 0;

        while (i < n && (type_name[i] == ' ' || type_name[i] == '\t'))
            i++;
        while (i < n) {
            char c = type_name[i];
            if (depth == 0 && (c == ',' || c == ')'))
                break;
            if (c == '<' || c == '(')
                depth++;
            else if ((c == '>' || c == ')') && depth > 0)
                depth--;
            if (elem_len + 1 >= sizeof(elem)) {
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_SPEC_LIMIT,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
                    "LLVM tuple type element is too long in '%s'",
                    type_name);
                return NULL;
            }
            elem[elem_len++] = c;
            i++;
        }
        while (elem_len > 0
               && (elem[elem_len - 1] == ' '
                   || elem[elem_len - 1] == '\t')) {
            elem_len--;
        }
        elem[elem_len] = '\0';
        if (elem_len == 0) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM tuple type '%s' contains an empty element",
                type_name);
            return NULL;
        }
        if (count >= sizeof(fields) / sizeof(fields[0])) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_SPEC_LIMIT,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
                "LLVM tuple type '%s' exceeds the 32-field beta limit",
                type_name);
            return NULL;
        }
        fields[count] = pergyra_type_to_llvm(ctx, elem);
        if (ctx->has_error || fields[count] == NULL)
            return NULL;
        count++;
        while (i < n && (type_name[i] == ' ' || type_name[i] == '\t'))
            i++;
        if (i < n && type_name[i] == ',') {
            i++;
            continue;
        }
        if (i < n && type_name[i] == ')')
            break;
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM tuple type '%s' is malformed",
            type_name);
        return NULL;
    }

    if (i >= n || type_name[i] != ')' || count < 2) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM tuple type '%s' requires a closed tuple with at least two elements",
            type_name);
        return NULL;
    }
    return LLVMStructTypeInContext(ctx->context, fields, (unsigned)count, 0);
}

#endif /* PGY_LLVM_ENABLED */
