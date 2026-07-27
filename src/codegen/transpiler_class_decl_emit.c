#include "transpiler_class_decl_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compiler/mir.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_generic_method_specialization_emit.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_mir_func_emit.h"
#include "transpiler_slot_runtime_row.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

static bool
transpiler_class_surface_desc(char *out, size_t out_size,
                              const char *surface_kind,
                              const char *class_name,
                              const char *member_name,
                              const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0 || surface_kind == NULL)
        return false;

    if (param_name != NULL) {
        written = snprintf(out, out_size, "%s '%s.%s(%s)'",
            surface_kind,
            class_name != NULL ? class_name : "(anonymous)",
            member_name != NULL ? member_name : "(anonymous)",
            param_name);
    } else {
        written = snprintf(out, out_size, "%s '%s.%s'",
            surface_kind,
            class_name != NULL ? class_name : "(anonymous)",
            member_name != NULL ? member_name : "(anonymous)");
    }

    return written >= 0 && (size_t)written < out_size;
}

static bool
transpiler_class_method_emit_name(char *out, size_t out_size,
                                  const char *class_name,
                                  const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s_%s",
        class_name != NULL ? class_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)");

    return written >= 0 && (size_t)written < out_size;
}

static void
transpiler_class_format_too_long(TranspilerCtx *ctx, const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "class generated name");
}

static void
emit_one_field_slot_claim_meta(TranspilerCtx *ctx,
                               const MIRDeclFieldClaim *claim)
{
    const char *slot = mir_decl_field_claim_slot_name(claim);
    const char *suffix = mir_decl_field_claim_inner_type_name(claim);
    const char *token = mir_decl_field_claim_token_name(claim);
    const char *claim_fn = NULL;

    if (slot == NULL || suffix == NULL)
        return;
    claim_fn = transpiler_slot_runtime_fn_for_decl_claim(ctx, claim);
    if (claim_fn == NULL)
        return;
    if (mir_decl_field_claim_is_secure(claim) && token != NULL) {
        codebuf_write(ctx->out,
            "    self.%s = %s(&self.%s);\n",
            slot, claim_fn, token);
    } else if (!mir_decl_field_claim_is_secure(claim)) {
        codebuf_write(ctx->out,
            "    self.%s = %s();\n", slot, claim_fn);
    }
}

static void
emit_one_field_slot_claim(TranspilerCtx *ctx, ASTNode *group)
{
    ASTNode *init;
    const char *callee;
    const char *slot;
    const char *suffix;
    const char *claim_fn;

    if (group == NULL || ast_let_destructure_name_count(group) < 1)
        return;
    init = ast_let_destructure_initializer(group);
    if (init == NULL || ast_call_callee(init) == NULL)
        return;

    callee = ast_identifier_name(ast_call_callee(init));
    slot = ast_let_destructure_name(group, 0);
    suffix = ast_call_generic_arg_count(init) > 0
        ? ast_generic_param_name(ast_call_generic_arg(init, 0)) : "Int";
    if (callee != NULL && strcmp(callee, "ClaimSecureSlot") == 0
        && ast_let_destructure_name_count(group) >= 2) {
        claim_fn = transpiler_slot_runtime_fn(ctx, true, suffix, "Claim");
        if (claim_fn == NULL)
            return;
        codebuf_write(ctx->out,
            "    self.%s = %s(&self.%s);\n",
            slot, claim_fn, ast_let_destructure_name(group, 1));
    } else if (callee != NULL && strcmp(callee, "ClaimSlot") == 0) {
        claim_fn = transpiler_slot_runtime_fn(ctx, false, suffix, "Claim");
        if (claim_fn == NULL)
            return;
        codebuf_write(ctx->out,
            "    self.%s = %s();\n", slot, claim_fn);
    }
}

/* Emit a constructor helper that claims the class's destructure slot fields so
 * a freshly-built object has live (occupied) secure/plain slots instead of a
 * `{0}` cell that would panic on first Write. */
static void
emit_class_field_slot_initializer(TranspilerCtx *ctx, ASTNode *node,
                                  const char *name)
{
    const MIRDeclHeader *header =
        transpiler_active_decl_header_of_type(ctx, AST_CLASS_DECL, name);
    size_t claim_count = mir_decl_header_field_claim_count(header);

    if (name == NULL)
        return;
    if (transpiler_active_has_mir(ctx)) {
        if (header == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing class field-claim metadata for '%s'",
                name);
            return;
        }
        if (claim_count == 0)
            return;
        codebuf_write(ctx->out,
            "\nstatic %s %s__pgy_field_slot_init(%s self)\n{\n",
            name, name, name);
        for (size_t i = 0; i < claim_count; i++)
            emit_one_field_slot_claim_meta(
                ctx, mir_decl_header_field_claim(header, i));
        codebuf_write(ctx->out, "    return self;\n}\n");
        return;
    }

    {
        size_t group_count = ast_class_field_destructure_count(node);
        if (group_count == 0)
            return;
        codebuf_write(ctx->out,
            "\nstatic %s %s__pgy_field_slot_init(%s self)\n{\n",
            name, name, name);
        for (size_t gi = 0; gi < group_count; gi++)
            emit_one_field_slot_claim(
                ctx, ast_class_field_destructure_at(node, gi));
        codebuf_write(ctx->out, "    return self;\n}\n");
    }
}

static void
emit_class_decl_impl(ASTNode *node,
                     const MIRDeclHeader *mir_header,
                     const char *mir_name,
                     TranspilerCtx *ctx)
{
    const char *name = mir_name;

    if (transpiler_active_has_mir(ctx)) {
        if (mir_header == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing class declaration header");
            return;
        }
        name = mir_decl_header_name(mir_header);
        if (name == NULL || name[0] == '\0') {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing class declaration header name");
            return;
        }
        if (mir_decl_header_generic_param_count(mir_header) > 0)
            return;
    } else {
        if (transpiler_class_has_generic_params(node))
            return;
        name = transpiler_decl_name_local(node);
    }

    if (name == NULL)
        return;
    TranspilerHostedFieldView field_view =
        transpiler_hosted_class_field_view_from_decl(ctx, name, node);
    if (transpiler_hosted_field_view_missing_mir_metadata(&field_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration field metadata for class '%s'",
            name != NULL ? name : "(anonymous-class)");
        return;
    }
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration metadata for class methods '%s'",
            name != NULL ? name : "(anonymous-class)");
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for class '%s'",
            name != NULL ? name : "(anonymous-class)")) {
        return;
    }

    for (size_t i = 0; i < field_view.count; i++) {
        const MIRDeclField *field =
            transpiler_hosted_field_view_metadata(&field_view, i);
        const char *field_type_name =
            transpiler_mir_decl_field_type_name(field);
        if (field_type_name != NULL) {
            ensure_type_specializations_from_type_name_to(
                ctx, ctx->out, field_type_name);
        } else {
            ASTNode *field_type =
                transpiler_hosted_field_view_type(&field_view, i);
            if (field_type != NULL)
                ensure_type_specializations_from_ast_to(
                    ctx, ctx->out, field_type);
        }
    }

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    for (size_t i = 0; i < field_view.count; i++) {
        const char *field_name =
            transpiler_hosted_field_view_name(&field_view, i);
        const MIRDeclField *field =
            transpiler_hosted_field_view_metadata(&field_view, i);
        const char *field_type_name =
            transpiler_mir_decl_field_type_name(field);
        char ft[256];
        char surface_desc[256];
        if (field_name == NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C backend: class '%s' field[%zu] is missing declaration field metadata",
                name != NULL ? name : "(anonymous-class)",
                i);
            return;
        }
        if (!transpiler_class_surface_desc(surface_desc,
                sizeof(surface_desc), "class field", name,
                field_name, NULL)) {
            transpiler_class_format_too_long(ctx, "class field diagnostic surface");
            return;
        }
        if (field_type_name != NULL) {
            if (!transpiler_require_type_name_c_type_copy(ctx,
                    field_type_name,
                    surface_desc,
                    ft,
                    sizeof(ft))) {
                return;
            }
        } else {
            ASTNode *field_type =
                transpiler_hosted_field_view_type(&field_view, i);
            if (field_view.requires_mir_metadata) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing class field type-name metadata for '%s.%s'",
                    name != NULL ? name : "(anonymous-class)",
                    field_name);
                return;
            }
            if (!transpiler_require_ast_c_type_copy(ctx,
                    field_type,
                    surface_desc,
                    ft,
                    sizeof(ft))) {
                return;
            }
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft, field_name);
    }

    if (field_view.count == 0) {
        /* A fieldless class (methods only, no state) would otherwise emit an
         * empty struct -- a GCC extension that rejects the (Type){0}
         * zero-initializer the constructor path emits, breaking the C backend
         * while LLVM compiles fine. A single reserved member keeps the struct
         * standard C and makes {0} a valid initializer. */
        codebuf_write(ctx->out, "    char _pgy_reserved;\n");
    }

    codebuf_write(ctx->out, "} %s;\n", name);
    transpiler_emit_nominal_container_runtime_rows(ctx->out, name, true);

    emit_class_field_slot_initializer(ctx, node, name);

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        const MIRRoutine *mir_method =
            transpiler_mir_decl_method_routine(ctx, method_meta);
        if (method_meta == NULL || mir_method == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing method specialization routine for class '%s'",
                name != NULL ? name : "(anonymous-class)");
            return;
        }
        ensure_collection_specializations_from_mir_routine_to(ctx, ctx->out,
            mir_method);
    }

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for class '%s'",
                name != NULL ? name : "(anonymous-class)");
            return;
        }
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing method body metadata row for class '%s'",
                name != NULL ? name : "(anonymous-class)");
            return;
        }
        if (method_meta == NULL) {
            continue;
        }
        {
            const MIRRoutine *mir_method =
                transpiler_mir_decl_method_routine(ctx, method_meta);
            if (mir_method != NULL
                && transpiler_mir_routine_generic_param_count(mir_method) > 0) {
                if (!transpiler_emit_generic_method_specialization_forwards(
                        ctx, name, method_meta, mir_method, use_self_cell))
                    return;
                continue;
            }
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta, NULL,
            use_self_cell, ctx->out, ctx);
    }

}

bool
transpiler_emit_class_method_bodies_from_inventory(TranspilerCtx *ctx)
{
    MIRDeclHeaderInventory inventory;

    if (ctx == NULL)
        return false;
    if (transpiler_active_has_mir(ctx) == false) {
        transpiler_set_mir_inventory_missing(ctx,
            "C class method body schedule requires active MIR declaration inventory");
        return false;
    }

    transpiler_active_decl_header_inventory(ctx, &inventory);
    for (size_t i = 0; i < inventory.count; i++) {
        const MIRDeclHeader *header =
            mir_decl_header_inventory_get(&inventory, i);
        const char *name;
        TranspilerHostedMethodView method_view;

        if (header == NULL
            || mir_decl_header_ast_type_or(header, AST_PROGRAM)
                != AST_CLASS_DECL) {
            continue;
        }
        name = mir_decl_header_name(header);
        if (name == NULL || name[0] == '\0') {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing class declaration header name during method body schedule");
            return false;
        }
        method_view = transpiler_hosted_method_view_from_decl(
            ctx, name, NULL);
        if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing declaration metadata for class methods '%s'",
                name);
            return false;
        }
        if (!transpiler_require_hosted_method_view_rows(
                ctx,
                &method_view,
                "MIR-only C path has invalid method declaration metadata row for class '%s'",
                name)) {
            return false;
        }

        for (size_t j = 0; j < method_view.count; j++) {
            const MIRDeclMethod *method_meta =
                transpiler_hosted_method_view_metadata(&method_view, j);
            const MIRRoutine *mir_method;
            const char *method_name;
            char emitted_name[256];

            if (method_meta == NULL) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing method body metadata row for class '%s'",
                    name);
                return false;
            }
            method_name = transpiler_mir_decl_method_name(method_meta);
            mir_method = transpiler_mir_decl_method_routine(ctx, method_meta);
            if (method_name == NULL) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing method name metadata for class method '%s.%s'",
                    name, "(anonymous)");
                return false;
            }
            if (mir_method == NULL) {
                transpiler_set_mir_inventory_missing(ctx,
                    "MIR-only C path missing routine for class method '%s.%s'",
                    name, method_name);
                return false;
            }
            if (transpiler_mir_routine_generic_param_count(mir_method) > 0) {
                if (!transpiler_emit_generic_method_specialization_bodies(
                        ctx, mir_method)) {
                    return false;
                }
                continue;
            }
            if (!transpiler_class_method_emit_name(emitted_name,
                    sizeof(emitted_name), name, method_name)) {
                transpiler_class_format_too_long(
                    ctx, "class method emitted name");
                return false;
            }
            emit_func_decl_from_mir_named(NULL, mir_method, emitted_name,
                                          ctx->out, ctx);
            if (ctx->backend_error != NULL)
                return false;
        }
    }
    return true;
}

void
emit_class_decl(ASTNode *node, TranspilerCtx *ctx)
{
    emit_class_decl_impl(node, NULL, NULL, ctx);
}

void
emit_class_decl_from_mir_header(const MIRDeclHeader *header,
                                TranspilerCtx *ctx)
{
    if (header == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx, "MIR-only C path missing class declaration header");
        return;
    }
    emit_class_decl_impl(NULL, header, mir_decl_header_name(header), ctx);
}
