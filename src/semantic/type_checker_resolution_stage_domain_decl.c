#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
stage_domain_decl_strdup_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

void
semantic_stage_relation_decl(ASTNode *decl, SemanticContext *ctx)
{
    ASTNode **slots;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t slot_count;
    size_t shared_count;
    size_t method_count;

    if (decl == NULL || decl->type != AST_RELATION_DECL || ctx == NULL)
        return;

    ctx->current_relation = decl;
    slots = ast_relation_slots(decl, &slot_count);
    shared_fields = ast_relation_shared_fields(decl, &shared_count);
    methods = ast_relation_methods(decl, &method_count);
    (void)semantic_stage_resolve_type_quiet(
        decl->data.relation_decl.between_left_type,
        ctx,
        decl,
        ast_relation_name(decl),
        "relation between-left type lookup");
    (void)semantic_stage_resolve_type_quiet(
        decl->data.relation_decl.between_right_type,
        ctx,
        decl,
        ast_relation_name(decl),
        "relation between-right type lookup");
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx,
                ast_domain_slot_type(slot)) == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                ast_domain_slot_type(slot),
                ctx,
                slot,
                ast_domain_slot_name(slot),
                "relation slot type lookup");
        }
    }
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx,
                ast_party_shared_type(field)) == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                ast_party_shared_type(field),
                ctx,
                field,
                ast_party_shared_name(field),
                "relation shared field type lookup");
        }
    }
    semantic_stage_method_array(
        methods,
        method_count,
        ctx,
        ast_relation_name(decl));
}

void
semantic_stage_effect_decl(ASTNode *decl, SemanticContext *ctx)
{
    ASTNode **slots;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t slot_count;
    size_t shared_count;
    size_t method_count;

    if (decl == NULL || decl->type != AST_EFFECT_DECL || ctx == NULL)
        return;

    ctx->current_effect = decl;
    slots = ast_effect_slots(decl, &slot_count);
    shared_fields = ast_effect_shared_fields(decl, &shared_count);
    methods = ast_effect_methods(decl, &method_count);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx,
                ast_domain_slot_type(slot)) == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                ast_domain_slot_type(slot),
                ctx,
                slot,
                ast_domain_slot_name(slot),
                "effect slot type lookup");
        }
    }
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx,
                ast_party_shared_type(field)) == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                ast_party_shared_type(field),
                ctx,
                field,
                ast_party_shared_name(field),
                "effect shared field type lookup");
        }
    }
    semantic_stage_method_array(
        methods,
        method_count,
        ctx,
        ast_effect_name(decl));
}

void
semantic_stage_zone_decl(ASTNode *decl, SemanticContext *ctx)
{
    ASTNode **slots;
    ASTNode **layer_slots;
    ASTNode **shared_fields;
    ASTNode **authorities;
    ASTNode **methods;
    size_t slot_count;
    size_t layer_slot_count;
    size_t shared_count;
    size_t authority_count;
    size_t method_count;

    if (decl == NULL || decl->type != AST_ZONE_DECL || ctx == NULL)
        return;

    ctx->current_zone = decl;
    slots = ast_zone_slots(decl, &slot_count);
    layer_slots = ast_zone_layer_slots(decl, &layer_slot_count);
    shared_fields = ast_zone_shared_fields(decl, &shared_count);
    authorities = ast_zone_authorities(decl, &authority_count);
    methods = ast_zone_methods(decl, &method_count);

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx,
                ast_domain_slot_type(slot)) == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                ast_domain_slot_type(slot),
                ctx,
                slot,
                ast_domain_slot_name(slot),
                "zone slot type lookup");
        }
    }
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *layer = layer_slots[i];
        if (layer == NULL || layer->type != AST_ZONE_LAYER_SLOT)
            continue;
        (void)semantic_stage_named_decl_quiet(
            ctx,
            ast_zone_layer_slot_is_relation(layer)
                ? AST_RELATION_DECL
                : AST_EFFECT_DECL,
            ast_zone_layer_slot_layer_type(layer));
    }
    semantic_stage_zone_local_contracts(decl);
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx,
                ast_party_shared_type(field)) == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                ast_party_shared_type(field),
                ctx,
                field,
                ast_party_shared_name(field),
                "zone shared field type lookup");
        }
    }
    for (size_t i = 0; i < authority_count; i++) {
        ASTNode *authority = authorities[i];
        char *consumer_name;
        if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
            continue;
        consumer_name = stage_domain_decl_strdup_fmt(
            "zone %s.%s",
            ast_zone_name(decl) != NULL
                ? ast_zone_name(decl) : "<zone>",
            authority->data.zone_authority.subject_slot_name != NULL
                ? authority->data.zone_authority.subject_slot_name
                : "<authority>");
        if (consumer_name == NULL)
            continue;
        semantic_stage_required_abilities(
            authority->data.zone_authority.required_abilities,
            authority->data.zone_authority.ability_count,
            ctx,
            authority,
            consumer_name,
            "zone authority ability consumer lookup");
        free(consumer_name);
    }
    semantic_stage_method_array(
        methods,
        method_count,
        ctx,
        ast_zone_name(decl));
}
