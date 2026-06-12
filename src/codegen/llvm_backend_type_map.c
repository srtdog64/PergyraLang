/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM backend type-name rendering and AST/Pergyra type mapping.
 */

#ifdef PGY_LLVM_ENABLED

#include "codegen_channel_runtime_abi.h"
#include "host_decl_compat.h"
#include "llvm_backend.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"
#include "../compiler/mir_decl_headers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <llvm-c/Core.h>

/* Forward declaration for type mapping (used by slot helpers) */
LLVMTypeRef pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name);

/* =================================================================
 * Pergyra type to LLVM type mapping.
 * ================================================================= */

/* Resolve inner type for generic containers: "Result<Int>" -> i32. */
static bool
llvm_required_constructed_arg_name_copy(LLVMGenCtx *ctx,
                                        const char *type_name,
                                        int arg_index,
                                        const char *container_name,
                                        char *out,
                                        size_t out_size)
{
    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';

    if (!llvm_constructed_arg_name_copy(type_name, arg_index, out, out_size)) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "%s type argument %d is too long while lowering '%s'",
                container_name != NULL ? container_name : "generic type",
                arg_index + 1,
                type_name != NULL ? type_name : "<null>");
        }
        return false;
    }
    if (out[0] != '\0')
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "%s requires explicit concrete type argument %d while lowering '%s'",
            container_name != NULL ? container_name : "generic type",
            arg_index + 1,
            type_name != NULL ? type_name : "<null>");
    }
    return false;
}

LLVMTypeRef
llvm_resolve_inner_type(LLVMGenCtx *ctx, const char *type_name)
{
    char inner_buf[256];
    LLVMTypeRef resolved;

    if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
            "Option<T>", inner_buf, sizeof(inner_buf))) {
        return NULL;
    }
    if (strcmp(inner_buf, "Void") == 0) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Option<Void> does not lower until the Option<Void> ABI is frozen");
        return NULL;
    }
    resolved = pergyra_type_to_llvm(ctx, inner_buf);
    if (resolved == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "Option<T>: cannot resolve concrete inner type metadata");
    }
    return resolved;
}

static ASTNode *
llvm_generic_default_from_header(const MIRDeclHeader *header,
                                 const char *type_name)
{
    if (header == NULL || type_name == NULL)
        return NULL;
    for (size_t i = 0; i < mir_decl_header_generic_param_count(header); i++) {
        const MIRDeclGenericParam *param =
            mir_decl_header_generic_param(header, i);
        const char *param_name = mir_decl_generic_param_name(param);
        if (param_name == NULL || strcmp(param_name, type_name) != 0)
            continue;
        if (mir_decl_generic_param_default_type(param) != NULL)
            return mir_decl_generic_param_default_type(param);
        if (mir_decl_generic_param_constraint(param) != NULL)
            return mir_decl_generic_param_constraint(param);
        return NULL;
    }
    return NULL;
}

static ASTNode *
llvm_generic_default_from_params(GenericParams *params, const char *type_name)
{
    if (params == NULL || type_name == NULL)
        return NULL;
    size_t param_count = ast_generic_param_count(params);
    for (size_t i = 0; i < param_count; i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        if (param == NULL || ast_generic_param_name(param) == NULL)
            continue;
        if (strcmp(ast_generic_param_name(param), type_name) == 0) {
            if (ast_generic_param_default_type(param) != NULL)
                return ast_generic_param_default_type(param);
            if (ast_generic_param_constraint(param) != NULL)
                return ast_generic_param_constraint(param);
            return NULL;
        }
    }
    return NULL;
}

static ASTNode *
llvm_generic_default_from_decl(ASTNode *decl, const char *type_name)
{
    if (decl == NULL || type_name == NULL)
        return NULL;

    return llvm_generic_default_from_params(
        ast_declaration_generic_params(decl), type_name);
}

static ASTNode *
llvm_generic_default_from_context_decl(LLVMGenCtx *ctx, ASTNode *decl,
                                       const char *type_name)
{
    const char *decl_name;
    const MIRDeclHeader *header;
    ASTNode *resolved;

    if (ctx == NULL || decl == NULL || type_name == NULL)
        return NULL;

    decl_name = llvm_decl_node_name(decl);
    header = llvm_find_decl_header_in_context_of_type(ctx, decl->type, decl_name);
    resolved = llvm_generic_default_from_header(header, type_name);
    if (resolved != NULL || header != NULL)
        return resolved;
    return llvm_generic_default_from_decl(decl, type_name);
}

static bool
llvm_generic_default_candidate_keep(LLVMGenCtx *ctx,
                                    ASTNode **candidate,
                                    ASTNode *resolved)
{
    char *candidate_name;
    char *resolved_name;

    if (candidate == NULL || resolved == NULL)
        return true;
    if (*candidate == NULL) {
        *candidate = resolved;
        return true;
    }

    candidate_name =
        llvm_render_type_name_scratch_in_ctx(ctx, *candidate, &ctx->scratch);
    resolved_name =
        llvm_render_type_name_scratch_in_ctx(ctx, resolved, &ctx->scratch);
    return candidate_name != NULL
        && resolved_name != NULL
        && strcmp(candidate_name, resolved_name) == 0;
}

static ASTNode *
llvm_find_generic_default_in_mir_inventory(LLVMGenCtx *ctx,
                                           const char *type_name)
{
    LLVMMIRDeclHeaderInventory headers;
    ASTNode *candidate = NULL;

    llvm_active_decl_header_inventory(ctx, &headers);
    for (size_t i = 0; i < headers.count; i++) {
        const MIRDeclHeader *header =
            llvm_decl_header_inventory_get(&headers, i);
        ASTNode *resolved =
            llvm_generic_default_from_header(header, type_name);
        if (resolved == NULL)
            continue;
        if (!llvm_generic_default_candidate_keep(ctx, &candidate, resolved))
            return NULL;
    }

    return candidate;
}

static ASTNode *
llvm_find_generic_default_in_compat_inventory(LLVMGenCtx *ctx,
                                              const char *type_name)
{
    ASTNode *candidate = NULL;
    ASTNode *resolved = NULL;
    const ASTNodeType direct_decl_types[] = {
        AST_FUNC_DECL,
        AST_ABILITY_DECL
    };
    const ASTNodeType *host_decl_types = NULL;
    size_t host_decl_type_count = 0;

    for (size_t kind = 0;
         kind < sizeof(direct_decl_types) / sizeof(direct_decl_types[0]);
         kind++) {
        ASTNode **nodes = NULL;
        size_t count = 0;
        llvm_active_inventory(ctx, direct_decl_types[kind], &nodes, &count);
        for (size_t i = 0; i < count; i++) {
            resolved = llvm_generic_default_from_context_decl(
                ctx, nodes != NULL ? nodes[i] : NULL, type_name);
            if (resolved == NULL)
                continue;
            if (!llvm_generic_default_candidate_keep(ctx, &candidate, resolved))
                return NULL;
        }
    }

    host_decl_types = pgy_host_decl_compat_types(&host_decl_type_count);
    for (size_t kind = 0; host_decl_types != NULL
         && kind < host_decl_type_count; kind++) {
        ASTNode **nodes = NULL;
        size_t count = 0;
        llvm_active_inventory(ctx, host_decl_types[kind], &nodes, &count);
        for (size_t i = 0; i < count; i++) {
            resolved = llvm_generic_default_from_context_decl(
                ctx, nodes != NULL ? nodes[i] : NULL, type_name);
            if (resolved == NULL)
                continue;
            if (!llvm_generic_default_candidate_keep(ctx, &candidate, resolved))
                return NULL;
        }
    }

    return candidate;
}

static ASTNode *
llvm_find_generic_default_in_inventory(LLVMGenCtx *ctx, const char *type_name)
{
    ASTNode *resolved = NULL;

    if (ctx == NULL || type_name == NULL)
        return NULL;

    if (llvm_active_has_mir(ctx)) {
        if (ctx->current_host_decl != NULL) {
            const char *decl_name =
                llvm_decl_node_name(ctx->current_host_decl);
            const MIRDeclHeader *header =
                llvm_find_decl_header_in_context_of_type(
                    ctx, ctx->current_host_decl->type, decl_name);
            resolved = llvm_generic_default_from_header(header, type_name);
            if (resolved != NULL)
                return resolved;
        }
        return llvm_find_generic_default_in_mir_inventory(ctx, type_name);
    }

    resolved = llvm_generic_default_from_context_decl(
        ctx, ctx->current_host_decl, type_name);
    if (resolved != NULL)
        return resolved;

    return llvm_find_generic_default_in_compat_inventory(ctx, type_name);
}

static LLVMTypeRef
llvm_resolve_alias_type(LLVMGenCtx *ctx, const char *type_name)
{
    ASTNode *alias_decl;

    if (ctx == NULL || type_name == NULL)
        return NULL;

    alias_decl = llvm_find_decl_in_active_inventory(ctx, AST_TYPE_ALIAS, type_name);
    if (alias_decl == NULL || ast_type_alias_target_type(alias_decl) == NULL)
        return NULL;

    return ast_type_to_llvm(ctx, ast_type_alias_target_type(alias_decl));
}

static LLVMTypeRef
llvm_resolve_generic_formal_default(LLVMGenCtx *ctx, const char *type_name)
{
    ASTNode *default_type;

    if (ctx == NULL || type_name == NULL)
        return NULL;

    default_type = llvm_find_generic_default_in_inventory(ctx, type_name);
    if (default_type == NULL)
        return NULL;

    return ast_type_to_llvm(ctx, default_type);
}

static LLVMTypeRef
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

/*
 * On-demand monomorphization of a user generic class type reference, e.g.
 * Pair<Int>. The C backend emits a specialized struct per instantiation; this
 * mirrors that for the LLVM type map. The base generic class decl is located in
 * the active inventory, each type parameter is bound to the corresponding
 * instantiation argument through the existing type-substitution stack, and the
 * fields are resolved under that substitution to build and register a concrete
 * named struct keyed by the raw instantiation name so later lookups hit it.
 */
static LLVMTypeRef
llvm_specialize_generic_class_type(LLVMGenCtx *ctx, const char *type_name)
{
    const char *lt = type_name != NULL ? strchr(type_name, '<') : NULL;
    size_t base_len;
    char base[128];
    ASTNode *tmpl;
    GenericParams *gp;
    size_t gpc;
    int saved_subst;
    LLVMHostedFieldView fv;
    size_t fc;
    LLVMTypeRef *ftypes;
    LLVMTypeRef struct_ty;
    NominalDeclKind nk;
    bool is_subject;
    bool is_ptr_self;

    if (lt == NULL)
        return NULL;
    {
        LLVMClassTypeEntry *cached = llvm_lookup_class(ctx, type_name);
        if (cached != NULL)
            return cached->struct_type;
    }
    base_len = (size_t)(lt - type_name);
    if (base_len == 0 || base_len >= sizeof(base))
        return NULL;
    memcpy(base, type_name, base_len);
    base[base_len] = '\0';

    tmpl = llvm_find_decl_in_active_inventory(ctx, AST_CLASS_DECL, base);
    if (tmpl == NULL)
        return NULL;
    gp = ast_class_generic_params(tmpl);
    gpc = ast_generic_param_count(gp);
    if (gpc == 0 || gpc > MAX_TYPE_SUBST)
        return NULL;

    saved_subst = ctx->type_subst_count;
    for (size_t gi = 0; gi < gpc; gi++) {
        GenericParam *p = ast_generic_param_at(gp, gi);
        const char *pname = ast_generic_param_name(p);
        char arg_buf[256];
        LLVMTypeRef arg_ty;
        if (pname == NULL
            || !llvm_required_constructed_arg_name_copy(ctx, type_name, gi,
                   base, arg_buf, sizeof(arg_buf))) {
            ctx->type_subst_count = saved_subst;
            return NULL;
        }
        /* Only monomorphize on concrete arguments. An unbound type parameter
         * (a single uppercase letter with no active substitution) must stay
         * type-erased to preserve the generic-ability / slot surface that
         * relies on i8ptr handles rather than inline specialized storage. */
        if (arg_buf[0] >= 'A' && arg_buf[0] <= 'Z' && arg_buf[1] == '\0') {
            bool bound = false;
            for (int si = 0; si < ctx->type_subst_count; si++) {
                if (ctx->type_subst[si].param_name != NULL
                    && strcmp(ctx->type_subst[si].param_name, arg_buf) == 0) {
                    bound = true;
                    break;
                }
            }
            if (!bound) {
                ctx->type_subst_count = saved_subst;
                return NULL;
            }
        }
        arg_ty = pergyra_type_to_llvm(ctx, arg_buf);
        if (arg_ty == NULL || ctx->type_subst_count >= MAX_TYPE_SUBST) {
            ctx->type_subst_count = saved_subst;
            return NULL;
        }
        ctx->type_subst[ctx->type_subst_count].param_name = pname;
        ctx->type_subst[ctx->type_subst_count].llvm_type = arg_ty;
        ctx->type_subst[ctx->type_subst_count].type_name =
            pergyra_strdup(arg_buf);
        ctx->type_subst_count++;
    }

    fv = llvm_hosted_class_field_view_from_decl(ctx, base, tmpl);
    if (llvm_hosted_field_view_missing_mir_metadata(&fv)) {
        ctx->type_subst_count = saved_subst;
        return NULL;
    }
    fc = fv.count;
    ftypes = pgy_arena_calloc(&ctx->scratch,
        (fc > 0 ? fc : 1) * sizeof(LLVMTypeRef));
    if (ftypes == NULL) {
        ctx->type_subst_count = saved_subst;
        return NULL;
    }
    for (size_t j = 0; j < fc; j++) {
        ASTNode *ft = llvm_hosted_field_view_type(&fv, j);
        ftypes[j] = ast_type_to_llvm(ctx, ft);
        if (ctx->has_error || ftypes[j] == NULL) {
            ctx->type_subst_count = saved_subst;
            return NULL;
        }
    }

    struct_ty = LLVMStructCreateNamed(ctx->context, type_name);
    LLVMStructSetBody(struct_ty, ftypes, (unsigned)fc, 0);
    nk = ast_class_nominal_kind(tmpl);
    is_subject = nk == NOMINAL_DECL_SUBJECT;
    is_ptr_self = is_subject || nk == NOMINAL_DECL_VESSEL;
    llvm_register_class(ctx, pergyra_strdup(type_name), struct_ty,
        is_subject, is_ptr_self);

    ctx->type_subst_count = saved_subst;
    return struct_ty;
}

LLVMTypeRef
pergyra_type_to_llvm(LLVMGenCtx *ctx, const char *type_name)
{
    if (type_name == NULL)
        return ctx->type_void;

    if (type_name[0] == '(')
        return llvm_tuple_type_from_rendered_name(ctx, type_name);

    if (strcmp(type_name, "PgyError") == 0)
        return ctx->type_i8ptr;

    if (strncmp(type_name, "List<", 5) == 0) {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "List<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_list_struct_type(ctx, inner_buf);
    }
    if (strncmp(type_name, "Set<", 4) == 0) {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Set<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_set_struct_type(ctx, inner_buf);
    }
    if (strncmp(type_name, "Queue<", 6) == 0) {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Queue<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_queue_struct_type(ctx, inner_buf);
    }
    if (strncmp(type_name, "HashMap<", 8) == 0) {
        char value_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 1,
                "HashMap<K, V>", value_buf, sizeof(value_buf))) {
            return NULL;
        }
        return llvm_hashmap_struct_type(ctx, value_buf);
    }

    /* Check active type substitution (monomorphization) first */
    for (int i = 0; i < ctx->type_subst_count; i++) {
        if (strcmp(type_name, ctx->type_subst[i].param_name) == 0)
            return ctx->type_subst[i].llvm_type;
    }

    PgyTypeKind kind = pgy_classify_type(type_name);

    /* Primitive types: direct mapping. */
    LLVMTypeRef primitive = pgy_kind_to_llvm(ctx, kind);
    if (primitive != NULL)
        return primitive;

    {
        LLVMTypeRef alias_type = llvm_resolve_alias_type(ctx, type_name);
        if (alias_type != NULL)
            return alias_type;
    }

    {
        LLVMTypeRef generic_default =
            llvm_resolve_generic_formal_default(ctx, type_name);
        if (generic_default != NULL)
            return generic_default;
    }

    /* Generic container types */
    switch (kind) {
    case PGY_TK_RESULT: {
        char ok_name_buf[256]  = {0};
        char err_name_buf[256] = {0};
        (void)llvm_constructed_arg_name_copy(type_name, 0,
                                             ok_name_buf,
                                             sizeof(ok_name_buf));
        (void)llvm_constructed_arg_name_copy(type_name, 1,
                                             err_name_buf,
                                             sizeof(err_name_buf));
        const char *ok_name  = ok_name_buf[0]  != '\0' ? ok_name_buf  : NULL;
        const char *err_name = err_name_buf[0] != '\0' ? err_name_buf : NULL;

        /* Legacy single-arg Result<T> defaults err to PgyError (i8ptr).
         * Two-arg Result<T, E> routes through the named-struct cache so
         * Ok/Err builders and match destructuring share one layout. */
        if (ok_name != NULL && err_name != NULL) {
            if (strcmp(ok_name, "Void") == 0) {
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "Result<Void, %s> does not lower until the Result<Void> ABI is frozen",
                    err_name);
                return NULL;
            }
            LLVMResultSpecEntry *spec =
                llvm_ensure_result_type(ctx, ok_name, err_name);
            if (spec != NULL && spec->struct_ty != NULL)
                return spec->struct_ty;
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "Result<%s, %s>: cannot materialize named layout",
                    ok_name, err_name);
            }
            return NULL;
        }
        if (ok_name == NULL) {
            if (ctx != NULL && !ctx->has_error) {
                llvm_set_error_with_hints(ctx,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "Result<T>: concrete Ok type metadata is required");
            }
            return NULL;
        }
        if (strcmp(ok_name, "Void") == 0) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "Result<Void> does not lower until the Result<Void> ABI is frozen");
            return NULL;
        }
        LLVMTypeRef ok_ty  = pergyra_type_to_llvm(ctx, ok_name);
        LLVMTypeRef err_ty = pergyra_type_to_llvm(ctx,
            err_name != NULL ? err_name : "PgyError");
        if (ctx->has_error || ok_ty == NULL || err_ty == NULL)
            return NULL;
        LLVMTypeRef fields[] = {
            ctx->type_i32,
            ok_ty,
            err_ty
        };
        return LLVMStructTypeInContext(ctx->context, fields, 3, 0);
    }
    case PGY_TK_OPTION: {
        LLVMTypeRef inner = llvm_resolve_inner_type(ctx, type_name);
        if (ctx->has_error || inner == NULL)
            return NULL;
        LLVMTypeRef fields[] = { ctx->type_i32, inner };
        return LLVMStructTypeInContext(ctx->context, fields, 2, 0);
    }
    case PGY_TK_SLOT: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Slot<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_slot_struct_type(ctx, inner_buf);
    }
    case PGY_TK_SECURE_SLOT: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "SecureSlot<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_secure_slot_struct_type(ctx, inner_buf);
    }
    case PGY_TK_DEVICE_SLOT: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "DeviceSlot<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_slot_struct_type(ctx, inner_buf);
    }
    case PGY_TK_REMOTE_FUTURE:
        return ctx->type_task_handle;
    case PGY_TK_ARRAY: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Array<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_array_struct_type(ctx, inner_buf);
    }
    case PGY_TK_SLICE: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Slice<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        return llvm_slice_struct_type(ctx, inner_buf);
    }
    case PGY_TK_CHANNEL: {
        char inner_buf[256];
        if (!llvm_required_constructed_arg_name_copy(ctx, type_name, 0,
                "Channel<T>", inner_buf, sizeof(inner_buf))) {
            return NULL;
        }
        if (!pgy_channel_runtime_payload_has_abi(inner_buf)) {
            llvm_set_error_with_hints(ctx,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM type '%s' has no runtime Channel<%s> ABI; beta runtime supports %s",
                type_name,
                inner_buf,
                pgy_channel_runtime_payload_supported_list());
            return NULL;
        }
        return ctx->type_i8ptr;
    }
    case PGY_TK_BOX:
    case PGY_TK_RC:
    case PGY_TK_WEAK:
        return ctx->type_i8ptr;
    case PGY_TK_FUTURE:
        return ctx->type_task_handle;

    case PGY_TK_UNKNOWN:
    case PGY_TK_CLASS:
        break;
    default:
        break;
    }

    /* Check if it's a registered class type */
    LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
    if (cls != NULL)
        return cls->struct_type;

    if (llvm_find_enum_decl(ctx, type_name) != NULL)
        return ctx->type_i32;

    {
        LLVMTypeRef specialized =
            llvm_specialize_generic_class_type(ctx, type_name);
        if (specialized != NULL)
            return specialized;
        if (ctx != NULL && ctx->has_error)
            return NULL;
    }

    /* Closure #75: unresolved generic placeholder names (single uppercase
     * letter — T, K, V, ...) fall through to type-erased i8ptr. Generic
     * type parameters in ability/role/intent contexts are erased at the
     * LLVM ABI boundary; the concrete type lives only at the type-checker
     * level. Strict-mode rejection here is too aggressive for the existing
     * generic-ability surface (generic_ability_requires_minimal,
     * logistics_intent_probe). */
    if (type_name != NULL && type_name[0] >= 'A' && type_name[0] <= 'Z'
        && type_name[1] == '\0') {
        return ctx->type_i8ptr;
    }

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_with_hints(ctx,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM type '%s' is not registered in the LLVM type map; silent i32 fallback is not allowed",
            type_name);
    }
    return NULL;
}
#endif /* PGY_LLVM_ENABLED */
