#include "transpiler_constructor_channel_guard.h"

#include <stdlib.h>

#include "../semantic/diag_codes.h"
#include "host_decl_compat.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

bool
transpiler_constructor_field_is_channel(TranspilerCtx *ctx,
                                        ASTNode *field_type)
{
    char *type_name;
    bool is_channel;

    if (field_type == NULL)
        return false;
    if (field_type->type == AST_CHANNEL_TYPE)
        return true;
    type_name = render_type_name_in_ctx(ctx, field_type);
    is_channel = transpiler_type_name_is_channel(type_name);
    free(type_name);
    return is_channel;
}

bool
transpiler_constructor_reject_channel_field(TranspilerCtx *ctx,
                                            const char *field_name)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_PROVIDE_MOVABLE_HANDLE,
        "C backend: Channel field '%s' cannot be aggregate-constructed or default-initialized until movable channel-handle lowering is available",
        field_name != NULL ? field_name : "<field>");
    return false;
}

static const char *
transpiler_constructor_find_shared_channel(TranspilerCtx *ctx,
                                           ASTNode **fields,
                                           size_t count)
{
    for (size_t i = 0; fields != NULL && i < count; i++) {
        ASTNode *field = fields[i];
        if (field != NULL
            && transpiler_constructor_field_is_channel(
                ctx, ast_party_shared_type(field))) {
            return ast_party_shared_name(field);
        }
    }
    return NULL;
}

static const char *
transpiler_constructor_find_slot_channel(TranspilerCtx *ctx,
                                         ASTNode **slots,
                                         size_t count)
{
    for (size_t i = 0; slots != NULL && i < count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL
            && transpiler_constructor_field_is_channel(
                ctx, ast_domain_slot_type(slot))) {
            return ast_domain_slot_name(slot);
        }
    }
    return NULL;
}

const char *
transpiler_constructor_find_channel_field(TranspilerCtx *ctx, ASTNode *decl)
{
    PgyHostSharedFieldsCompatView shared;
    size_t count = 0;
    ASTNode **nodes = NULL;

    if (ctx == NULL || decl == NULL)
        return NULL;

    if (decl->type == AST_CLASS_DECL) {
        PgyHostClassFieldsCompatView class_fields =
            pgy_host_class_fields_compat_view_from_decl(decl);
        for (size_t i = 0;
             class_fields.fields != NULL && i < class_fields.count; i++) {
            ClassField *field = class_fields.fields[i];
            if (field != NULL
                && transpiler_constructor_field_is_channel(
                    ctx, field->type)) {
                return field->name;
            }
        }
        return NULL;
    }

    switch (decl->type) {
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
        shared = pgy_host_shared_fields_compat_view_from_decl(decl);
        return transpiler_constructor_find_shared_channel(
            ctx, shared.fields, shared.count);
    case AST_RELATION_DECL:
        nodes = ast_relation_slots(decl, &count);
        {
            const char *slot =
                transpiler_constructor_find_slot_channel(ctx, nodes, count);
            if (slot != NULL)
                return slot;
        }
        shared = pgy_host_shared_fields_compat_view_from_decl(decl);
        return transpiler_constructor_find_shared_channel(
            ctx, shared.fields, shared.count);
    case AST_EFFECT_DECL:
        nodes = ast_effect_slots(decl, &count);
        {
            const char *slot =
                transpiler_constructor_find_slot_channel(ctx, nodes, count);
            if (slot != NULL)
                return slot;
        }
        shared = pgy_host_shared_fields_compat_view_from_decl(decl);
        return transpiler_constructor_find_shared_channel(
            ctx, shared.fields, shared.count);
    case AST_ZONE_DECL:
        nodes = ast_zone_slots(decl, &count);
        {
            const char *slot =
                transpiler_constructor_find_slot_channel(ctx, nodes, count);
            if (slot != NULL)
                return slot;
        }
        shared = pgy_host_shared_fields_compat_view_from_decl(decl);
        return transpiler_constructor_find_shared_channel(
            ctx, shared.fields, shared.count);
    case AST_WORLD_DECL:
        shared = pgy_host_shared_fields_compat_view_from_decl(decl);
        return transpiler_constructor_find_shared_channel(
            ctx, shared.fields, shared.count);
    default:
        return NULL;
    }
}
