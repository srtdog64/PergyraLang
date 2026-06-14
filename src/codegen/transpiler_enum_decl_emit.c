#include "transpiler_enum_decl_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "../compiler/mir.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "host_decl_compat.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum_method_names.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_mir_func_emit.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

void
emit_enum_decl_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *ename = transpiler_decl_name_local(node);
    size_t variant_count = 0;
    char **variants = NULL;
    const MIRDeclHeader *enum_header = NULL;
    bool use_mir_variants = false;
    if (ename == NULL)
        return;
    enum_header = transpiler_active_decl_header_of_type(
        ctx, AST_ENUM_DECL, ename);
    if (transpiler_active_has_mir(ctx) && enum_header == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing enum variant metadata for '%s'",
            ename);
        return;
    }
    use_mir_variants = enum_header != NULL;
    if (use_mir_variants) {
        variant_count = mir_decl_header_enum_variant_count(enum_header);
    } else {
        variants = ast_enum_variants(node, &variant_count);
    }
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, ename, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration metadata for enum methods '%s'",
            ename != NULL ? ename : "(anonymous-enum)");
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for enum '%s'",
            ename != NULL ? ename : "(anonymous-enum)")) {
        return;
    }

    bool has_data = false;
    for (size_t i = 0; i < variant_count; i++) {
        const MIRDeclEnumVariant *variant_meta = use_mir_variants
            ? mir_decl_header_enum_variant(enum_header, i) : NULL;
        size_t param_count = use_mir_variants
            ? mir_decl_enum_variant_param_count(variant_meta)
            : ast_enum_variant_param_count(node, i);
        if (param_count > 0) {
            has_data = true;
            break;
        }
    }

    if (!has_data) {
        codebuf_write(ctx->out, "typedef enum {\n");
        for (size_t i = 0; i < variant_count; i++) {
            const MIRDeclEnumVariant *variant_meta = use_mir_variants
                ? mir_decl_header_enum_variant(enum_header, i) : NULL;
            const char *vname = use_mir_variants
                ? mir_decl_enum_variant_name(variant_meta)
                : (variants != NULL ? variants[i] : NULL);
            if (vname == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path has invalid enum variant metadata row for '%s'",
                    ename);
                return;
            }
            codebuf_write(ctx->out, "    %s_%s = %zu",
                ename, vname, i);
            if (i + 1 < variant_count)
                codebuf_write(ctx->out, ",");
            codebuf_write(ctx->out, "\n");
        }
        codebuf_write(ctx->out, "} %s;\n\n", ename);
    } else {
        codebuf_write(ctx->out, "typedef enum {\n");
        for (size_t i = 0; i < variant_count; i++) {
            const MIRDeclEnumVariant *variant_meta = use_mir_variants
                ? mir_decl_header_enum_variant(enum_header, i) : NULL;
            const char *vname = use_mir_variants
                ? mir_decl_enum_variant_name(variant_meta)
                : (variants != NULL ? variants[i] : NULL);
            if (vname == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path has invalid enum variant metadata row for '%s'",
                    ename);
                return;
            }
            codebuf_write(ctx->out, "    %s_TAG_%s = %zu",
                ename, vname, i);
            if (i + 1 < variant_count)
                codebuf_write(ctx->out, ",");
            codebuf_write(ctx->out, "\n");
        }
        codebuf_write(ctx->out, "} %s_Tag;\n\n", ename);

        codebuf_write(ctx->out, "typedef struct {\n");
        codebuf_write(ctx->out, "    %s_Tag tag;\n", ename);
        codebuf_write(ctx->out, "    union {\n");
        for (size_t i = 0; i < variant_count; i++) {
            const MIRDeclEnumVariant *v = use_mir_variants
                ? mir_decl_header_enum_variant(enum_header, i) : NULL;
            const char *vname = use_mir_variants
                ? mir_decl_enum_variant_name(v)
                : (variants != NULL ? variants[i] : NULL);
            size_t pc = use_mir_variants
                ? mir_decl_enum_variant_param_count(v)
                : ast_enum_variant_param_count(node, i);
            if (pc == 0)
                continue;
            if (vname == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path has invalid enum variant metadata row for '%s'",
                    ename);
                return;
            }
            codebuf_write(ctx->out, "        struct { ");
            for (size_t p = 0; p < pc; p++) {
                char ctype[256];
                if (use_mir_variants) {
                    const char *ptn =
                        mir_decl_enum_variant_param_type_name(v, p);
                    if (!transpiler_require_type_name_c_type_copy(ctx, ptn,
                            "enum variant payload field",
                            ctype, sizeof(ctype))) {
                        return;
                    }
                } else {
                    ASTNode *pt = ast_enum_variant_param(node, i, p);
                    if (!transpiler_require_ast_c_type_copy(ctx, pt,
                            "enum variant payload field",
                            ctype, sizeof(ctype))) {
                        return;
                    }
                }
                codebuf_write(ctx->out, "%s _%zu; ", ctype, p);
            }
            codebuf_write(ctx->out, "} %s;\n", vname);
        }
        codebuf_write(ctx->out, "    };\n");
        codebuf_write(ctx->out, "} %s;\n\n", ename);

        for (size_t i = 0; i < variant_count; i++) {
            const MIRDeclEnumVariant *v = use_mir_variants
                ? mir_decl_header_enum_variant(enum_header, i) : NULL;
            size_t pc = use_mir_variants
                ? mir_decl_enum_variant_param_count(v)
                : ast_enum_variant_param_count(node, i);
            const char *vname = use_mir_variants
                ? mir_decl_enum_variant_name(v)
                : (variants != NULL ? variants[i] : NULL);
            if (vname == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path has invalid enum variant metadata row for '%s'",
                    ename);
                return;
            }
            if (pc == 0) {
                /* Payload-less variant of a tagged union is a constant value,
                 * referenced bare as `Enum.Variant` (no call). */
                codebuf_write(ctx->out,
                    "#define %s_%s ((%s){ .tag = %s_TAG_%s })\n",
                    ename, vname, ename, ename, vname);
            } else {
                codebuf_write(ctx->out,
                    "static inline %s %s_%s(", ename, ename, vname);
                for (size_t p = 0; p < pc; p++) {
                    char ctype[256];
                    if (use_mir_variants) {
                        const char *ptn =
                            mir_decl_enum_variant_param_type_name(v, p);
                        if (!transpiler_require_type_name_c_type_copy(ctx, ptn,
                                "enum variant constructor parameter",
                                ctype, sizeof(ctype))) {
                            return;
                        }
                    } else {
                        ASTNode *pt = ast_enum_variant_param(node, i, p);
                        if (!transpiler_require_ast_c_type_copy(ctx, pt,
                                "enum variant constructor parameter",
                                ctype, sizeof(ctype))) {
                            return;
                        }
                    }
                    if (p > 0)
                        codebuf_write(ctx->out, ", ");
                    codebuf_write(ctx->out, "%s _%zu", ctype, p);
                }
                codebuf_write(ctx->out, ") {\n");
                codebuf_write(ctx->out,
                    "    %s _v; _v.tag = %s_TAG_%s;\n", ename, ename, vname);
                for (size_t p = 0; p < pc; p++)
                    codebuf_write(ctx->out,
                        "    _v.%s._%zu = _%zu;\n", vname, p, p);
                codebuf_write(ctx->out, "    return _v;\n}\n");
            }
        }
        codebuf_write(ctx->out, "\n");
    }

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for enum '%s'",
                ename != NULL ? ename : "(anonymous-enum)");
            return;
        }
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing method body metadata row for enum '%s'",
                ename != NULL ? ename : "(anonymous-enum)");
            return;
        }
        if (method_meta == NULL) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(ename, method_meta,
            NULL, false, ctx->out, ctx);
    }

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method = NULL;
        const MIRRoutine *mir_method;
        const char *method_name;
        method_name = transpiler_mir_decl_method_name(method_meta);
        mir_method = transpiler_mir_decl_method_routine(ctx, method_meta);
        if (method == NULL && mir_method != NULL)
            method = transpiler_mir_decl_method_body_decl(ctx, method_meta);
        if (method_name == NULL && method != NULL)
            method_name = ast_declaration_name(method);
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }
        if (transpiler_active_has_mir(ctx) && mir_method == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing routine for enum method '%s.%s'",
                ename != NULL ? ename : "(anonymous-enum)",
                method_name != NULL ? method_name : "(anonymous)");
            return;
        }
        if (mir_method != NULL) {
            char emitted_name[256];
            if (!transpiler_enum_method_emit_name(emitted_name,
                    sizeof(emitted_name), ename, method_name)) {
                transpiler_enum_format_too_long(
                    ctx, "enum method emitted name");
                return;
            }
            emit_func_decl_from_mir_named(method, mir_method, emitted_name,
                                          ctx->out, ctx);
            continue;
        }

        char ret_type_buf[256];
        const char *ret_type = "void";
        if (ast_func_return_type(method) != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx, ast_func_return_type(method),
                ret_type_buf, sizeof(ret_type_buf))) {
            ret_type = ret_type_buf;
        }

        codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                      ret_type, ename, method_name, ename);
        for (size_t j = 0; j < ast_func_param_count(method); j++) {
            FuncParam *p = ast_func_param(method, j);
            if (p == NULL || p->name == NULL || strcmp(p->name, "self") == 0)
                continue;
            char pt[256];
            char surface_desc[256];
            if (!transpiler_enum_method_surface_desc(surface_desc,
                    sizeof(surface_desc), ename, method_name,
                    p != NULL ? p->name : NULL)) {
                transpiler_enum_format_too_long(
                    ctx, "enum method parameter diagnostic surface");
                return;
            }
            if (!transpiler_require_ast_c_type_copy(ctx,
                    p != NULL ? p->type : NULL, surface_desc, pt, sizeof(pt))) {
                return;
            }
            codebuf_write(ctx->out, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->out, ")\n{\n");
        transpiler_emit_host_method_body_local(
            ctx,
            transpiler_find_named_decl_local(ctx, AST_ENUM_DECL, ename),
            ename,
            method,
            NULL,
            false);
        codebuf_write(ctx->out, "}\n");
    }
}
