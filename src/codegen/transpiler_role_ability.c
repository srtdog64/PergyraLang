#include "transpiler.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_role_ability_helpers.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

bool
role_has_ability(ASTNode *role, const char *ability_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || ability_name == NULL)
        return false;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl != NULL
            && impl->type == AST_IMPL_ABILITY
            && ast_impl_ability_name(impl) != NULL
            && strcmp(ast_impl_ability_name(impl), ability_name) == 0) {
            return true;
        }
    }

    return false;
}

char *
render_ability_ref_vtable_tag_in_ctx(TranspilerCtx *ctx, ASTNode *ability_ref)
{
    char suffix[128];
    size_t len;
    char *rendered;

    if (ability_ref == NULL)
        return NULL;

    rendered = render_type_name_in_ctx(ctx, ability_ref);
    if (rendered == NULL)
        return NULL;
    sanitize_c_suffix(rendered, suffix, sizeof(suffix));
    len = strlen(suffix);
    while (len > 0 && suffix[len - 1] == '_')
        suffix[--len] = '\0';
    if (len == 0) {
        free(rendered);
        return NULL;
    }
    free(rendered);
    return pergyra_strdup(suffix);
}

char *
render_ability_ref_vtable_tag(ASTNode *ability_ref)
{
    return render_ability_ref_vtable_tag_in_ctx(NULL, ability_ref);
}

char *
render_ability_type_name_vtable_tag(const char *ability_type_name)
{
    char suffix[128];
    size_t len;

    if (ability_type_name == NULL)
        return NULL;
    sanitize_c_suffix(ability_type_name, suffix, sizeof(suffix));
    len = strlen(suffix);
    while (len > 0 && suffix[len - 1] == '_')
        suffix[--len] = '\0';
    if (len == 0)
        return NULL;
    return pergyra_strdup(suffix);
}

static bool
ability_type_name_base_copy(const char *ability_type_name,
                            char *out,
                            size_t out_size)
{
    const char *generic_start;
    size_t len;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (ability_type_name == NULL)
        return false;

    generic_start = strchr(ability_type_name, '<');
    len = generic_start != NULL
        ? (size_t)(generic_start - ability_type_name)
        : strlen(ability_type_name);
    while (len > 0
        && (ability_type_name[len - 1] == ' '
            || ability_type_name[len - 1] == '\t')) {
        len--;
    }
    if (len == 0 || len >= out_size)
        return false;
    memcpy(out, ability_type_name, len);
    out[len] = '\0';
    return true;
}

static const char *
transpiler_ability_arg_bound_name(TranspilerCtx *ctx, const char *name)
{
    if (ctx == NULL || name == NULL)
        return NULL;

    for (int i = ctx->generic_binding_count - 1; i >= 0; i--) {
        if (strcmp(ctx->generic_bindings[i].name, name) == 0)
            return ctx->generic_bindings[i].concrete_type;
    }
    return NULL;
}

static bool
transpiler_subst_ability_arg_type_name(TranspilerCtx *ctx,
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
            bound = transpiler_ability_arg_bound_name(ctx, tok);
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

static char *
transpiler_render_ability_actual_name(TranspilerCtx *ctx,
                                      const char *type_name)
{
    char subst[256];

    if (type_name == NULL)
        return NULL;
    if (transpiler_subst_ability_arg_type_name(
            ctx, type_name, subst, sizeof(subst))) {
        return pergyra_strdup(subst);
    }
    return pergyra_strdup(type_name);
}

static char *
transpiler_render_ability_formal_fallback(TranspilerCtx *ctx,
                                          GenericParam *formal)
{
    ASTNode *fallback;

    if (formal == NULL)
        return NULL;
    fallback = ast_generic_param_default_type(formal);
    if (fallback == NULL)
        fallback = ast_generic_param_constraint(formal);
    return fallback != NULL ? render_type_name_in_ctx(ctx, fallback) : NULL;
}

static char *
transpiler_render_mir_ability_formal_fallback(
    TranspilerCtx *ctx,
    const MIRDeclGenericParam *formal)
{
    const char *fallback;

    if (formal == NULL)
        return NULL;
    fallback = mir_decl_generic_param_default_type_name(formal);
    if (fallback == NULL)
        fallback = mir_decl_generic_param_constraint_type_name(formal);
    return transpiler_render_ability_actual_name(ctx, fallback);
}

static char *
render_ability_ref_parts_vtable_tag_in_ctx(TranspilerCtx *ctx,
                                           const char *base_name,
                                           const MIRAbilityRef *ability_ref)
{
    const MIRDeclHeader *ability_header = NULL;
    ASTNode *ability_decl;
    GenericParams *generics;
    bool mir_active = ctx != NULL && transpiler_active_has_mir(ctx);
    size_t generic_count;
    size_t actual_count;
    size_t rendered_count;
    CodeBuf *buf;
    char *tag;

    if (ability_ref != NULL)
        base_name = mir_ability_ref_base_name(ability_ref);
    if (base_name == NULL)
        return NULL;

    if (mir_active) {
        ability_header =
            transpiler_active_decl_header_of_type(
                ctx, AST_ABILITY_DECL, base_name);
    }
    ability_decl = !mir_active && ctx != NULL
        ? find_ability_decl(ctx, base_name)
        : NULL;
    generics = ability_decl != NULL && ability_decl->type == AST_ABILITY_DECL
        ? ast_declaration_generic_params(ability_decl)
        : NULL;
    generic_count = ability_header != NULL
        ? mir_decl_header_generic_param_count(ability_header)
        : ast_generic_param_count(generics);
    actual_count = ability_ref != NULL
        ? mir_ability_ref_actual_arg_count(ability_ref) : 0;
    rendered_count = generic_count > actual_count ? generic_count : actual_count;
    if (rendered_count == 0)
        return render_ability_type_name_vtable_tag(base_name);

    buf = codebuf_create();
    if (buf == NULL)
        return NULL;
    codebuf_write(buf, "%s<", base_name);
    for (size_t i = 0; i < rendered_count; i++) {
        char *rendered = NULL;
        if (ability_ref != NULL && i < actual_count) {
            rendered = transpiler_render_ability_actual_name(
                ctx, mir_ability_ref_actual_arg_type_name(ability_ref, i));
        }
        if (rendered == NULL && i < generic_count) {
            rendered = ability_header != NULL
                ? transpiler_render_mir_ability_formal_fallback(
                    ctx, mir_decl_header_generic_param(ability_header, i))
                : transpiler_render_ability_formal_fallback(
                    ctx, ast_generic_param_at(generics, i));
        }
        if (rendered == NULL) {
            codebuf_destroy(buf);
            return render_ability_type_name_vtable_tag(base_name);
        }
        if (i > 0)
            codebuf_write(buf, ", ");
        codebuf_write(buf, "%s", rendered);
        free(rendered);
    }
    codebuf_write(buf, ">");
    tag = render_ability_type_name_vtable_tag(buf->data);
    codebuf_destroy(buf);
    return tag;
}

char *
render_ability_type_name_vtable_tag_in_ctx(TranspilerCtx *ctx,
                                           const char *ability_type_name)
{
    char base_name[128];

    if (ability_type_name == NULL)
        return NULL;
    if (strchr(ability_type_name, '<') != NULL)
        return render_ability_type_name_vtable_tag(ability_type_name);
    if (!ability_type_name_base_copy(
            ability_type_name, base_name, sizeof(base_name))) {
        return NULL;
    }
    return render_ability_ref_parts_vtable_tag_in_ctx(ctx, base_name, NULL);
}

char *
render_mir_ability_ref_vtable_tag_in_ctx(TranspilerCtx *ctx,
                                         const MIRAbilityRef *ability_ref)
{
    return render_ability_ref_parts_vtable_tag_in_ctx(ctx, NULL, ability_ref);
}

bool
role_has_method(ASTNode *role, const char *method_name)
{
    if (role == NULL || role->type != AST_ROLE_DECL || method_name == NULL)
        return false;

    for (size_t i = 0; i < ast_role_impl_count(role); i++) {
        ASTNode *impl = ast_role_impl(role, i);
        if (impl == NULL || impl->type != AST_IMPL_ABILITY)
            continue;

        for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
            ASTNode *method = ast_impl_ability_method(impl, j);
            const char *candidate_name = ast_declaration_name(method);
            if (method != NULL
                && method->type == AST_FUNC_DECL
                && candidate_name != NULL
                && strcmp(candidate_name, method_name) == 0) {
                return true;
            }
        }
    }

    return false;
}

char *
transpiler_party_slot_first_ability_tag(TranspilerCtx *ctx,
                                        ASTNode *party_decl,
                                        const char *slot_name)
{
    const char *party_name;
    TranspilerHostedRoleSlotView role_view;

    if (ctx == NULL || party_decl == NULL || party_decl->type != AST_PARTY_DECL
        || slot_name == NULL) {
        return NULL;
    }

    party_name = transpiler_decl_name_local(party_decl);
    role_view =
        transpiler_hosted_role_slot_view_from_decl(ctx, party_name, party_decl);
    if (transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view))
        return NULL;

    for (size_t i = 0; i < role_view.count; i++) {
        const char *role_slot_name =
            transpiler_hosted_role_slot_view_name(&role_view, i);
        if (role_slot_name == NULL
            || strcmp(role_slot_name, slot_name) != 0) {
            continue;
        }
        return render_mir_ability_ref_vtable_tag_in_ctx(
            ctx,
            transpiler_hosted_role_slot_view_required_ability_ref(
                &role_view, i, 0));
    }

    return NULL;
}

char *
transpiler_party_slot_method_ability_tag(TranspilerCtx *ctx,
                                         ASTNode *party_decl,
                                         const char *slot_name,
                                         const char *method_name)
{
    char *fallback_tag = NULL;
    const char *party_name;
    TranspilerHostedRoleSlotView role_view;

    if (ctx == NULL || party_decl == NULL || party_decl->type != AST_PARTY_DECL
        || slot_name == NULL || method_name == NULL) {
        return NULL;
    }

    party_name = transpiler_decl_name_local(party_decl);
    role_view =
        transpiler_hosted_role_slot_view_from_decl(ctx, party_name, party_decl);
    if (transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view))
        return NULL;

    for (size_t i = 0; i < role_view.count; i++) {
        const char *role_slot_name =
            transpiler_hosted_role_slot_view_name(&role_view, i);
        size_t ability_count;
        if (role_slot_name == NULL
            || strcmp(role_slot_name, slot_name) != 0) {
            continue;
        }
        ability_count =
            transpiler_hosted_role_slot_view_required_ability_count(
                &role_view, i);

        for (size_t j = 0; j < ability_count; j++) {
            ASTNode *ability_decl;
            bool has_method = false;
            char *ability_tag;
            const MIRAbilityRef *ability_ref =
                transpiler_hosted_role_slot_view_required_ability_ref(
                    &role_view, i, j);
            const char *ability_name =
                mir_ability_ref_base_name(ability_ref);

            if (ability_name == NULL)
                continue;
            ability_decl = find_ability_decl(ctx, ability_name);
            if (ability_decl != NULL) {
                for (size_t mi = 0;
                     mi < ast_ability_method_count(ability_decl); mi++) {
                    ASTNode *method = ast_ability_method(ability_decl, mi);
                    const char *candidate_name = ast_declaration_name(method);
                    if (method != NULL && method->type == AST_FUNC_DECL
                        && candidate_name != NULL
                        && strcmp(candidate_name, method_name) == 0) {
                        has_method = true;
                        break;
                    }
                }
            }

            ability_tag = render_mir_ability_ref_vtable_tag_in_ctx(
                ctx, ability_ref);
            if (has_method) {
                free(fallback_tag);
                return ability_tag;
            }
            if (fallback_tag == NULL) {
                fallback_tag = ability_tag;
            } else {
                free(ability_tag);
            }
        }
        break;
    }

    return fallback_tag;
}
