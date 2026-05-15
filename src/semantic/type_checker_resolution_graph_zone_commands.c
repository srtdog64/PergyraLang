#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
tc_zone_command_strdup_fmt(const char *fmt, ...)
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

static void
record_zone_layer_dependency(ASTNode *zone_decl,
                             ASTNode *site,
                             SemanticContext *ctx,
                             const char *consumer_label,
                             const char *slot_name,
                             const char *reason)
{
    char *label;
    if (slot_name == NULL)
        return;
    label = semantic_type_resolution_zone_layer_label(zone_decl, slot_name);
    if (label == NULL)
        return;
    semantic_type_resolution_record_local_contract_dependency(
        ctx, site, consumer_label, NULL, label, reason);
    free(label);
}

static void
record_zone_slot_dependency(ASTNode *zone_decl,
                            ASTNode *site,
                            SemanticContext *ctx,
                            const char *consumer_label,
                            const char *slot_name,
                            const char *reason)
{
    char *label;
    if (slot_name == NULL)
        return;
    label = semantic_type_resolution_zone_slot_label(zone_decl, slot_name);
    if (label == NULL)
        return;
    semantic_type_resolution_record_local_contract_dependency(
        ctx, site, consumer_label, NULL, label, reason);
    free(label);
}

static void
record_zone_state_dependency(ASTNode *zone_decl,
                             ASTNode *site,
                             SemanticContext *ctx,
                             const char *consumer_label,
                             const char *state_name,
                             const char *reason)
{
    char *label;
    if (state_name == NULL)
        return;
    label = semantic_type_resolution_zone_state_label(zone_decl, state_name);
    if (label == NULL)
        return;
    semantic_type_resolution_record_local_contract_dependency(
        ctx, site, consumer_label, NULL, label, reason);
    free(label);
}

static const char *
zone_name_or_placeholder(ASTNode *zone_decl)
{
    return zone_decl != NULL && zone_decl->type == AST_ZONE_DECL
        && ast_zone_name(zone_decl) != NULL
        ? ast_zone_name(zone_decl)
        : "<zone>";
}

void
semantic_type_resolution_precollect_zone_command_inventory(
    ASTNode *zone_decl,
    SemanticContext *ctx)
{
    ASTNode **refreshes;
    ASTNode **applies;
    ASTNode **links;
    ASTNode **detaches;
    ASTNode **unlinks;
    ASTNode **maintained_effects;
    ASTNode **maintained_relations;
    size_t refresh_count;
    size_t apply_count;
    size_t link_count;
    size_t detach_count;
    size_t unlink_count;
    size_t maintained_effect_count;
    size_t maintained_relation_count;

    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL || ctx == NULL)
        return;
    refreshes = ast_zone_refreshes(zone_decl, &refresh_count);
    applies = ast_zone_applies(zone_decl, &apply_count);
    links = ast_zone_links(zone_decl, &link_count);
    detaches = ast_zone_detaches(zone_decl, &detach_count);
    unlinks = ast_zone_unlinks(zone_decl, &unlink_count);
    maintained_effects = ast_zone_maintained_effects(
        zone_decl,
        &maintained_effect_count);
    maintained_relations = ast_zone_maintained_relations(
        zone_decl,
        &maintained_relation_count);

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        char *consumer_label;

        if (refresh == NULL || refresh->type != AST_ZONE_REFRESH)
            continue;

        consumer_label = tc_zone_command_strdup_fmt(
            "zone %s.refresh.%s",
            zone_name_or_placeholder(zone_decl),
            ast_zone_refresh_object_slot_name(refresh) != NULL
                ? ast_zone_refresh_object_slot_name(refresh) : "<refresh>");
        if (consumer_label == NULL)
            continue;

        record_zone_slot_dependency(zone_decl, refresh, ctx, consumer_label,
            ast_zone_refresh_object_slot_name(refresh),
            "zone refresh target-slot lookup");
        record_zone_slot_dependency(zone_decl, refresh, ctx, consumer_label,
            ast_zone_refresh_source_slot_name(refresh),
            "zone refresh source-slot lookup");
        semantic_type_resolution_precollect_zone_refresh_projection_map(
            zone_decl, refresh, ctx, consumer_label);
        free(consumer_label);
    }

    for (size_t i = 0; i < apply_count; i++) {
        ASTNode *apply = applies[i];
        char *consumer_label;

        if (apply == NULL || apply->type != AST_ZONE_APPLY)
            continue;

        consumer_label = tc_zone_command_strdup_fmt(
            "zone %s.apply.%s",
            zone_name_or_placeholder(zone_decl),
            ast_zone_effect_slot_name(apply) != NULL
                ? ast_zone_effect_slot_name(apply) : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        record_zone_layer_dependency(zone_decl, apply, ctx, consumer_label,
            ast_zone_effect_slot_name(apply),
            "zone apply effect-slot lookup");
        record_zone_slot_dependency(zone_decl, apply, ctx, consumer_label,
            ast_zone_effect_target_slot_name(apply),
            "zone apply target-slot lookup");
        record_zone_state_dependency(zone_decl, apply, ctx, consumer_label,
            ast_zone_directive_state_name(apply),
            "zone apply state lookup");
        free(consumer_label);
    }

    for (size_t i = 0; i < link_count; i++) {
        ASTNode *link = links[i];
        char *consumer_label;

        if (link == NULL || link->type != AST_ZONE_LINK)
            continue;

        consumer_label = tc_zone_command_strdup_fmt(
            "zone %s.link.%s",
            zone_name_or_placeholder(zone_decl),
            ast_zone_relation_slot_name(link) != NULL
                ? ast_zone_relation_slot_name(link) : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        record_zone_layer_dependency(zone_decl, link, ctx, consumer_label,
            ast_zone_relation_slot_name(link),
            "zone link relation-slot lookup");
        record_zone_slot_dependency(zone_decl, link, ctx, consumer_label,
            ast_zone_relation_left_slot_name(link),
            "zone link left-slot lookup");
        record_zone_slot_dependency(zone_decl, link, ctx, consumer_label,
            ast_zone_relation_right_slot_name(link),
            "zone link right-slot lookup");
        record_zone_state_dependency(zone_decl, link, ctx, consumer_label,
            ast_zone_directive_state_name(link),
            "zone link state lookup");
        free(consumer_label);
    }

    for (size_t i = 0; i < detach_count; i++) {
        ASTNode *detach = detaches[i];
        char *consumer_label;

        if (detach == NULL || detach->type != AST_ZONE_DETACH)
            continue;

        consumer_label = tc_zone_command_strdup_fmt(
            "zone %s.detach.%s",
            zone_name_or_placeholder(zone_decl),
            ast_zone_effect_slot_name(detach) != NULL
                ? ast_zone_effect_slot_name(detach) : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        record_zone_layer_dependency(zone_decl, detach, ctx, consumer_label,
            ast_zone_effect_slot_name(detach),
            "zone detach effect-slot lookup");
        record_zone_slot_dependency(zone_decl, detach, ctx, consumer_label,
            ast_zone_effect_target_slot_name(detach),
            "zone detach target-slot lookup");
        record_zone_state_dependency(zone_decl, detach, ctx, consumer_label,
            ast_zone_directive_state_name(detach),
            "zone detach state lookup");
        free(consumer_label);
    }

    for (size_t i = 0; i < unlink_count; i++) {
        ASTNode *unlink = unlinks[i];
        char *consumer_label;

        if (unlink == NULL || unlink->type != AST_ZONE_UNLINK)
            continue;

        consumer_label = tc_zone_command_strdup_fmt(
            "zone %s.unlink.%s",
            zone_name_or_placeholder(zone_decl),
            ast_zone_relation_slot_name(unlink) != NULL
                ? ast_zone_relation_slot_name(unlink) : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        record_zone_layer_dependency(zone_decl, unlink, ctx, consumer_label,
            ast_zone_relation_slot_name(unlink),
            "zone unlink relation-slot lookup");
        record_zone_slot_dependency(zone_decl, unlink, ctx, consumer_label,
            ast_zone_relation_left_slot_name(unlink),
            "zone unlink left-slot lookup");
        record_zone_slot_dependency(zone_decl, unlink, ctx, consumer_label,
            ast_zone_relation_right_slot_name(unlink),
            "zone unlink right-slot lookup");
        record_zone_state_dependency(zone_decl, unlink, ctx, consumer_label,
            ast_zone_directive_state_name(unlink),
            "zone unlink state lookup");
        free(consumer_label);
    }

    for (size_t i = 0; i < maintained_effect_count; i++) {
        ASTNode *maintain = maintained_effects[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_EFFECT)
            continue;

        consumer_label = tc_zone_command_strdup_fmt(
            "zone %s.maintain-effect.%s",
            zone_name_or_placeholder(zone_decl),
            ast_zone_effect_slot_name(maintain) != NULL
                ? ast_zone_effect_slot_name(maintain) : "<effect-slot>");
        if (consumer_label == NULL)
            continue;

        record_zone_layer_dependency(zone_decl, maintain, ctx, consumer_label,
            ast_zone_effect_slot_name(maintain),
            "zone maintain-effect slot lookup");
        record_zone_slot_dependency(zone_decl, maintain, ctx, consumer_label,
            ast_zone_effect_target_slot_name(maintain),
            "zone maintain-effect target-slot lookup");
        free(consumer_label);
    }

    for (size_t i = 0; i < maintained_relation_count; i++) {
        ASTNode *maintain = maintained_relations[i];
        char *consumer_label;

        if (maintain == NULL || maintain->type != AST_ZONE_MAINTAIN_RELATION)
            continue;

        consumer_label = tc_zone_command_strdup_fmt(
            "zone %s.maintain-relation.%s",
            zone_name_or_placeholder(zone_decl),
            ast_zone_relation_slot_name(maintain) != NULL
                ? ast_zone_relation_slot_name(maintain) : "<relation-slot>");
        if (consumer_label == NULL)
            continue;

        record_zone_layer_dependency(zone_decl, maintain, ctx, consumer_label,
            ast_zone_relation_slot_name(maintain),
            "zone maintain-relation slot lookup");
        record_zone_slot_dependency(zone_decl, maintain, ctx, consumer_label,
            ast_zone_relation_left_slot_name(maintain),
            "zone maintain-relation left-slot lookup");
        record_zone_slot_dependency(zone_decl, maintain, ctx, consumer_label,
            ast_zone_relation_right_slot_name(maintain),
            "zone maintain-relation right-slot lookup");
        free(consumer_label);
    }
}
