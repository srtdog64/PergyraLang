/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM domain role lookup helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_domain_role_helpers.h"
#include "llvm_inventory_decl_lookup.h"
#include "llvm_inventory_host_methods.h"
#include "../compiler/mir_decl_headers.h"

bool
llvm_role_method_symbol_name(char *out,
                             size_t out_size,
                             const char *role_name,
                             const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0 || role_name == NULL
        || method_name == NULL) {
        return false;
    }

    written = snprintf(out, out_size, "%s_%s", role_name, method_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
llvm_role_operator_symbol_name(char *out,
                               size_t out_size,
                               const char *suffix,
                               const char *for_type_name)
{
    int written;

    if (out == NULL || out_size == 0 || suffix == NULL
        || for_type_name == NULL) {
        return false;
    }

    written = snprintf(out, out_size, "operator_%s_%s", suffix, for_type_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
llvm_role_vtable_type_name(char *out,
                           size_t out_size,
                           const char *ability_name)
{
    int written;

    if (out == NULL || out_size == 0 || ability_name == NULL)
        return false;

    written = snprintf(out, out_size, "%s_vtable", ability_name);
    return written >= 0 && (size_t)written < out_size;
}

bool
llvm_role_vtable_global_name(char *out,
                             size_t out_size,
                             const char *role_name,
                             const char *ability_name)
{
    int written;

    if (out == NULL || out_size == 0 || role_name == NULL
        || ability_name == NULL) {
        return false;
    }

    written = snprintf(out, out_size, "%s_%s_vtable_instance",
        role_name, ability_name);
    return written >= 0 && (size_t)written < out_size;
}

static void
llvm_ability_tag_sanitize(const char *in, char *out, size_t out_size)
{
    size_t j = 0;
    bool last_under = false;

    if (out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if (in == NULL)
        return;
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_size; i++) {
        char c = in[i];
        bool keep = (c >= '0' && c <= '9')
            || (c >= 'A' && c <= 'Z')
            || (c >= 'a' && c <= 'z')
            || c == '_';
        if (keep) {
            out[j++] = c;
            last_under = c == '_';
        } else if (!last_under) {
            out[j++] = '_';
            last_under = true;
        }
    }
    while (j > 0 && out[j - 1] == '_')
        j--;
    out[j] = '\0';
}

static const char *
llvm_ability_arg_bound_name(LLVMGenCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;
    for (int i = ctx->type_subst_count - 1; i >= 0; i--) {
        if (ctx->type_subst[i].param_name != NULL
            && strcmp(ctx->type_subst[i].param_name, name) == 0) {
            return ctx->type_subst[i].type_name;
        }
    }
    return NULL;
}

static bool
llvm_subst_ability_arg_type_name(LLVMGenCtx *ctx,
                                 const char *in,
                                 char *out,
                                 size_t out_size)
{
    size_t oi = 0;
    size_t i = 0;

    if (ctx == NULL || in == NULL || out == NULL || out_size == 0)
        return false;
    while (in[i] != '\0') {
        char c = in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
            size_t start = i;
            char tok[128];
            size_t len;
            const char *bound;
            const char *rep;
            while (in[i] != '\0'
                && ((in[i] >= 'A' && in[i] <= 'Z')
                    || (in[i] >= 'a' && in[i] <= 'z')
                    || (in[i] >= '0' && in[i] <= '9') || in[i] == '_'))
                i++;
            len = i - start;
            if (len >= sizeof(tok))
                return false;
            memcpy(tok, in + start, len);
            tok[len] = '\0';
            bound = llvm_ability_arg_bound_name(ctx, tok);
            rep = bound != NULL ? bound : tok;
            for (size_t k = 0; rep[k] != '\0'; k++) {
                if (oi + 1 >= out_size)
                    return false;
                out[oi++] = rep[k];
            }
        } else {
            if (oi + 1 >= out_size)
                return false;
            out[oi++] = c;
            i++;
        }
    }
    out[oi] = '\0';
    return true;
}

static const char *
llvm_keep_ability_tag(LLVMGenCtx *ctx, const char *rendered)
{
    char suffix[128];

    if (ctx == NULL || rendered == NULL)
        return NULL;
    llvm_ability_tag_sanitize(rendered, suffix, sizeof(suffix));
    if (suffix[0] == '\0')
        return NULL;
    return pgy_arena_strdup(&ctx->persistent, suffix);
}

static const char *
llvm_render_ability_actual_name(LLVMGenCtx *ctx, const char *type_name)
{
    char subst[256];

    if (ctx == NULL || type_name == NULL)
        return NULL;
    if (llvm_subst_ability_arg_type_name(
            ctx, type_name, subst, sizeof(subst))) {
        return pgy_arena_strdup(&ctx->scratch, subst);
    }
    return pgy_arena_strdup(&ctx->scratch, type_name);
}

static const char *
llvm_render_ability_formal_fallback(LLVMGenCtx *ctx, GenericParam *formal)
{
    ASTNode *fallback;
    char *rendered;

    if (ctx == NULL || formal == NULL)
        return NULL;
    fallback = ast_generic_param_default_type(formal);
    if (fallback == NULL)
        fallback = ast_generic_param_constraint(formal);
    if (fallback == NULL)
        return NULL;
    rendered = llvm_render_type_name_in_ctx(ctx, fallback);
    return llvm_keep_rendered_persistent(
        ctx, rendered, "LLVM ability generic fallback copy failed");
}

static const char *
llvm_render_mir_ability_formal_fallback(LLVMGenCtx *ctx,
                                        const MIRDeclGenericParam *formal)
{
    const char *fallback;

    if (ctx == NULL || formal == NULL)
        return NULL;
    fallback = mir_decl_generic_param_default_type_name(formal);
    if (fallback == NULL)
        fallback = mir_decl_generic_param_constraint_type_name(formal);
    return llvm_render_ability_actual_name(ctx, fallback);
}

const char *
llvm_render_mir_ability_ref_vtable_tag(LLVMGenCtx *ctx,
                                       const MIRAbilityRef *ability_ref)
{
    const char *base_name;
    const MIRDeclHeader *ability_header = NULL;
    ASTNode *ability_decl;
    GenericParams *generics;
    bool mir_active = llvm_active_has_mir(ctx);
    size_t generic_count;
    size_t actual_count;
    size_t rendered_count;
    char rendered[512];
    size_t offset = 0;

    if (ctx == NULL || ability_ref == NULL)
        return NULL;
    base_name = mir_ability_ref_base_name(ability_ref);
    if (base_name == NULL)
        return NULL;
    if (mir_active) {
        ability_header = llvm_find_decl_header_in_context_of_type(
            ctx, AST_ABILITY_DECL, base_name);
        if (ability_header == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing ability declaration header for ability tag '%s'",
                base_name);
            return NULL;
        }
    }
    ability_decl = !mir_active
        ? llvm_find_decl_in_active_inventory(ctx, AST_ABILITY_DECL, base_name)
        : NULL;
    generics = ability_decl != NULL && ability_decl->type == AST_ABILITY_DECL
        ? ast_declaration_generic_params(ability_decl) : NULL;
    generic_count = ability_header != NULL
        ? mir_decl_header_generic_param_count(ability_header)
        : ast_generic_param_count(generics);
    actual_count = mir_ability_ref_actual_arg_count(ability_ref);
    rendered_count = generic_count > actual_count ? generic_count : actual_count;
    if (rendered_count == 0)
        return llvm_keep_ability_tag(ctx, base_name);

    {
        int written = snprintf(rendered, sizeof(rendered), "%s<", base_name);
        if (written < 0 || (size_t)written >= sizeof(rendered))
            return NULL;
        offset = (size_t)written;
    }
    for (size_t i = 0; i < rendered_count; i++) {
        const char *arg = NULL;
        int written;

        if (i < actual_count) {
            arg = llvm_render_ability_actual_name(
                ctx, mir_ability_ref_actual_arg_type_name(ability_ref, i));
        }
        if (arg == NULL && i < generic_count) {
            arg = ability_header != NULL
                ? llvm_render_mir_ability_formal_fallback(
                    ctx, mir_decl_header_generic_param(ability_header, i))
                : llvm_render_ability_formal_fallback(
                    ctx, ast_generic_param_at(generics, i));
        }
        if (arg == NULL) {
            if (mir_active) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing generic ability argument metadata for '%s' at index %llu",
                    base_name, (unsigned long long)i);
                return NULL;
            }
            return llvm_keep_ability_tag(ctx, base_name);
        }
        written = snprintf(rendered + offset, sizeof(rendered) - offset,
            "%s%s", i > 0 ? ", " : "", arg);
        if (written < 0 || (size_t)written >= sizeof(rendered) - offset)
            return NULL;
        offset += (size_t)written;
    }
    if (offset + 2 > sizeof(rendered))
        return NULL;
    rendered[offset++] = '>';
    rendered[offset] = '\0';
    return llvm_keep_ability_tag(ctx, rendered);
}

const char *
llvm_render_ast_ability_ref_vtable_tag(LLVMGenCtx *ctx, ASTNode *ability_ref)
{
    const char *base_name;
    const MIRDeclHeader *ability_header = NULL;
    ASTNode *ability_decl;
    GenericParams *generics;
    GenericParams *actuals;
    bool mir_active = llvm_active_has_mir(ctx);
    size_t generic_count;
    size_t actual_count;
    size_t rendered_count;
    char rendered[512];
    size_t offset;

    if (ctx == NULL || ability_ref == NULL || ability_ref->type != AST_TYPE)
        return NULL;
    base_name = ast_type_name(ability_ref);
    if (base_name == NULL)
        return NULL;
    if (mir_active) {
        ability_header = llvm_find_decl_header_in_context_of_type(
            ctx, AST_ABILITY_DECL, base_name);
    }
    ability_decl = !mir_active
        ? llvm_find_decl_in_active_inventory(ctx, AST_ABILITY_DECL, base_name)
        : NULL;
    generics = ability_decl != NULL && ability_decl->type == AST_ABILITY_DECL
        ? ast_declaration_generic_params(ability_decl) : NULL;
    actuals = ast_type_generic_args(ability_ref);
    generic_count = ability_header != NULL
        ? mir_decl_header_generic_param_count(ability_header)
        : ast_generic_param_count(generics);
    actual_count = ast_generic_param_count(actuals);
    rendered_count = generic_count > actual_count ? generic_count : actual_count;
    if (rendered_count == 0)
        return llvm_keep_ability_tag(ctx, base_name);

    {
        int written = snprintf(rendered, sizeof(rendered), "%s<", base_name);
        if (written < 0 || (size_t)written >= sizeof(rendered))
            return NULL;
        offset = (size_t)written;
    }
    for (size_t i = 0; i < rendered_count; i++) {
        const char *arg = NULL;
        char *owned_arg = NULL;
        int written;

        if (i < actual_count) {
            GenericParam *actual = ast_generic_param_at(actuals, i);
            ASTNode *actual_type = ast_generic_param_constraint(actual);
            if (actual_type != NULL) {
                owned_arg = llvm_render_type_name_in_ctx(ctx, actual_type);
                arg = llvm_render_ability_actual_name(ctx, owned_arg);
            } else {
                arg = llvm_render_ability_actual_name(
                    ctx, ast_generic_param_name(actual));
            }
        }
        if (arg == NULL && i < generic_count) {
            arg = ability_header != NULL
                ? llvm_render_mir_ability_formal_fallback(
                    ctx, mir_decl_header_generic_param(ability_header, i))
                : llvm_render_ability_formal_fallback(
                    ctx, ast_generic_param_at(generics, i));
        }
        free(owned_arg);
        if (arg == NULL)
            return llvm_keep_ability_tag(ctx, base_name);
        written = snprintf(rendered + offset, sizeof(rendered) - offset,
            "%s%s", i > 0 ? ", " : "", arg);
        if (written < 0 || (size_t)written >= sizeof(rendered) - offset)
            return NULL;
        offset += (size_t)written;
    }
    if (offset + 2 > sizeof(rendered))
        return NULL;
    rendered[offset++] = '>';
    rendered[offset] = '\0';
    return llvm_keep_ability_tag(ctx, rendered);
}

ASTNode *
llvm_find_role_decl(LLVMGenCtx *ctx, const char *role_name)
{
    if (ctx == NULL || role_name == NULL)
        return NULL;
    return llvm_find_decl_in_active_inventory(ctx, AST_ROLE_DECL, role_name);
}

ASTNode *
llvm_role_for_type_node(ASTNode *role)
{
    return ast_role_for_type(role);
}

const char *
llvm_role_for_type_name(ASTNode *role)
{
    ASTNode *for_type = llvm_role_for_type_node(role);

    return ast_type_name(for_type);
}

ASTNode *
llvm_find_role_operator_method(LLVMGenCtx *ctx, ASTNode *role,
                               PgyTokenType op, int depth)
{
    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL || depth > 16)
        return NULL;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;
        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            ASTNode *method = ast_impl_ability_method(impl, j);
            const char *method_name = ast_declaration_name(method);
            if (method != NULL && method->type == AST_FUNC_DECL
                && method_name != NULL
                && llvm_operator_method_name_matches(op, method_name)) {
                return method;
            }
        }
    }

    for (size_t i = 0; i < ast_role_include_count(role); i++) {
        ASTNode *inc = ast_role_include(role, i);
        const char *role_name = ast_include_role_name(inc);
        ASTNode *included;
        ASTNode *method;

        if (role_name == NULL)
            continue;
        included = llvm_find_role_decl(ctx, role_name);
        method = llvm_find_role_operator_method(ctx, included, op, depth + 1);
        if (method != NULL)
            return method;
    }

    return NULL;
}

static const MIRDeclMethod *
llvm_find_role_operator_method_metadata_in_header(LLVMGenCtx *ctx,
                                                 const char *role_name,
                                                 const MIRDeclHeader *role_header,
                                                 PgyTokenType op,
                                                 int depth)
{
    if (ctx == NULL || depth > 16)
        return NULL;
    if (role_header == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing role operator method metadata for role '%s'",
            role_name != NULL ? role_name : "(anonymous-role)");
        return NULL;
    }

    for (size_t i = 0; i < mir_decl_header_method_count(role_header); i++) {
        const MIRDeclMethod *method =
            mir_decl_header_method(role_header, i);
        const char *method_name = llvm_mir_decl_method_name(method);

        if (method_name != NULL
            && llvm_operator_method_name_matches(op, method_name)) {
            return method;
        }
    }

    for (size_t i = 0;
         i < mir_decl_header_role_include_count(role_header);
         i++) {
        const MIRDeclRoleInclude *include_meta =
            mir_decl_header_role_include(role_header, i);
        const char *included_name =
            mir_decl_role_include_name(include_meta);
        const MIRDeclHeader *included_header;
        const MIRDeclMethod *method;

        if (included_name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing included role name metadata for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)");
            return NULL;
        }
        included_header = llvm_find_decl_header_in_context_of_type(
            ctx, AST_ROLE_DECL, included_name);
        if (included_header == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing role include metadata header for included role '%s'",
                included_name);
            return NULL;
        }
        method = llvm_find_role_operator_method_metadata_in_header(
            ctx, included_name, included_header, op, depth + 1);
        if (method != NULL)
            return method;
        if (ctx != NULL && ctx->has_error)
            return NULL;
    }

    return NULL;
}

const MIRDeclMethod *
llvm_find_role_operator_method_metadata(LLVMGenCtx *ctx,
                                        ASTNode *role,
                                        PgyTokenType op,
                                        int depth)
{
    const char *role_name;
    LLVMHostedMethodView view;

    if (ctx == NULL || role == NULL || role->type != AST_ROLE_DECL
        || depth > 16 || !llvm_active_has_mir(ctx)) {
        return NULL;
    }

    role_name = llvm_decl_node_name(role);
    view = llvm_hosted_method_view_from_decl(ctx, role_name, role);
    if (llvm_hosted_method_view_missing_mir_metadata(&view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing role operator method metadata for role '%s'",
            role_name != NULL ? role_name : "(anonymous-role)");
        return NULL;
    }
    return llvm_find_role_operator_method_metadata_in_header(
        ctx, role_name, view.decl_header, op, depth);
}

const char *
llvm_party_slot_first_ability_tag(LLVMGenCtx *ctx,
                                  const char *party_type_name,
                                  const char *slot_name)
{
    ASTNode *party_decl;
    LLVMHostedRoleSlotView role_view;

    if (ctx == NULL || party_type_name == NULL || slot_name == NULL)
        return NULL;

    party_decl = llvm_find_named_domain_decl(ctx, AST_PARTY_DECL,
        party_type_name);
    if (party_decl == NULL || party_decl->type != AST_PARTY_DECL)
        return NULL;

    role_view = llvm_hosted_role_slot_view_from_decl(
        ctx, party_type_name, party_decl);
    if (llvm_hosted_role_slot_view_missing_mir_metadata(&role_view))
        return NULL;

    for (size_t i = 0; i < role_view.count; i++) {
        const char *role_slot_name =
            llvm_hosted_role_slot_view_name(&role_view, i);

        if (role_slot_name == NULL
            || strcmp(role_slot_name, slot_name) != 0) {
            continue;
        }

        return llvm_render_mir_ability_ref_vtable_tag(
            ctx,
            llvm_hosted_role_slot_view_required_ability_ref(
                &role_view, i, 0));
    }

    return NULL;
}

LLVMValueRef
llvm_lookup_role_vtable_global(LLVMGenCtx *ctx,
                               const char *role_name,
                               const char *ability_name)
{
    char global_name[256];

    if (ctx == NULL || role_name == NULL || ability_name == NULL)
        return NULL;
    if (!llvm_role_vtable_global_name(global_name, sizeof(global_name),
            role_name, ability_name)) {
        return NULL;
    }

    return LLVMGetNamedGlobal(ctx->module, global_name);
}

#endif /* PGY_LLVM_ENABLED */
