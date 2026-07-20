#include "mir_decl_header_world_directive.h"

#include "../parser/ast_api.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_decl_world_directive_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (text == NULL)
        return NULL;
    length = strlen(text);
    copy = malloc(length + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, text, length + 1);
    return copy;
}

static void
mir_decl_world_directive_clear(MIRDeclWorldDirective *meta)
{
    if (meta == NULL)
        return;
    free(meta->zone_slot_name);
    free(meta->state_name);
    meta->zone_slot_name = NULL;
    meta->state_name = NULL;
}

void
mir_decl_header_free_world_directives(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    for (size_t i = 0; i < header->world_directive_metadata_count; i++)
        mir_decl_world_directive_clear(
            &header->world_directive_metadata[i]);
    free(header->world_directive_metadata);
    header->world_directive_metadata = NULL;
    header->world_directive_metadata_count = 0;
    header->world_directive_count = 0;
}

static bool
mir_decl_world_directive_capture(MIRDeclWorldDirective *meta,
                                  const MIRDeclHeader *header,
                                  ASTNode *directive,
                                  MIRDeclWorldDirectiveKind kind)
{
    const char *zone_slot_name;
    const char *state_name;

    if (meta == NULL || header == NULL || directive == NULL)
        return false;
    zone_slot_name = ast_world_directive_zone_slot_name(directive);
    state_name = ast_world_directive_state_name(directive);
    if ((zone_slot_name == NULL) == (state_name == NULL))
        return false;
    meta->owner_name = header->name;
    meta->kind = kind;
    if (zone_slot_name != NULL) {
        meta->zone_slot_name = mir_decl_world_directive_strdup(zone_slot_name);
        if (meta->zone_slot_name == NULL)
            return false;
    }
    if (state_name != NULL) {
        meta->state_name = mir_decl_world_directive_strdup(state_name);
        if (meta->state_name == NULL)
            return false;
    }
    return meta->owner_name != NULL;
}

bool
mir_decl_header_set_world_directives(MIRDeclHeader *header, ASTNode *decl)
{
    ASTNode **activations = NULL;
    ASTNode **maintained_zones = NULL;
    ASTNode **deactivations = NULL;
    size_t activate_count = 0;
    size_t maintain_count = 0;
    size_t deactivate_count = 0;
    size_t total;
    size_t out = 0;

    if (header == NULL)
        return false;
    header->world_directive_count = 0;
    header->world_directive_metadata = NULL;
    header->world_directive_metadata_count = 0;
    if (decl == NULL || decl->type != AST_WORLD_DECL)
        return true;

    activations = ast_world_activations(decl, &activate_count);
    maintained_zones = ast_world_maintained_zones(decl, &maintain_count);
    deactivations = ast_world_deactivations(decl, &deactivate_count);
    if (activate_count > SIZE_MAX - maintain_count
        || activate_count + maintain_count > SIZE_MAX - deactivate_count)
        return false;
    total = activate_count + maintain_count + deactivate_count;
    header->world_directive_count = total;
    if (total == 0)
        return true;
    if (total > SIZE_MAX / sizeof(MIRDeclWorldDirective))
        return false;
    header->world_directive_metadata = calloc(
        total, sizeof(MIRDeclWorldDirective));
    if (header->world_directive_metadata == NULL)
        return false;

    for (size_t i = 0; i < activate_count; i++, out++) {
        ASTNode *directive = activations != NULL ? activations[i] : NULL;
        if (!mir_decl_world_directive_capture(
                &header->world_directive_metadata[out], header, directive,
                MIR_DECL_WORLD_DIRECTIVE_ACTIVATE)) {
            header->world_directive_metadata_count = out + 1;
            mir_decl_header_free_world_directives(header);
            return false;
        }
        header->world_directive_metadata_count = out + 1;
    }
    for (size_t i = 0; i < maintain_count; i++, out++) {
        ASTNode *directive = maintained_zones != NULL
            ? maintained_zones[i] : NULL;
        if (!mir_decl_world_directive_capture(
                &header->world_directive_metadata[out], header, directive,
                MIR_DECL_WORLD_DIRECTIVE_MAINTAIN)) {
            header->world_directive_metadata_count = out + 1;
            mir_decl_header_free_world_directives(header);
            return false;
        }
        header->world_directive_metadata_count = out + 1;
    }
    for (size_t i = 0; i < deactivate_count; i++, out++) {
        ASTNode *directive = deactivations != NULL ? deactivations[i] : NULL;
        if (!mir_decl_world_directive_capture(
                &header->world_directive_metadata[out], header, directive,
                MIR_DECL_WORLD_DIRECTIVE_DEACTIVATE)) {
            header->world_directive_metadata_count = out + 1;
            mir_decl_header_free_world_directives(header);
            return false;
        }
        header->world_directive_metadata_count = out + 1;
    }
    return out == total;
}
