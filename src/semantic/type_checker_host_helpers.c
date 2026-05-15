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
    return semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
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
    return host_helper_resolve_type_ref(ast_domain_slot_type(slot), ctx);
}

size_t
overlay_field_count(ASTNode *decl)
{
    if (decl == NULL)
        return 0;
    switch (decl->type) {
    case AST_PARTY_DECL:
        return ast_party_shared_count(decl);
    case AST_ROSTER_DECL:
        return ast_roster_party_count(decl) + ast_roster_shared_count(decl);
    case AST_WORLD_DECL: {
        size_t roster_count;
        size_t zone_count;
        size_t shared_count;
        (void)ast_world_rosters(decl, &roster_count);
        (void)ast_world_zones(decl, &zone_count);
        (void)ast_world_shared_fields(decl, &shared_count);
        return roster_count + zone_count + shared_count;
    }
    case AST_ZONE_DECL: {
        size_t slot_count;
        size_t shared_count;
        (void)ast_zone_slots(decl, &slot_count);
        (void)ast_zone_shared_fields(decl, &shared_count);
        return slot_count + shared_count;
    }
    case AST_RELATION_DECL: {
        size_t slot_count;
        size_t shared_count;
        (void)ast_relation_slots(decl, &slot_count);
        (void)ast_relation_shared_fields(decl, &shared_count);
        return slot_count + shared_count;
    }
    case AST_EFFECT_DECL: {
        size_t slot_count;
        size_t shared_count;
        (void)ast_effect_slots(decl, &slot_count);
        (void)ast_effect_shared_fields(decl, &shared_count);
        return slot_count + shared_count;
    }
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
    case AST_PARTY_DECL: {
        if (index < ast_party_shared_count(decl)) {
            ASTNode *shared = ast_party_shared(decl, index);
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_ROSTER_DECL: {
        size_t party_count = ast_roster_party_count(decl);
        if (index < party_count)
            return NULL;
        index -= party_count;
        if (index < ast_roster_shared_count(decl)) {
            ASTNode *shared = ast_roster_shared(decl, index);
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_WORLD_DECL: {
        size_t roster_count;
        size_t zone_count;
        size_t shared_count;
        ASTNode **shared_fields;
        (void)ast_world_rosters(decl, &roster_count);
        (void)ast_world_zones(decl, &zone_count);
        shared_fields = ast_world_shared_fields(decl, &shared_count);
        if (index < roster_count)
            return NULL;
        index -= roster_count;
        if (index < zone_count)
            return NULL;
        index -= zone_count;
        if (index < shared_count) {
            ASTNode *shared = shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_ZONE_DECL: {
        size_t slot_count;
        size_t shared_count;
        ASTNode **slots = ast_zone_slots(decl, &slot_count);
        ASTNode **shared_fields = ast_zone_shared_fields(decl, &shared_count);
        if (index < slot_count) {
            ASTNode *slot = slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = ast_domain_slot_name(slot);
            return ast_domain_slot_type(slot);
        }
        index -= slot_count;
        if (index < shared_count) {
            ASTNode *shared = shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_RELATION_DECL: {
        size_t slot_count;
        size_t shared_count;
        ASTNode **slots = ast_relation_slots(decl, &slot_count);
        ASTNode **shared_fields = ast_relation_shared_fields(decl, &shared_count);
        if (index < slot_count) {
            ASTNode *slot = slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = ast_domain_slot_name(slot);
            return ast_domain_slot_type(slot);
        }
        index -= slot_count;
        if (index < shared_count) {
            ASTNode *shared = shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
    case AST_EFFECT_DECL: {
        size_t slot_count;
        size_t shared_count;
        ASTNode **slots = ast_effect_slots(decl, &slot_count);
        ASTNode **shared_fields = ast_effect_shared_fields(decl, &shared_count);
        if (index < slot_count) {
            ASTNode *slot = slots[index];
            if (field_name_out != NULL && slot != NULL)
                *field_name_out = ast_domain_slot_name(slot);
            return ast_domain_slot_type(slot);
        }
        index -= slot_count;
        if (index < shared_count) {
            ASTNode *shared = shared_fields[index];
            if (field_name_out != NULL && shared != NULL)
                *field_name_out = ast_party_shared_name(shared);
            return ast_party_shared_type(shared);
        }
        break;
    }
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
    if (ctx->current_party != NULL)
        return ctx->current_party;
    if (ctx->current_roster != NULL)
        return ctx->current_roster;
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
    case SYMBOL_PARTY:
        return find_domain_decl_by_name(program, AST_PARTY_DECL, name);
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

static ASTNode *
semantic_enum_decl_by_name(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt != NULL && stmt->type == AST_ENUM_DECL
            && ast_enum_name(stmt) != NULL
            && strcmp(ast_enum_name(stmt), name) == 0) {
            return stmt;
        }
    }

    return NULL;
}

ASTNode *
semantic_host_decl_for_type(SemanticContext *ctx, const Type *type)
{
    ASTNode *decl;

    if (ctx == NULL || type == NULL || type->name == NULL)
        return NULL;
    if (type->kind == TYPE_KIND_ENUM)
        return semantic_enum_decl_by_name(ctx->program_root, type->name);
    if (type->kind != TYPE_KIND_CLASS)
        return NULL;

    decl = find_type_decl_by_name(ctx->program_root, type->name);
    if (decl != NULL)
        return decl;
    decl = find_domain_decl_by_name(ctx->program_root, AST_PARTY_DECL,
                                    type->name);
    if (decl != NULL)
        return decl;
    decl = find_domain_decl_by_name(ctx->program_root, AST_ROSTER_DECL,
                                    type->name);
    if (decl != NULL)
        return decl;
    decl = find_domain_decl_by_name(ctx->program_root, AST_WORLD_DECL,
                                    type->name);
    if (decl != NULL)
        return decl;
    decl = find_domain_decl_by_name(ctx->program_root, AST_ZONE_DECL,
                                    type->name);
    if (decl != NULL)
        return decl;
    decl = find_domain_decl_by_name(ctx->program_root, AST_RELATION_DECL,
                                    type->name);
    if (decl != NULL)
        return decl;
    return find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
                                    type->name);
}

ASTNode **
semantic_host_decl_methods(ASTNode *decl, size_t *method_count)
{
    if (method_count != NULL)
        *method_count = 0;
    if (decl == NULL)
        return NULL;

    switch (decl->type) {
    case AST_CLASS_DECL:
        return ast_class_methods(decl, method_count);
    case AST_ENUM_DECL:
        return ast_enum_methods(decl, method_count);
    case AST_PARTY_DECL:
        return ast_party_methods(decl, method_count);
    case AST_ROSTER_DECL:
        return ast_roster_methods(decl, method_count);
    case AST_ZONE_DECL:
        return ast_zone_methods(decl, method_count);
    case AST_WORLD_DECL:
        return ast_world_methods(decl, method_count);
    case AST_RELATION_DECL:
        return ast_relation_methods(decl, method_count);
    case AST_EFFECT_DECL:
        return ast_effect_methods(decl, method_count);
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
            && ast_class_name(enclosing_nominal) != NULL
                ? ast_class_name(enclosing_nominal)
                : NULL;
    }

    for (size_t i = 0; i < ast_func_param_count(func); i++) {
        FuncParam *param = ast_func_param(func, i);
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
            || !ast_domain_slot_is_subject(slot)
            || ast_domain_slot_type(slot) == NULL) {
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
    ASTNode **authorities;
    size_t authority_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || ctx == NULL
        || type_name == NULL) {
        return false;
    }

    authorities = ast_zone_authorities(zone, &authority_count);
    for (size_t i = 0; i < authority_count; i++) {
        ASTNode *authority = authorities[i];
        ASTNode *slot;
        Type *slot_type;
        if (authority == NULL
            || authority->data.zone_authority.subject_slot_name == NULL) {
            continue;
        }
        slot = find_zone_domain_slot(zone,
            authority->data.zone_authority.subject_slot_name);
        if (slot == NULL || ast_domain_slot_type(slot) == NULL)
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
    ASTNode **layer_slots;
    size_t layer_slot_count;

    if (zone == NULL || zone->type != AST_ZONE_DECL || effect_name == NULL)
        return false;

    layer_slots = ast_zone_layer_slots(zone, &layer_slot_count);
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || ast_zone_layer_slot_layer_type(slot) == NULL
            || ast_zone_layer_slot_is_relation(slot)) {
            continue;
        }
        if (strcmp(ast_zone_layer_slot_layer_type(slot), effect_name) == 0)
            return true;
    }

    return false;
}

bool
expr_is_class_constructor_call(const ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *callee;
    ASTNode *decl;

    if (expr == NULL || expr->type != AST_CALL || ctx == NULL)
        return false;

    callee = ast_call_callee(expr);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || callee->data.identifier.name == NULL) {
        return false;
    }

    decl = find_type_decl_by_name(ctx->program_root, callee->data.identifier.name);
    if (decl != NULL)
        return !ast_class_is_struct(decl);
    return false;
}

bool
expr_is_qubit_claim(const ASTNode *expr)
{
    ASTNode *callee = (expr != NULL && expr->type == AST_CALL)
        ? ast_call_callee(expr)
        : NULL;

    return expr != NULL
        && expr->type == AST_CALL
        && callee != NULL
        && callee->type == AST_IDENTIFIER
        && callee->data.identifier.name != NULL
        && strcmp(callee->data.identifier.name, "ClaimQubit") == 0;
}

bool
expr_is_device_slot_claim(const ASTNode *expr)
{
    ASTNode *callee = (expr != NULL && expr->type == AST_CALL)
        ? ast_call_callee(expr)
        : NULL;

    return expr != NULL
        && expr->type == AST_CALL
        && callee != NULL
        && callee->type == AST_IDENTIFIER
        && callee->data.identifier.name != NULL
        && strcmp(callee->data.identifier.name, "ClaimDeviceSlot") == 0;
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
