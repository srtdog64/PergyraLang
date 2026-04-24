#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
resolution_label_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
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
semantic_type_resolution_register_local_contract_node(SemanticContext *ctx,
                                                      const ASTNode *site,
                                                      const char *label)
{
    if (ctx == NULL || label == NULL || label[0] == '\0')
        return;

    (void)type_resolution_intern_node(&ctx->type_resolution_graph,
                                      TYPE_RES_NODE_LOCAL_CONTRACT,
                                      site,
                                      label);
}

void
semantic_type_resolution_record_local_contract_dependency(SemanticContext *ctx,
                                                          const ASTNode *consumer_site,
                                                          const char *consumer_label,
                                                          const ASTNode *provider_site,
                                                          const char *provider_label,
                                                          const char *reason)
{
    if (ctx == NULL || provider_label == NULL || provider_label[0] == '\0')
        return;

    semantic_type_resolution_record_named_dependency(
        ctx,
        consumer_site,
        consumer_label,
        TYPE_RES_NODE_LOCAL_CONTRACT,
        provider_site,
        provider_label,
        reason);
}

char *
semantic_type_resolution_world_zone_slot_label(ASTNode *world_decl,
                                               const char *slot_name)
{
    return resolution_label_strdup_fmt(
        "world %s.zone.%s",
        world_decl != NULL
            && world_decl->type == AST_WORLD_DECL
            && world_decl->data.world_decl.name != NULL
                ? world_decl->data.world_decl.name : "<world>",
        slot_name != NULL ? slot_name : "<zone-slot>");
}

char *
semantic_type_resolution_world_state_label(ASTNode *world_decl,
                                           const char *state_name)
{
    return resolution_label_strdup_fmt(
        "world %s.state.%s",
        world_decl != NULL
            && world_decl->type == AST_WORLD_DECL
            && world_decl->data.world_decl.name != NULL
                ? world_decl->data.world_decl.name : "<world>",
        state_name != NULL ? state_name : "<state>");
}

char *
semantic_type_resolution_zone_slot_label(ASTNode *zone_decl,
                                         const char *slot_name)
{
    return resolution_label_strdup_fmt(
        "zone %s.slot.%s",
        zone_decl != NULL
            && zone_decl->type == AST_ZONE_DECL
            && zone_decl->data.zone_decl.name != NULL
                ? zone_decl->data.zone_decl.name : "<zone>",
        slot_name != NULL ? slot_name : "<slot>");
}

char *
semantic_type_resolution_zone_layer_label(ASTNode *zone_decl,
                                          const char *slot_name)
{
    return resolution_label_strdup_fmt(
        "zone %s.layer.%s",
        zone_decl != NULL
            && zone_decl->type == AST_ZONE_DECL
            && zone_decl->data.zone_decl.name != NULL
                ? zone_decl->data.zone_decl.name : "<zone>",
        slot_name != NULL ? slot_name : "<layer>");
}

char *
semantic_type_resolution_zone_state_label(ASTNode *zone_decl,
                                          const char *state_name)
{
    return resolution_label_strdup_fmt(
        "zone %s.state.%s",
        zone_decl != NULL
            && zone_decl->type == AST_ZONE_DECL
            && zone_decl->data.zone_decl.name != NULL
                ? zone_decl->data.zone_decl.name : "<zone>",
        state_name != NULL ? state_name : "<state>");
}

char *
semantic_type_resolution_projection_path_label(ASTNode *zone_decl,
                                               const char *target_slot_name,
                                               const char *source_slot_name,
                                               const char *target_field_name,
                                               const char *source_field_name)
{
    return resolution_label_strdup_fmt(
        "zone %s.projection.%s.%s<-%s.%s",
        zone_decl != NULL
            && zone_decl->type == AST_ZONE_DECL
            && zone_decl->data.zone_decl.name != NULL
                ? zone_decl->data.zone_decl.name : "<zone>",
        target_slot_name != NULL ? target_slot_name : "<target-slot>",
        target_field_name != NULL ? target_field_name : "<target-field>",
        source_slot_name != NULL ? source_slot_name : "<source-slot>",
        source_field_name != NULL ? source_field_name : "<source-field>");
}

char *
semantic_type_resolution_projection_slot_field_label(ASTNode *zone_decl,
                                                     const char *slot_name,
                                                     const char *field_path)
{
    return resolution_label_strdup_fmt(
        "zone %s.slot.%s.field.%s",
        zone_decl != NULL
            && zone_decl->type == AST_ZONE_DECL
            && zone_decl->data.zone_decl.name != NULL
                ? zone_decl->data.zone_decl.name : "<zone>",
        slot_name != NULL ? slot_name : "<slot>",
        field_path != NULL ? field_path : "<field-path>");
}
