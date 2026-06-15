/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend allocator builtin emission.
 */

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_symbols.h"

#include "../common/string_compat.h"
#include "../semantic/diag_codes.h"
#include "../semantic/builtin_kind.h"

#include <stdlib.h>
#include <string.h>

char *
emit_builtin_allocator(ASTNode *call, BuiltinKind kind, TranspilerCtx *ctx)
{
    switch (kind) {
    case BUILTIN_ALLOCATOR_DESTROY:
        if (ast_call_arg_count(call) != 1) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                "C backend: AllocatorDestroy requires exactly one Allocator argument");
            return NULL;
        }
        {
            ASTNode *arg = ast_call_argument(call, 0);
            const char *arg_name;
            const char *arg_type;
            const char *ssa_name;
            TypedVarEntry *arg_entry;
            char *c_arg_name;
            char *result;

            if (arg == NULL || arg->type != AST_IDENTIFIER
                || ast_identifier_name(arg) == NULL) {
                transpiler_set_backend_error_with_hints(
                    ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                    "C backend: AllocatorDestroy requires a named Allocator local");
                return NULL;
            }
            arg_name = ast_identifier_name(arg);
            arg_entry = lookup_typed_entry(ctx, arg_name);
            arg_type = arg_entry != NULL ? arg_entry->type_name : NULL;
            if (arg_type == NULL)
                arg_type = infer_expression_type_name(ctx, arg);
            if (arg_type == NULL || strcmp(arg_type, "Allocator") != 0) {
                transpiler_set_backend_error_with_hints(
                    ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                    "C backend: AllocatorDestroy argument '%s' must have type Allocator",
                    arg_name != NULL ? arg_name : "<allocator>");
                return NULL;
            }
            ssa_name = transpiler_resolve_active_ssa_name(ctx, arg_name);
            if (ssa_name == NULL && arg_entry != NULL
                && arg_entry->ssa_name[0] != '\0')
                ssa_name = arg_entry->ssa_name;
            c_arg_name = ssa_name != NULL
                ? transpiler_make_c_ssa_name(ctx, ssa_name)
                : pergyra_strdup(arg_name);
            if (c_arg_name == NULL) {
                transpiler_set_backend_error_with_hints(
                    ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: AllocatorDestroy could not render allocator local '%s'",
                    arg_name);
                return NULL;
            }
            result = strdup_fmt("pgy_allocator_destroy(&%s)", c_arg_name);
            free(c_arg_name);
            return result;
        }
    case BUILTIN_ALLOCATOR_SYSTEM:
        return pergyra_strdup("pgy_allocator_system()");
    case BUILTIN_ALLOCATOR_TRACING:
        return pergyra_strdup("pgy_allocator_tracing()");
    case BUILTIN_ALLOCATOR_DEBUG:
        return pergyra_strdup("pgy_allocator_debug()");
    case BUILTIN_ALLOCATOR_SCRATCH:
        return pergyra_strdup("pgy_allocator_scratch()");
    case BUILTIN_ALLOCATOR_RESULT:
        return pergyra_strdup("pgy_allocator_result()");
    case BUILTIN_ALLOCATOR_PERSISTENT:
        return pergyra_strdup("pgy_allocator_persistent()");
    case BUILTIN_ALLOCATOR_POOL:
        if (ast_call_arg_count(call) != 1) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: AllocatorPool requires exactly one capacity argument");
            return NULL;
        }
        {
            char *cap = emit_expression(ast_call_argument(call, 0), ctx);
            if (cap == NULL) {
                transpiler_set_backend_error_with_hints(
                    ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: AllocatorPool could not lower capacity expression");
                return NULL;
            }
            char *result = strdup_fmt("pgy_allocator_pool(%s)", cap);
            free(cap);
            return result;
        }
    default:
        transpiler_set_backend_error_with_hints(
            ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "C backend: unsupported allocator builtin kind %d", (int)kind);
        return NULL;
    }
}
