#include "transpiler_mir_self_field_slots.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"

/* For a destructured secure slot field, the paired token field is the second
 * name of the `ClaimSecureSlot` group whose first name is the slot. */
static const char *
field_slot_paired_token(const TranspilerHostedFieldView *field_view,
                        ASTNode *host,
                        const char *slot_name)
{
    const MIRDeclHeader *header;
    size_t group_count;

    header = field_view != NULL ? field_view->decl_header : NULL;
    for (size_t i = 0; header != NULL
         && i < mir_decl_header_field_claim_count(header); i++) {
        const MIRDeclFieldClaim *claim = mir_decl_header_field_claim(header, i);
        const char *claim_slot = mir_decl_field_claim_slot_name(claim);
        if (claim_slot != NULL && slot_name != NULL
            && strcmp(claim_slot, slot_name) == 0) {
            return mir_decl_field_claim_token_name(claim);
        }
    }
    if (field_view != NULL && field_view->requires_mir_metadata)
        return NULL;

    group_count = ast_class_field_destructure_count(host);
    for (size_t gi = 0; gi < group_count; gi++)
    {
        ASTNode *group = ast_class_field_destructure_at(host, gi);
        if (group == NULL || ast_let_destructure_name_count(group) < 2)
            continue;
        if (ast_let_destructure_name(group, 0) == NULL
            || strcmp(ast_let_destructure_name(group, 0), slot_name) != 0)
            continue;
        return ast_let_destructure_name(group, 1);
    }
    return NULL;
}

/* Register every `Slot<T>` / `SecureSlot<T>` field of the method's owning class
 * as a `self->field` slot so its Write / Read / Release lower like a local. */
void
transpiler_mir_register_class_field_slots(TranspilerCtx *ctx, ASTNode *host)
{
    TranspilerHostedFieldView fields_view;

    if (ctx == NULL || host == NULL || host->type != AST_CLASS_DECL)
        return;

    /* Route class-field access through the declaration inventory field view
     * instead of reopening the field array directly (declaration
     * source-of-truth). */
    fields_view = transpiler_hosted_class_field_view_from_decl(
        ctx, transpiler_decl_name_local(host), host);
    if (transpiler_hosted_field_view_missing_mir_metadata(&fields_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing class-field slot registration metadata for '%s'",
            transpiler_decl_name_local(host) != NULL
                ? transpiler_decl_name_local(host) : "<class>");
        return;
    }
    for (size_t i = 0; i < fields_view.count; i++)
    {
        const MIRDeclField *field_meta =
            transpiler_hosted_field_view_metadata(&fields_view, i);
        const char *field_name =
            transpiler_hosted_field_view_name(&fields_view, i);
        const char *type_name = field_meta != NULL
            ? transpiler_mir_decl_field_type_name(field_meta) : NULL;
        ASTNode *field_type;
        char *owned_type_name = NULL;
        char inner_buf[128];
        bool is_secure;

        if (field_name == NULL)
            continue;
        if (type_name == NULL) {
            field_type = transpiler_hosted_field_view_type(&fields_view, i);
            if (field_type != NULL) {
                owned_type_name = render_type_name_in_ctx(ctx, field_type);
                type_name = owned_type_name;
            }
        }
        if (!pgy_codegen_type_name_is_slot(type_name)
            && !pgy_codegen_type_name_is_secure_slot(type_name)) {
            free(owned_type_name);
            continue;
        }

        is_secure = pgy_codegen_type_name_is_secure_slot(type_name);
        if (!slot_inner_type_name_copy(type_name, inner_buf,
                sizeof(inner_buf))) {
            pergyra_str_copy(inner_buf, sizeof(inner_buf), "Int");
        }
        register_self_field_slot_var(ctx, field_name,
            inner_buf,
            is_secure,
            is_secure
                ? field_slot_paired_token(&fields_view, host, field_name)
                : NULL);
        free(owned_type_name);
    }
}
