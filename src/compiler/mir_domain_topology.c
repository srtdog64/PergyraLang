#include "mir_domain_topology.h"

#include "dir.h"
#include "mir.h"
#include "mir_decl_headers.h"

#include "../common/string_compat.h"

#include <stdlib.h>
#include <string.h>

static char *
mir_domain_topology_copy(const char *text)
{
    return text != NULL ? pergyra_strdup(text) : NULL;
}

static void
mir_domain_topology_row_clear(MIRDomainTopologyRow *row)
{
    if (row == NULL)
        return;
    free(row->owner_name);
    free(row->projection_slot_name);
    free(row->source_slot_name);
    free(row->layer_slot_name);
    free(row->target_slot_name);
    free(row->left_slot_name);
    free(row->right_slot_name);
    free(row->participant_slot_name);
    memset(row, 0, sizeof(*row));
}

void
mir_domain_topology_clear(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    for (size_t i = 0; i < mir->domain_topology_row_count; i++)
        mir_domain_topology_row_clear(&mir->domain_topology_rows[i]);
    free(mir->domain_topology_rows);
    mir->domain_topology_rows = NULL;
    mir->domain_topology_row_count = 0;
    mir->domain_graph_id = 0;
    mir->has_domain_topology = false;
}

static bool
mir_domain_topology_kind_from_dir(DIRDomainTopologyKind source,
                                  MIRDomainTopologyKind *target)
{
    if (target == NULL)
        return false;
    switch (source) {
    case DIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH:
        *target = MIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH;
        return true;
    case DIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH:
        *target = MIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH;
        return true;
    case DIR_DOMAIN_TOPOLOGY_PROJECTION_BIND:
        *target = MIR_DOMAIN_TOPOLOGY_PROJECTION_BIND;
        return true;
    case DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT:
        *target = MIR_DOMAIN_TOPOLOGY_APPLY_EFFECT;
        return true;
    case DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT:
        *target = MIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT;
        return true;
    case DIR_DOMAIN_TOPOLOGY_LINK_RELATION:
        *target = MIR_DOMAIN_TOPOLOGY_LINK_RELATION;
        return true;
    default:
        return false;
    }
}

static bool
mir_domain_topology_copy_row(MIRDomainTopologyRow *target,
                             const DIRProgram *dir,
                             const DIRDomainTopologyRow *source)
{
    const DIRNode *owner;

    if (target == NULL || dir == NULL || source == NULL
        || source->owner_node_id >= dir->node_count)
        return false;
    owner = &dir->nodes[source->owner_node_id];
    target->owner_source_syntax_id = source->owner_source_syntax_id;
    target->source_syntax_id = source->source_syntax_id;
    if (!mir_domain_topology_kind_from_dir(source->kind, &target->kind))
        return false;
    target->owner_name = mir_domain_topology_copy(owner->name);
    target->projection_slot_name =
        mir_domain_topology_copy(source->projection_slot_name);
    target->projection_slot_source_syntax_id =
        source->projection_slot_source_syntax_id;
    target->source_slot_name = mir_domain_topology_copy(
        source->source_slot_name);
    target->source_slot_source_syntax_id =
        source->source_slot_source_syntax_id;
    target->layer_slot_name = mir_domain_topology_copy(
        source->layer_slot_name);
    target->layer_slot_source_syntax_id =
        source->layer_slot_source_syntax_id;
    target->target_slot_name = mir_domain_topology_copy(
        source->target_slot_name);
    target->target_slot_source_syntax_id =
        source->target_slot_source_syntax_id;
    target->left_slot_name = mir_domain_topology_copy(source->left_slot_name);
    target->left_slot_source_syntax_id = source->left_slot_source_syntax_id;
    target->right_slot_name = mir_domain_topology_copy(
        source->right_slot_name);
    target->right_slot_source_syntax_id = source->right_slot_source_syntax_id;
    target->participant_slot_name = mir_domain_topology_copy(
        source->participant_slot_name);
    target->participant_slot_source_syntax_id =
        source->participant_slot_source_syntax_id;

    return target->owner_name != NULL
        && (source->projection_slot_name == NULL
            || target->projection_slot_name != NULL)
        && (source->source_slot_name == NULL || target->source_slot_name != NULL)
        && (source->layer_slot_name == NULL || target->layer_slot_name != NULL)
        && (source->target_slot_name == NULL || target->target_slot_name != NULL)
        && (source->left_slot_name == NULL || target->left_slot_name != NULL)
        && (source->right_slot_name == NULL || target->right_slot_name != NULL)
        && (source->participant_slot_name == NULL
            || target->participant_slot_name != NULL);
}

bool
mir_domain_topology_project_from_dir(MIRProgram *mir,
                                     const DIRProgram *dir,
                                     char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (mir == NULL || dir == NULL || dir->domain_graph_id == 0) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR domain topology projection requires anchored DIR input");
        return false;
    }
    mir_domain_topology_clear(mir);
    mir->has_domain_topology = true;
    mir->domain_graph_id = dir->domain_graph_id;
    if (dir->domain_topology_row_count == 0)
        return true;
    mir->domain_topology_rows = calloc(
        dir->domain_topology_row_count, sizeof(MIRDomainTopologyRow));
    if (mir->domain_topology_rows == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        mir_domain_topology_clear(mir);
        return false;
    }
    mir->domain_topology_row_count = dir->domain_topology_row_count;
    for (size_t i = 0; i < dir->domain_topology_row_count; i++) {
        if (!mir_domain_topology_copy_row(
                &mir->domain_topology_rows[i], dir,
                &dir->domain_topology_rows[i])) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR could not project DIR domain topology row");
            mir_domain_topology_clear(mir);
            return false;
        }
    }
    return true;
}

const char *
mir_domain_topology_kind_name(MIRDomainTopologyKind kind)
{
    switch (kind) {
    case MIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH: return "refresh";
    case MIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH: return "publish";
    case MIR_DOMAIN_TOPOLOGY_PROJECTION_BIND: return "bind";
    case MIR_DOMAIN_TOPOLOGY_APPLY_EFFECT: return "apply-effect";
    case MIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT: return "maintain-effect";
    case MIR_DOMAIN_TOPOLOGY_LINK_RELATION: return "link-relation";
    default: return "unknown";
    }
}

static bool
mir_domain_topology_is_projection(MIRDomainTopologyKind kind)
{
    return kind == MIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH
        || kind == MIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH
        || kind == MIR_DOMAIN_TOPOLOGY_PROJECTION_BIND;
}

enum {
    MIR_TOPOLOGY_FIELD_SUBJECT = 1u << 0,
    MIR_TOPOLOGY_FIELD_OBJECT = 1u << 1,
    MIR_TOPOLOGY_FIELD_TOBJECT = 1u << 2,
    MIR_TOPOLOGY_FIELD_BINDING = 1u << 3,
    MIR_TOPOLOGY_FIELD_EFFECT = 1u << 4,
    MIR_TOPOLOGY_FIELD_RELATION = 1u << 5
};

static bool
mir_domain_topology_field_kind_matches(const MIRDeclField *field,
                                       unsigned allowed_kinds)
{
    MIRDeclFieldKind kind = mir_decl_field_kind_or(
        field, MIR_DECL_FIELD_UNKNOWN);

    if (kind == MIR_DECL_FIELD_DOMAIN_SLOT) {
        unsigned actual_kind = mir_decl_field_is_subject_like(field)
            ? MIR_TOPOLOGY_FIELD_SUBJECT
            : mir_decl_field_is_tobject_like(field)
                ? MIR_TOPOLOGY_FIELD_TOBJECT
                : mir_decl_field_is_binding_like(field)
                    ? MIR_TOPOLOGY_FIELD_BINDING
                    : MIR_TOPOLOGY_FIELD_OBJECT;
        return (allowed_kinds & actual_kind) != 0;
    }
    if (kind == MIR_DECL_FIELD_ZONE_LAYER_SLOT) {
        unsigned actual_kind = mir_decl_field_is_relation_layer(field)
            ? MIR_TOPOLOGY_FIELD_RELATION : MIR_TOPOLOGY_FIELD_EFFECT;
        return !mir_decl_field_is_pool_layer(field)
            && (allowed_kinds & actual_kind) != 0;
    }
    return false;
}

static bool
mir_domain_topology_field_identity_matches(const MIRDeclHeader *owner,
                                           const char *name,
                                           uint32_t source_id,
                                           unsigned allowed_kinds)
{
    if (owner == NULL || name == NULL || source_id == 0)
        return false;
    for (size_t i = 0; i < mir_decl_header_field_count(owner); i++) {
        const MIRDeclField *field = mir_decl_header_field(owner, i);
        const char *field_name = mir_decl_field_name(field);
        if (field_name != NULL
            && strcmp(field_name, name) == 0
            && mir_decl_field_source_syntax_id(field) == source_id
            && mir_domain_topology_field_kind_matches(
                field, allowed_kinds)) {
            return true;
        }
    }
    return false;
}

static bool
mir_domain_topology_apply_effect_layer_identity_matches(
    const MIRDeclHeader *owner,
    const char *name,
    uint32_t source_id)
{
    if (owner == NULL || name == NULL || source_id == 0)
        return false;
    for (size_t i = 0; i < mir_decl_header_field_count(owner); i++) {
        const MIRDeclField *field = mir_decl_header_field(owner, i);
        const char *field_name = mir_decl_field_name(field);
        if (field_name != NULL
            && strcmp(field_name, name) == 0
            && mir_decl_field_source_syntax_id(field) == source_id
            && mir_decl_field_kind_or(field, MIR_DECL_FIELD_UNKNOWN)
                == MIR_DECL_FIELD_ZONE_LAYER_SLOT
            && !mir_decl_field_is_relation_layer(field)) {
            return true;
        }
    }
    return false;
}

static bool
mir_domain_topology_optional_field_identity_matches(
    const MIRDeclHeader *owner,
    const char *name,
    uint32_t source_id,
    unsigned allowed_kinds)
{
    if (name == NULL || source_id == 0)
        return name == NULL && source_id == 0;
    return mir_domain_topology_field_identity_matches(
        owner, name, source_id, allowed_kinds);
}

bool
mir_domain_topology_validate(const MIRProgram *mir, char **error_message)
{
    if (mir == NULL || !mir->has_domain_topology
        || mir->domain_graph_id == 0
        || (mir->domain_topology_row_count > 0
            && mir->domain_topology_rows == NULL)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "MIR is missing its DIR-owned domain topology projection");
        return false;
    }
    for (size_t i = 0; i < mir->domain_topology_row_count; i++) {
        const MIRDomainTopologyRow *row = &mir->domain_topology_rows[i];
        const MIRDeclHeader *owner_header =
            mir_find_decl_header(mir, row->owner_name);
        bool shape_ok = row->owner_source_syntax_id != 0
            && row->source_syntax_id != 0
            && row->owner_name != NULL
            && owner_header != NULL
            && mir_domain_topology_optional_field_identity_matches(
                owner_header,
                row->participant_slot_name,
                row->participant_slot_source_syntax_id,
                MIR_TOPOLOGY_FIELD_SUBJECT | MIR_TOPOLOGY_FIELD_BINDING);
        if (mir_domain_topology_is_projection(row->kind)) {
            shape_ok = shape_ok
                && (owner_header->ast_type == AST_ZONE_DECL
                    || owner_header->ast_type == AST_RELATION_DECL
                    || owner_header->ast_type == AST_EFFECT_DECL)
                && mir_domain_topology_field_identity_matches(
                    owner_header,
                    row->projection_slot_name,
                    row->projection_slot_source_syntax_id,
                    MIR_TOPOLOGY_FIELD_OBJECT | MIR_TOPOLOGY_FIELD_TOBJECT)
                && mir_domain_topology_field_identity_matches(
                    owner_header,
                    row->source_slot_name,
                    row->source_slot_source_syntax_id,
                    MIR_TOPOLOGY_FIELD_SUBJECT | MIR_TOPOLOGY_FIELD_OBJECT
                        | MIR_TOPOLOGY_FIELD_BINDING)
                && row->layer_slot_name == NULL
                && row->layer_slot_source_syntax_id == 0
                && row->target_slot_name == NULL
                && row->target_slot_source_syntax_id == 0
                && row->left_slot_name == NULL
                && row->left_slot_source_syntax_id == 0
                && row->right_slot_name == NULL
                && row->right_slot_source_syntax_id == 0;
        } else if (row->kind == MIR_DOMAIN_TOPOLOGY_APPLY_EFFECT
                   || row->kind == MIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT) {
            /* Pool admission belongs to the one-shot apply consumer. Keep the
             * generic layer matcher fail-closed so maintain/link cannot regain
             * pool authority through a shared field-kind fallback. */
            bool layer_identity_ok = row->kind == MIR_DOMAIN_TOPOLOGY_APPLY_EFFECT
                ? mir_domain_topology_apply_effect_layer_identity_matches(
                    owner_header,
                    row->layer_slot_name,
                    row->layer_slot_source_syntax_id)
                : mir_domain_topology_field_identity_matches(
                    owner_header,
                    row->layer_slot_name,
                    row->layer_slot_source_syntax_id,
                    MIR_TOPOLOGY_FIELD_EFFECT);
            shape_ok = shape_ok
                && owner_header->ast_type == AST_ZONE_DECL
                && layer_identity_ok
                && mir_domain_topology_field_identity_matches(
                    owner_header,
                    row->target_slot_name,
                    row->target_slot_source_syntax_id,
                    MIR_TOPOLOGY_FIELD_SUBJECT | MIR_TOPOLOGY_FIELD_BINDING)
                && row->projection_slot_name == NULL
                && row->projection_slot_source_syntax_id == 0
                && row->source_slot_name == NULL
                && row->source_slot_source_syntax_id == 0
                && row->left_slot_name == NULL
                && row->left_slot_source_syntax_id == 0
                && row->right_slot_name == NULL
                && row->right_slot_source_syntax_id == 0;
        } else if (row->kind == MIR_DOMAIN_TOPOLOGY_LINK_RELATION) {
            shape_ok = shape_ok
                && owner_header->ast_type == AST_ZONE_DECL
                && mir_domain_topology_field_identity_matches(
                    owner_header,
                    row->layer_slot_name,
                    row->layer_slot_source_syntax_id,
                    MIR_TOPOLOGY_FIELD_RELATION)
                && mir_domain_topology_field_identity_matches(
                    owner_header,
                    row->left_slot_name,
                    row->left_slot_source_syntax_id,
                    MIR_TOPOLOGY_FIELD_SUBJECT | MIR_TOPOLOGY_FIELD_BINDING)
                && mir_domain_topology_field_identity_matches(
                    owner_header,
                    row->right_slot_name,
                    row->right_slot_source_syntax_id,
                    MIR_TOPOLOGY_FIELD_SUBJECT | MIR_TOPOLOGY_FIELD_BINDING)
                && row->projection_slot_name == NULL
                && row->projection_slot_source_syntax_id == 0
                && row->source_slot_name == NULL
                && row->source_slot_source_syntax_id == 0
                && row->target_slot_name == NULL
                && row->target_slot_source_syntax_id == 0;
        } else {
            shape_ok = false;
        }
        if (!shape_ok) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "MIR domain topology row has incomplete identity or shape");
            return false;
        }
        for (size_t j = i + 1; j < mir->domain_topology_row_count; j++) {
            if (mir->domain_topology_rows[j].source_syntax_id
                == row->source_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "MIR domain topology rows duplicate source identity");
                return false;
            }
            if ((mir->domain_topology_rows[j].owner_source_syntax_id
                    == row->owner_source_syntax_id)
                != (strcmp(mir->domain_topology_rows[j].owner_name,
                           row->owner_name) == 0)) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "MIR domain topology owner name and stable identity disagree");
                return false;
            }
        }
    }
    return true;
}
