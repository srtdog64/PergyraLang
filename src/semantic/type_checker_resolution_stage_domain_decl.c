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
    if (decl == NULL || decl->type != AST_RELATION_DECL || ctx == NULL)
        return;

    ctx->current_relation = decl;
    (void)semantic_stage_resolve_type_quiet(
        decl->data.relation_decl.between_left_type,
        ctx,
        decl,
        decl->data.relation_decl.name,
        "relation between-left type lookup");
    (void)semantic_stage_resolve_type_quiet(
        decl->data.relation_decl.between_right_type,
        ctx,
        decl,
        decl->data.relation_decl.name,
        "relation between-right type lookup");
    for (size_t i = 0; i < decl->data.relation_decl.slot_count; i++) {
        ASTNode *slot = decl->data.relation_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name,
            "relation slot type lookup");
    }
    for (size_t i = 0; i < decl->data.relation_decl.shared_count; i++) {
        ASTNode *field = decl->data.relation_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name,
            "relation shared field type lookup");
    }
    semantic_stage_method_array(
        decl->data.relation_decl.methods,
        decl->data.relation_decl.method_count,
        ctx,
        decl->data.relation_decl.name);
}

void
semantic_stage_effect_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_EFFECT_DECL || ctx == NULL)
        return;

    ctx->current_effect = decl;
    for (size_t i = 0; i < decl->data.effect_decl.slot_count; i++) {
        ASTNode *slot = decl->data.effect_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name,
            "effect slot type lookup");
    }
    for (size_t i = 0; i < decl->data.effect_decl.shared_count; i++) {
        ASTNode *field = decl->data.effect_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name,
            "effect shared field type lookup");
    }
    semantic_stage_method_array(
        decl->data.effect_decl.methods,
        decl->data.effect_decl.method_count,
        ctx,
        decl->data.effect_decl.name);
}

void
semantic_stage_zone_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_ZONE_DECL || ctx == NULL)
        return;

    ctx->current_zone = decl;
    for (size_t i = 0; i < decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = decl->data.zone_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            slot->data.domain_slot.type,
            ctx,
            slot,
            slot->data.domain_slot.slot_name,
            "zone slot type lookup");
    }
    for (size_t i = 0; i < decl->data.zone_decl.layer_slot_count; i++) {
        ASTNode *layer = decl->data.zone_decl.layer_slots[i];
        if (layer == NULL || layer->type != AST_ZONE_LAYER_SLOT)
            continue;
        (void)semantic_stage_named_decl_quiet(
            ctx,
            layer->data.zone_layer_slot.is_relation
                ? AST_RELATION_DECL
                : AST_EFFECT_DECL,
            layer->data.zone_layer_slot.layer_type);
    }
    semantic_stage_zone_local_contracts(decl);
    for (size_t i = 0; i < decl->data.zone_decl.shared_count; i++) {
        ASTNode *field = decl->data.zone_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name,
            "zone shared field type lookup");
    }
    for (size_t i = 0; i < decl->data.zone_decl.authority_count; i++) {
        ASTNode *authority = decl->data.zone_decl.authorities[i];
        char *consumer_name;
        if (authority == NULL || authority->type != AST_ZONE_AUTHORITY)
            continue;
        consumer_name = stage_domain_decl_strdup_fmt(
            "zone %s.%s",
            decl->data.zone_decl.name != NULL
                ? decl->data.zone_decl.name : "<zone>",
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
        decl->data.zone_decl.methods,
        decl->data.zone_decl.method_count,
        ctx,
        decl->data.zone_decl.name);
}
