#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

Type *
create_overlay_nominal_type(const char *name)
{
    Type *type = calloc(1, sizeof(Type));
    if (type == NULL)
        return TYPE_UNKNOWN;
    type->kind = TYPE_KIND_CLASS;
    type->nominal_flavor = TYPE_NOMINAL_CLASS;
    type->name = pergyra_strdup(name);
    return type;
}

static Type *
host_helper_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_or_materialize(ctx, type_ref);
}

static Type *
host_helper_resolve_func_param_type(FuncParam *param, SemanticContext *ctx)
{
    if (param == NULL || param->type == NULL)
        return TYPE_UNKNOWN;
    return host_helper_resolve_type_ref(param->type, ctx);
}

static Type *
host_helper_resolve_domain_slot_type(ASTNode *slot, SemanticContext *ctx)
{
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return TYPE_UNKNOWN;
    return host_helper_resolve_type_ref(slot->data.domain_slot.type, ctx);
}

size_t
overlay_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    switch (decl->type) {
    case AST_ROSTER_DECL:
        return decl->data.roster_decl.party_count
            + decl->data.roster_decl.shared_count;
    case AST_WORLD_DECL:
        return decl->data.world_decl.roster_count
            + decl->data.world_decl.zone_count
            + decl->data.world_decl.shared_count;
    case AST_ZONE_DECL:
        return decl->data.zone_decl.slot_count
            + decl->data.zone_decl.shared_count;
    case AST_RELATION_DECL:
        return decl->data.relation_decl.slot_count
            + decl->data.relation_decl.shared_count;
    case AST_EFFECT_DECL:
        return decl->data.effect_decl.slot_count
            + decl->data.effect_decl.shared_count;
    default:
        return 0;
    }
}

ASTNode *
overlay_field_decl_at(ASTNode *decl, size_t index, const char **field_name_out)
{
    if (field_name_out != NULL)
        *field_name_out = NULL;
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_ROSTER_DECL:
        if (index < decl->data.roster_decl.party_count)
            return NULL;
        index -= decl->data.roster_decl.party_count;
        if (index < decl->data.roster_decl.shared_count) {
            ASTNode *shared = decl->data.roster_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_WORLD_DECL:
        if (index < decl->data.world_decl.roster_count)
            return NULL;
        index -= decl->data.world_decl.roster_count;
        if (index < decl->data.world_decl.zone_count)
            return NULL;
        index -= decl->data.world_decl.zone_count;
        if (index < decl->data.world_decl.shared_count) {
            ASTNode *shared = decl->data.world_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_ZONE_DECL:
        if (index < decl->data.zone_decl.slot_count) {
            ASTNode *slot = decl->data.zone_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.zone_decl.slot_count;
        if (index < decl->data.zone_decl.shared_count) {
            ASTNode *shared = decl->data.zone_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_RELATION_DECL:
        if (index < decl->data.relation_decl.slot_count) {
            ASTNode *slot = decl->data.relation_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.relation_decl.slot_count;
        if (index < decl->data.relation_decl.shared_count) {
            ASTNode *shared = decl->data.relation_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    case AST_EFFECT_DECL:
        if (index < decl->data.effect_decl.slot_count) {
            ASTNode *slot = decl->data.effect_decl.slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = slot->data.domain_slot.slot_name;
            return slot != NULL ? slot->data.domain_slot.type : NULL;
        }
        index -= decl->data.effect_decl.slot_count;
        if (index < decl->data.effect_decl.shared_count) {
            ASTNode *shared = decl->data.effect_decl.shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = shared->data.party_shared.name;
            return shared != NULL ? shared->data.party_shared.type : NULL;
        }
        break;
    default:
        break;
    }

    return NULL;
}

ASTNode *
current_host_decl(SemanticContext *ctx)
{
    if (ctx == NULL)
        return NULL;
    if (ctx->current_nominal_decl != NULL)
        return ctx->current_nominal_decl;
    if (ctx->current_relation != NULL)
        return ctx->current_relation;
    if (ctx->current_effect != NULL)
        return ctx->current_effect;
    if (ctx->current_zone != NULL)
        return ctx->current_zone;
    if (ctx->current_world != NULL)
        return ctx->current_world;
    return NULL;
}

ASTNode *
constructor_decl_for_symbol_kind(ASTNode *program, SymbolKind kind, const char *name)
{
    if (program == NULL || name == NULL)
        return NULL;

    switch (kind) {
    case SYMBOL_CLASS:
        return find_type_decl_by_name(program, name);
    case SYMBOL_ROSTER:
        return find_domain_decl_by_name(program, AST_ROSTER_DECL, name);
    case SYMBOL_WORLD:
        return find_domain_decl_by_name(program, AST_WORLD_DECL, name);
    case SYMBOL_ZONE:
        return find_domain_decl_by_name(program, AST_ZONE_DECL, name);
    case SYMBOL_RELATION:
        return find_domain_decl_by_name(program, AST_RELATION_DECL, name);
    case SYMBOL_EFFECT:
        return find_domain_decl_by_name(program, AST_EFFECT_DECL, name);
    default:
        return NULL;
    }
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx);

bool
type_is_subject_host_slot_handle(const Type *type, SemanticContext *ctx)
{
    if (!type_is_owned_slot_handle(type) || ctx == NULL)
        return false;
    return type_is_subject_type(type->data.slot.inner_type, ctx);
}

bool
type_is_class_object_type(const Type *type, SemanticContext *ctx)
{
    return type_is_subject_type(type, ctx);
}

bool
type_is_subject_type(const Type *type, SemanticContext *ctx)
{
    ASTNode *decl;

    if (type == NULL || type->kind != TYPE_KIND_CLASS
        || type->name == NULL || ctx == NULL)
        return false;

    if (type->nominal_flavor == TYPE_NOMINAL_SUBJECT)
        return true;

    decl = find_subject_host_decl_by_name(ctx->program_root, type->name);
    return decl_is_subject_host(decl);
}

const char *
find_action_binding_type_name(ASTNode *func, ASTNode *enclosing_nominal,
                              SemanticContext *ctx, const char *binding_name)
{
    if (func == NULL || func->type != AST_FUNC_DECL || ctx == NULL
        || binding_name == NULL) {
        return NULL;
    }

    if (strcmp(binding_name, "self") == 0) {
        return enclosing_nominal != NULL
            && enclosing_nominal->type == AST_CLASS_DECL
            && enclosing_nominal->data.class_decl.name != NULL
                ? enclosing_nominal->data.class_decl.name
                : NULL;
    }

    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *param = func->data.func_decl.params[i];
        Type *param_type;
        if (param == NULL || param->name == NULL
            || strcmp(param->name, binding_name) != 0) {
            continue;
        }
        if (param->type == NULL)
            return NULL;
        param_type = host_helper_resolve_func_param_type(param, ctx);
        if (param_type == NULL || !type_is_subject_type(param_type, ctx))
            return NULL;
        return param_type->name;
    }

    return NULL;
}

bool
domain_has_subject_slot_type(ASTNode **slots, size_t slot_count,
                             SemanticContext *ctx, const char *type_name)
{
    if (slots == NULL || ctx == NULL || type_name == NULL)
        return false;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        Type *slot_type;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.type == NULL) {
            continue;
        }
        slot_type = host_helper_resolve_domain_slot_type(slot, ctx);
        if (slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, type_name) == 0) {
            return true;
        }
    }

    return false;
}

bool
zone_has_authority_for_subject_type(ASTNode *zone, SemanticContext *ctx,
                                    const char *type_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || ctx == NULL
        || type_name == NULL) {
        return false;
    }

    for (size_t i = 0; i < zone->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone->data.zone_decl.authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(zone,
            authority->data.zone_authority.subject_slot_name);
        if (slot == NULL || slot->data.domain_slot.type == NULL)
            continue;
        slot_type = host_helper_resolve_domain_slot_type(slot, ctx);
        if (slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, type_name) == 0) {
            return true;
        }
    }

    return false;
}

bool
zone_has_effect_layer_type(ASTNode *zone, const char *effect_name)
{
    if (zone == NULL || zone->type != AST_ZONE_DECL || effect_name == NULL)
        return false;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.layer_type == NULL
            || slot->data.zone_layer_slot.is_relation) {
            continue;
        }
        if (strcmp(slot->data.zone_layer_slot.layer_type, effect_name) == 0)
            return true;
    }

    return false;
}

bool
expr_is_class_constructor_call(const ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *decl;

    if (expr == NULL || expr->type != AST_CALL
        || expr->data.call.callee == NULL
        || expr->data.call.callee->type != AST_IDENTIFIER
        || expr->data.call.callee->data.identifier.name == NULL
        || ctx == NULL) {
        return false;
    }

    decl = find_type_decl_by_name(ctx->program_root,
        expr->data.call.callee->data.identifier.name);
    if (decl != NULL)
        return !decl->data.class_decl.is_struct;
    return false;
}

bool
expr_is_qubit_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimQubit") == 0;
}

bool
expr_is_device_slot_claim(const ASTNode *expr)
{
    return expr != NULL
        && expr->type == AST_CALL
        && expr->data.call.callee != NULL
        && expr->data.call.callee->type == AST_IDENTIFIER
        && expr->data.call.callee->data.identifier.name != NULL
        && strcmp(expr->data.call.callee->data.identifier.name, "ClaimDeviceSlot") == 0;
}

bool
expr_is_movable_resource_transfer_source(const ASTNode *expr)
{
    if (expr == NULL)
        return false;

    switch (expr->type) {
    case AST_IDENTIFIER:
    case AST_CALL:
    case AST_CHANNEL_RECV:
    case AST_AWAIT_EXPR:
        return true;
    default:
        return false;
    }
}

bool
slot_transfer_compatible(const Type *from, const Type *to)
{
    if (from == NULL || to == NULL)
        return false;
    if (from->kind != TYPE_KIND_SLOT || to->kind != TYPE_KIND_SLOT)
        return false;
    if (from->data.slot.access_mode != SLOT_ACCESS_MOVE_TOKEN)
        return false;
    if (to->data.slot.access_mode != SLOT_ACCESS_OWNED)
        return false;
    if (from->data.slot.is_secure != to->data.slot.is_secure)
        return false;
    return type_is_assignable(from->data.slot.inner_type, to->data.slot.inner_type)
        && type_is_assignable(to->data.slot.inner_type, from->data.slot.inner_type);
}
