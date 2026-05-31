/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend type classification helpers split from llvm_backend.c.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "codegen_slot_type_policy.h"

bool
llvm_nominal_uses_immutable_projection_storage(NominalDeclKind kind)
{
    return kind == NOMINAL_DECL_OBJECT || kind == NOMINAL_DECL_TOBJECT;
}

bool
llvm_nominal_is_boundary_transfer_contract(NominalDeclKind kind)
{
    return kind == NOMINAL_DECL_TOBJECT;
}

PgyTypeKind
pgy_classify_type(const char *type_name)
{
    if (type_name == NULL)
        return PGY_TK_VOID;

    switch (type_name[0]) {
    case 'I': if (strcmp(type_name, "Int") == 0)        return PGY_TK_INT;        break;
    case 'L': if (strcmp(type_name, "Long") == 0)       return PGY_TK_LONG;       break;
    case 'F':
        if (strcmp(type_name, "Float") == 0)            return PGY_TK_FLOAT;
        if (strncmp(type_name, "Future<", 7) == 0)      return PGY_TK_FUTURE;
        break;
    case 'D':
        if (strcmp(type_name, "Double") == 0)           return PGY_TK_DOUBLE;
        if (strncmp(type_name, "DeviceSlot<", 11) == 0) return PGY_TK_DEVICE_SLOT;
        break;
    case 'B':
        if (strcmp(type_name, "Bool") == 0)             return PGY_TK_BOOL;
        if (strncmp(type_name, "Box<", 4) == 0)         return PGY_TK_BOX;
        break;
    case 'S':
        if (strcmp(type_name, "String") == 0)           return PGY_TK_STRING;
        if (strncmp(type_name, "Slot<", 5) == 0)        return PGY_TK_SLOT;
        if (strcmp(type_name, "Slot") == 0)             return PGY_TK_SLOT;
        if (strncmp(type_name, "SecureSlot<", 11) == 0) return PGY_TK_SECURE_SLOT;
        if (strncmp(type_name, "Slice<", 6) == 0)       return PGY_TK_SLICE;
        break;
    case 'V': if (strcmp(type_name, "Void") == 0)       return PGY_TK_VOID;       break;
    case 'Q': if (strcmp(type_name, "QubitSlot") == 0)  return PGY_TK_QUBIT_SLOT; break;
    case 'R':
        if (pgy_codegen_type_name_is_read_view(type_name)) return PGY_TK_SLOT;
        if (strncmp(type_name, "RemoteFuture<", 13) == 0) return PGY_TK_REMOTE_FUTURE;
        if (strncmp(type_name, "Result<", 7) == 0)      return PGY_TK_RESULT;
        if (strncmp(type_name, "Rc<", 3) == 0)          return PGY_TK_RC;
        break;
    case 'O':
        if (strncmp(type_name, "Option<", 7) == 0)      return PGY_TK_OPTION;
        break;
    case 'C':
        if (strncmp(type_name, "Channel<", 8) == 0)     return PGY_TK_CHANNEL;
        break;
    case 'W':
        if (pgy_codegen_type_name_is_write_view(type_name)) return PGY_TK_SLOT;
        if (strncmp(type_name, "Weak<", 5) == 0)        return PGY_TK_WEAK;
        break;
    case 'A':
        if (strncmp(type_name, "Array<", 6) == 0)       return PGY_TK_ARRAY;
        break;
    default:
        break;
    }
    return PGY_TK_UNKNOWN;
}

LLVMTypeRef
pgy_kind_to_llvm(LLVMGenCtx *ctx, PgyTypeKind kind)
{
    switch (kind) {
    case PGY_TK_INT:           return ctx->type_i32;
    case PGY_TK_LONG:          return ctx->type_i64;
    case PGY_TK_FLOAT:         return ctx->type_f32;
    case PGY_TK_DOUBLE:        return ctx->type_f64;
    case PGY_TK_BOOL:          return ctx->type_i1;
    case PGY_TK_STRING:        return ctx->type_i8ptr;
    case PGY_TK_QUBIT_SLOT:    return ctx->type_i32;
    case PGY_TK_REMOTE_FUTURE: return ctx->type_task_handle;
    case PGY_TK_VOID:          return ctx->type_void;
    default:                   return NULL;
    }
}

const char *
pgy_kind_to_suffix(PgyTypeKind kind)
{
    switch (kind) {
    case PGY_TK_INT:        return "Int";
    case PGY_TK_LONG:       return "Long";
    case PGY_TK_FLOAT:      return "Float";
    case PGY_TK_DOUBLE:     return "Double";
    case PGY_TK_BOOL:       return "Bool";
    case PGY_TK_STRING:     return "String";
    case PGY_TK_QUBIT_SLOT: return "QubitSlot";
    default:                return NULL;
    }
}

/* ---------------------------------------------------------------
 * Result<T, E> specialization helpers (C-backend parity).
 *
 * The C backend (transpiler_helpers_core_b.h) uses PGY_RESULT_DEFINE
 * macros to synthesize one struct typedef + helper functions per unique
 * (T, E) pair. LLVM IR has no preprocessor, so these helpers maintain a
 * per-module cache of named structs ({i32 tag, ok_ty value, err_ty err})
 * and build them on first reference.
 * ---------------------------------------------------------------- */

/* Copy `in` into `out`, replacing C-identifier-unsafe chars with '_'.
 * Collapses consecutive separators and strips trailing '_'. */
static void
pgy_sanitize_suffix(const char *in, char *out, size_t n)
{
    if (in == NULL || out == NULL || n == 0)
        return;

    size_t j = 0;
    bool last_under = false;
    for (size_t i = 0; in[i] != '\0' && j + 1 < n; i++) {
        char c = in[i];
        bool is_alnum = (c >= '0' && c <= '9')
                     || (c >= 'A' && c <= 'Z')
                     || (c >= 'a' && c <= 'z')
                     || c == '_';
        if (is_alnum) {
            out[j++] = c;
            last_under = (c == '_');
        } else if (!last_under) {
            out[j++] = '_';
            last_under = true;
        }
    }
    while (j > 0 && out[j - 1] == '_')
        j--;
    out[j] = '\0';
}

/* Split the inner args of `Result<T, E>` on the top-level comma.
 * Tracks `<>` depth so nested generics (`Result<Array<Int>, E>`) parse
 * correctly. Returns false if no top-level comma is found. */
static bool
pgy_result_type_ident_char(char c)
{
    return (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9')
        || c == '_';
}

static bool
pgy_result_type_arg_has_unknown(const char *arg)
{
    const char *p = arg;
    if (arg == NULL || arg[0] == '\0')
        return true;
    while (*p != '\0') {
        const char *start;
        size_t len;
        if (!pgy_result_type_ident_char(*p)) {
            p++;
            continue;
        }
        start = p;
        while (pgy_result_type_ident_char(*p))
            p++;
        len = (size_t)(p - start);
        if (len == 7 && strncmp(start, "Unknown", 7) == 0)
            return true;
    }
    return false;
}

static bool
pgy_result_copy_name(char *out, size_t out_n, const char *name)
{
    size_t len;

    if (out == NULL || out_n == 0 || name == NULL)
        return false;
    len = strlen(name);
    if (len >= out_n)
        return false;
    memcpy(out, name, len + 1);
    return true;
}

static bool
pgy_result_join_names(char *out, size_t out_n, const char *lhs,
                      const char *rhs)
{
    int written;

    if (out == NULL || out_n == 0 || lhs == NULL || rhs == NULL)
        return false;
    written = snprintf(out, out_n, "%s_%s", lhs, rhs);
    return written >= 0 && (size_t)written < out_n;
}

static bool
pgy_result_struct_name(char *out, size_t out_n, const char *suffix)
{
    int written;

    if (out == NULL || out_n == 0 || suffix == NULL)
        return false;
    written = snprintf(out, out_n, "PgyResult_%s", suffix);
    return written >= 0 && (size_t)written < out_n;
}

static bool
pgy_split_result_args(const char *inner,
                      char *ok_out, size_t ok_n,
                      char *err_out, size_t err_n)
{
    if (inner == NULL || ok_out == NULL || err_out == NULL)
        return false;
    if (ok_n == 0 || err_n == 0)
        return false;

    int depth = 0;
    const char *comma = NULL;
    for (const char *p = inner; *p != '\0'; p++) {
        if (*p == '<')
            depth++;
        else if (*p == '>') {
            if (depth > 0)
                depth--;
        } else if (*p == ',' && depth == 0) {
            comma = p;
            break;
        }
    }
    if (comma == NULL)
        return false;

    size_t ok_len = (size_t)(comma - inner);
    while (ok_len > 0 && (inner[ok_len - 1] == ' ' || inner[ok_len - 1] == '\t'))
        ok_len--;
    if (ok_len >= ok_n)
        ok_len = ok_n - 1;
    memcpy(ok_out, inner, ok_len);
    ok_out[ok_len] = '\0';

    const char *err_start = comma + 1;
    while (*err_start == ' ' || *err_start == '\t')
        err_start++;
    size_t err_len = strlen(err_start);
    while (err_len > 0 && (err_start[err_len - 1] == ' '
                           || err_start[err_len - 1] == '\t'))
        err_len--;
    if (err_len >= err_n)
        err_len = err_n - 1;
    memcpy(err_out, err_start, err_len);
    err_out[err_len] = '\0';

    return ok_len > 0 && err_len > 0;
}

/* Extract `Result<..>` inner section into a heap-free scratch. Returns
 * false if `name` is not a Result type or has the old single-arg spelling. */
static bool
pgy_result_inner_args(const char *name,
                      char *inner_out, size_t inner_n)
{
    if (name == NULL || inner_out == NULL || inner_n == 0)
        return false;
    if (strncmp(name, "Result<", 7) != 0)
        return false;

    const char *open  = name + 7;
    const char *close = strrchr(name, '>');
    if (close == NULL || close <= open)
        return false;

    size_t len = (size_t)(close - open);
    if (len >= inner_n)
        len = inner_n - 1;
    memcpy(inner_out, open, len);
    inner_out[len] = '\0';

    /* Must contain a top-level comma to be a 2-arg Result. */
    int depth = 0;
    for (size_t i = 0; i < len; i++) {
        if (inner_out[i] == '<')
            depth++;
        else if (inner_out[i] == '>') {
            if (depth > 0)
                depth--;
        } else if (inner_out[i] == ',' && depth == 0) {
            return true;
        }
    }
    return false;
}

bool
llvm_result_suffix_from_context(LLVMGenCtx *ctx,
                                char *suffix_out, size_t suffix_n,
                                char *ok_out, size_t ok_n,
                                char *err_out, size_t err_n)
{
    if (ctx == NULL || suffix_out == NULL || ok_out == NULL || err_out == NULL)
        return false;
    if (suffix_n == 0 || ok_n == 0 || err_n == 0)
        return false;

    /* Candidate source-level type names, priority order:
     * 1. let-binding annotation (expected_type_name)
     * 2. enclosing function's declared return type */
    const char *candidates[2];
    candidates[0] = ctx->expected_type_name;
    candidates[1] = NULL;
    ASTNode *current_return_type = ast_func_return_type(ctx->current_func_decl);
    if (current_return_type != NULL && current_return_type->type == AST_TYPE) {
        candidates[1] = ast_type_name(current_return_type);
    }

    char inner[256];
    const char *picked = NULL;
    for (int i = 0; i < 2; i++) {
        if (candidates[i] != NULL
            && pgy_result_inner_args(candidates[i], inner, sizeof(inner))) {
            picked = candidates[i];
            break;
        }
    }
    if (picked == NULL)
        return false;

    char ok_raw[128], err_raw[128];
    if (!pgy_split_result_args(inner, ok_raw, sizeof(ok_raw),
                               err_raw, sizeof(err_raw)))
        return false;
    if (pgy_result_type_arg_has_unknown(ok_raw)
        || pgy_result_type_arg_has_unknown(err_raw)) {
        return false;
    }

    if (!pgy_result_copy_name(ok_out, ok_n, ok_raw)
        || !pgy_result_copy_name(err_out, err_n, err_raw))
        return false;

    char combined[260];
    if (!pgy_result_join_names(combined, sizeof(combined), ok_raw, err_raw))
        return false;
    pgy_sanitize_suffix(combined, suffix_out, suffix_n);
    return suffix_out[0] != '\0';
}

/* Best-effort: resolve a source-level type name to an LLVM type.
 * Handles primitives, user-defined classes/subjects (via llvm_lookup_class),
 * and enums (represented as i32 in the LLVM backend). Returns NULL if the
 * name cannot be resolved; callers should emit a diagnostic. */
LLVMTypeRef
llvm_resolve_source_type(LLVMGenCtx *ctx, const char *type_name)
{
    if (ctx == NULL || type_name == NULL || type_name[0] == '\0')
        return NULL;

    PgyTypeKind kind = pgy_classify_type(type_name);
    LLVMTypeRef prim = pgy_kind_to_llvm(ctx, kind);
    if (prim != NULL)
        return prim;

    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
    if (cls != NULL && cls->struct_type != NULL)
        return cls->struct_type;

    if (llvm_enum_type_exists(ctx, type_name))
        return ctx->type_i32;

    return NULL;
}

LLVMResultSpecEntry *
llvm_ensure_result_type(LLVMGenCtx *ctx,
                        const char *ok_name, const char *err_name)
{
    if (ctx == NULL || ok_name == NULL || err_name == NULL)
        return NULL;
    if (pgy_result_type_arg_has_unknown(ok_name)
        || pgy_result_type_arg_has_unknown(err_name)) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Result<%s, %s>: cannot materialize Unknown result layout",
            ok_name != NULL ? ok_name : "<unknown>",
            err_name != NULL ? err_name : "<unknown>");
        return NULL;
    }

    char suffix[128];
    char combined[260];
    if (!pgy_result_join_names(combined, sizeof(combined), ok_name, err_name)) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Result<%s, %s>: specialization name is too long",
            ok_name, err_name);
        return NULL;
    }
    pgy_sanitize_suffix(combined, suffix, sizeof(suffix));
    if (suffix[0] == '\0')
        return NULL;

    for (int i = 0; i < ctx->result_spec_count; i++) {
        if (strcmp(ctx->result_specs[i].suffix, suffix) == 0)
            return &ctx->result_specs[i];
    }

    if (ctx->result_spec_count >= MAX_LLVM_RESULT_SPECS) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_RESULT_SPEC_CAPACITY,
            PGY_FIX_REUSE_SHARED_ERROR_ENUM,
            "Result<T,E> specialization limit (%d) exceeded at %s",
            MAX_LLVM_RESULT_SPECS, suffix);
        return NULL;
    }

    LLVMTypeRef ok_ty  = llvm_resolve_source_type(ctx, ok_name);
    LLVMTypeRef err_ty = llvm_resolve_source_type(ctx, err_name);
    if (ok_ty == NULL || err_ty == NULL) {
        llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_TYPE_UNSUPPORTED, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "Result<%s, %s>: cannot resolve %s type",
            ok_name, err_name, ok_ty == NULL ? ok_name : err_name);
        return NULL;
    }

    char struct_name[160];
    if (!pgy_result_struct_name(struct_name, sizeof(struct_name), suffix)) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Result<%s, %s>: generated struct name is too long",
            ok_name, err_name);
        return NULL;
    }
    LLVMTypeRef struct_ty = LLVMStructCreateNamed(ctx->context, struct_name);
    LLVMTypeRef fields[3] = { ctx->type_i32, ok_ty, err_ty };
    LLVMStructSetBody(struct_ty, fields, 3, 0);

    LLVMResultSpecEntry *entry = &ctx->result_specs[ctx->result_spec_count++];
    if (!pgy_result_copy_name(entry->suffix, sizeof(entry->suffix), suffix)
        || !pgy_result_copy_name(entry->ok_name, sizeof(entry->ok_name), ok_name)
        || !pgy_result_copy_name(entry->err_name, sizeof(entry->err_name), err_name)) {
        ctx->result_spec_count--;
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Result<%s, %s>: cache key is too long",
            ok_name, err_name);
        return NULL;
    }
    entry->struct_ty = struct_ty;
    entry->ok_ty     = ok_ty;
    entry->err_ty    = err_ty;
    return entry;
}

#endif
