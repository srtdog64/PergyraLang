#include "transpiler_domain_role_ability_mir_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"
#include "../semantic/diag_codes.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_role_ability_names.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_type_require.h"

static const char *
ability_vtable_bound_type_name(TranspilerCtx *ctx,
                               GenericBindingEntry *bindings,
                               size_t binding_count,
                               const char *token)
{
    if (token == NULL)
        return NULL;

    for (size_t i = 0; i < binding_count; i++) {
        if (bindings[i].name[0] != '\0'
            && bindings[i].concrete_type[0] != '\0'
            && strcmp(bindings[i].name, token) == 0) {
            return bindings[i].concrete_type;
        }
    }
    if (ctx != NULL) {
        for (int i = ctx->generic_binding_count - 1; i >= 0; i--) {
            if (strcmp(ctx->generic_bindings[i].name, token) == 0)
                return ctx->generic_bindings[i].concrete_type;
        }
    }
    return NULL;
}

static char *
ability_vtable_substitute_type_name(TranspilerCtx *ctx,
                                    const char *type_name,
                                    GenericBindingEntry *bindings,
                                    size_t binding_count)
{
    CodeBuf *buf;
    size_t i = 0;

    if (type_name == NULL)
        return NULL;
    buf = codebuf_create();
    if (buf == NULL)
        return NULL;

    while (type_name[i] != '\0') {
        char c = type_name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
            size_t start = i;
            size_t len;
            char token[128];
            const char *bound;
            while (type_name[i] != '\0'
                && ((type_name[i] >= 'A' && type_name[i] <= 'Z')
                    || (type_name[i] >= 'a' && type_name[i] <= 'z')
                    || (type_name[i] >= '0' && type_name[i] <= '9')
                    || type_name[i] == '_')) {
                i++;
            }
            len = i - start;
            if (len >= sizeof(token)) {
                for (size_t k = start; k < i; k++)
                    codebuf_write(buf, "%c", type_name[k]);
                continue;
            }
            memcpy(token, type_name + start, len);
            token[len] = '\0';
            bound = ability_vtable_bound_type_name(
                ctx, bindings, binding_count, token);
            codebuf_write(buf, "%s", bound != NULL ? bound : token);
        } else {
            codebuf_write(buf, "%c", c);
            i++;
        }
    }

    char *result = pergyra_strdup(buf->data);
    codebuf_destroy(buf);
    return result;
}

static bool
build_mir_ability_ref_bindings(const MIRDeclHeader *ability_header,
                               const MIRAbilityRef *ability_ref,
                               TranspilerCtx *ctx,
                               GenericBindingEntry *bindings,
                               size_t *binding_count,
                               const char *ability_name)
{
    size_t out = 0;
    size_t generic_count;
    size_t actual_count;

    if (binding_count != NULL)
        *binding_count = 0;
    if (ability_header == NULL || bindings == NULL)
        return false;

    generic_count = mir_decl_header_generic_param_count(ability_header);
    actual_count = ability_ref != NULL
        ? mir_ability_ref_actual_arg_count(ability_ref) : 0;
    for (size_t i = 0; i < generic_count; i++) {
        const MIRDeclGenericParam *formal =
            mir_decl_header_generic_param(ability_header, i);
        const char *formal_name = mir_decl_generic_param_name(formal);
        const char *type_name = NULL;
        char *resolved = NULL;

        if (formal_name == NULL)
            continue;
        if (i < actual_count)
            type_name = mir_ability_ref_actual_arg_type_name(ability_ref, i);
        if (type_name == NULL)
            type_name = mir_decl_generic_param_default_type_name(formal);
        if (type_name == NULL)
            type_name = mir_decl_generic_param_constraint_type_name(formal);
        if (type_name == NULL)
            continue;
        if (out >= MAX_GENERIC_BINDINGS) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend generic ability binding registry exceeded MAX_GENERIC_BINDINGS while lowering ability '%s'",
                ability_name != NULL ? ability_name : "<ability>");
            return false;
        }
        resolved = ability_vtable_substitute_type_name(
            ctx, type_name, bindings, out);
        if (resolved == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot render generic ability binding '%s' for ability '%s'",
                formal_name, ability_name != NULL ? ability_name : "<ability>");
            return false;
        }
        if (!transpiler_role_ability_copy_name(
                bindings[out].name, sizeof(bindings[out].name), formal_name)
            || !transpiler_role_ability_copy_name(
                bindings[out].concrete_type,
                sizeof(bindings[out].concrete_type), resolved)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: generic ability binding name is too long for ability '%s'",
                ability_name != NULL ? ability_name : "<ability>");
            free(resolved);
            return false;
        }
        free(resolved);
        out++;
    }

    if (binding_count != NULL)
        *binding_count = out;
    return true;
}

bool
transpiler_emit_mir_ability_ref_vtable_decl(
    CodeBuf *target,
    TranspilerCtx *ctx,
    const MIRDeclHeader *ability_header,
    const MIRAbilityRef *ability_ref,
    const char *ability_name,
    const char *typedef_name)
{
    GenericBindingEntry bindings[MAX_GENERIC_BINDINGS];
    size_t binding_count = 0;

    if (target == NULL || ctx == NULL || ability_header == NULL
        || typedef_name == NULL) {
        return false;
    }
    if (!build_mir_ability_ref_bindings(ability_header, ability_ref, ctx,
            bindings, &binding_count, ability_name)) {
        return false;
    }

    codebuf_write(target, "\ntypedef struct\n{\n");
    for (size_t i = 0; i < mir_decl_header_method_count(ability_header); i++) {
        const MIRDeclMethod *method_meta =
            mir_decl_header_method(ability_header, i);
        const char *method_name;
        const char *return_type_name;
        char *effective_return_name = NULL;
        char ret_type_buf[256];
        const char *ret_type = "void";

        if (method_meta == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing ability vtable method metadata row for '%s'",
                ability_name != NULL ? ability_name : "(anonymous-ability)");
            return false;
        }
        method_name = transpiler_mir_decl_method_name(method_meta);
        if (method_name == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing ability vtable method name metadata for '%s'",
                ability_name != NULL ? ability_name : "(anonymous-ability)");
            return false;
        }
        if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
                method_meta,
                ability_name,
                method_name,
                TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
                "MIR-only C path missing ability vtable return type-name metadata for '%s.%s'",
                "MIR-only C path missing ability vtable parameter type-name metadata for '%s.%s'")) {
            return false;
        }
        return_type_name =
            transpiler_mir_decl_method_return_type_name(method_meta);
        if (return_type_name != NULL) {
            effective_return_name = ability_vtable_substitute_type_name(
                ctx, return_type_name, bindings, binding_count);
            if (effective_return_name == NULL
                || !transpiler_require_type_name_c_type_copy(ctx,
                    effective_return_name, "ability vtable return",
                    ret_type_buf, sizeof(ret_type_buf))) {
                free(effective_return_name);
                return false;
            }
            ret_type = ret_type_buf;
        }

        codebuf_write(target, "    %s (*%s)(void *self",
            ret_type, method_name);

        for (size_t j = 0;
             j < transpiler_mir_decl_method_param_count(method_meta); j++) {
            FuncParam *p = transpiler_mir_decl_method_param(method_meta, j);
            const char *param_type_name =
                transpiler_mir_decl_method_param_type_name(method_meta, j);
            char *effective_param_name = NULL;
            char pt_buf[256];
            bool pointer_param = false;
            char surface_desc[256];

            if (p == NULL) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing ability vtable parameter metadata for '%s.%s'",
                    ability_name != NULL ? ability_name : "(anonymous-ability)",
                    method_name != NULL ? method_name : "(anonymous)");
                free(effective_return_name);
                return false;
            }
            if (p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            if (!transpiler_role_ability_surface_desc(surface_desc,
                    sizeof(surface_desc), "ability vtable parameter",
                    ability_name, method_name, p->name)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: ability vtable parameter diagnostic is too long");
                free(effective_return_name);
                return false;
            }
            effective_param_name = ability_vtable_substitute_type_name(
                ctx, param_type_name, bindings, binding_count);
            if (effective_param_name == NULL
                || !transpiler_require_type_name_c_type_copy(ctx,
                    effective_param_name, surface_desc,
                    pt_buf, sizeof(pt_buf))) {
                free(effective_param_name);
                free(effective_return_name);
                return false;
            }
            pointer_param =
                is_pointer_self_host_type_name(ctx, effective_param_name);
            codebuf_write(target, ", %s%s %s", pt_buf,
                          pointer_param ? " *" : "", p->name);
            free(effective_param_name);
        }
        codebuf_write(target, ");\n");
        free(effective_return_name);
    }

    codebuf_write(target, "} %s;\n", typedef_name);
    return true;
}
