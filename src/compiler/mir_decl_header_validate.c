#include "mir_fact_validate.h"
#include "mir_fact_validate_internal.h"
#include "mir_decl_header_role_validate.h"
#include "mir_decl_field_claim_abi.h"
#include "mir_decl_header_shape_validate.h"
#include "mir_decl_header_world_directive_validate.h"
#include "mir_decl_header_world_state_validate.h"
#include "mir_decl_header_zone_state_validate.h"
#include "mir_decl_headers.h"
#include "mir_type_helpers.h"
#include "../semantic/callable_contract_vocabulary.h"

#include <stdlib.h>
#include <string.h>

#define mir_strdup_fmt mir_fact_strdup_fmt

static char *
mir_decl_field_expected_type_name(const MIRDeclField *field)
{
    if (field == NULL)
        return NULL;
    if (field->type != NULL)
        return mir_capture_type_name(field->type, NULL);
    return field->type_name != NULL ? mir_strdup_fmt("%s", field->type_name) : NULL;
}

static bool
mir_validate_decl_method_metadata(const MIRProgram *mir,
                                  const MIRDeclHeader *header,
                                  size_t header_index,
                                  char **error_message)
{
    MIRRoutineInventory inventory;
    if (mir == NULL || header == NULL)
        return false;

    if (!mir_validate_decl_header_shape_metadata(header, header_index, error_message))
        return false;

    if (header->method_metadata_count > 0 && header->method_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu method metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->method_metadata_count);
        }
        return false;
    }

    if (header->method_count > 0 && header->method_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu hosted method(s) without MIRDeclMethod metadata",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->method_count);
        }
        return false;
    }

    if (header->method_metadata_count != header->method_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' method metadata count %zu does not match declaration method count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->method_metadata_count,
                header->method_count);
        }
        return false;
    }

    if (header->field_metadata_count > 0 && header->field_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu field metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->field_metadata_count);
        }
        return false;
    }

    if (header->field_count > 0 && header->field_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu hosted field(s) without MIRDeclField metadata",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->field_count);
        }
        return false;
    }

    if (header->field_metadata_count != header->field_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' field metadata count %zu does not match declaration field count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->field_metadata_count,
                header->field_count);
        }
        return false;
    }

    if (!mir_decl_header_field_claim_abi_validate(
            header, header_index, error_message)) {
        return false;
    }

    if (header->ast_type != AST_EVENT_DECL
        && (header->event_param_metadata_present
            || header->event_param_count != 0
            || header->event_param_names != NULL
            || header->event_param_type_names != NULL)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has event parameter metadata on a non-event declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->ast_type == AST_EVENT_DECL
        && !header->event_param_metadata_present) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' is missing event parameter metadata",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->event_param_count > 0
        && (header->event_param_names == NULL
            || header->event_param_type_names == NULL)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' event parameter metadata is incomplete",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    for (size_t i = 0; i < header->event_param_count; i++) {
        if (header->event_param_names[i] == NULL
            || header->event_param_type_names[i] == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] '%s' event parameter[%zu] has no ABI metadata",
                    header_index,
                    header->name != NULL ? header->name : "(anonymous)",
                    i);
            }
            return false;
        }
    }

    if (header->generic_metadata_count > 0
        && header->generic_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu generic metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->generic_metadata_count);
        }
        return false;
    }

    if (header->generic_metadata_count != header->generic_param_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' generic metadata count %zu does not match declaration generic count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->generic_metadata_count,
                header->generic_param_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->generic_metadata_count; i++) {
        const MIRDeclGenericParam *generic = &header->generic_metadata[i];
        if (generic->name == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] generic[%zu] has incomplete metadata",
                    header_index, i);
            }
            return false;
        }
    }

    if (header->variant_metadata_count > 0
        && header->variant_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu enum variant metadata row(s) but no storage",
                header_index,
                header->name,
                header->variant_metadata_count);
        }
        return false;
    }

    if (header->variant_metadata_count != header->variant_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] enum variant metadata count %zu does not match declaration variant count %zu",
                header_index,
                header->variant_metadata_count,
                header->variant_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->variant_metadata_count; i++) {
        const MIRDeclEnumVariant *variant =
            &header->variant_metadata[i];
        if (variant->name == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] enum variant[%zu] has incomplete metadata",
                    header_index, i);
            }
            return false;
        }
        if (variant->param_count > 0 && variant->param_type_names == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] enum variant[%zu] has payload metadata count but no storage",
                    header_index, i);
            }
            return false;
        }
        for (size_t p = 0; p < variant->param_count; p++) {
            if (variant->param_type_names[p] == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] enum variant[%zu] payload[%zu] has no type metadata",
                        header_index, i, p);
                }
                return false;
            }
        }
    }

    for (size_t i = 0; i < header->field_metadata_count; i++) {
        const MIRDeclField *field = &header->field_metadata[i];
        if (field->owner_name == NULL
            || header->name == NULL
            || strcmp(field->owner_name, header->name) != 0) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] field[%zu] has owner metadata drift",
                    header_index, i);
            }
            return false;
        }
        if (field->kind == MIR_DECL_FIELD_UNKNOWN || field->name == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] field[%zu] has incomplete field metadata",
                    header_index, i);
            }
            return false;
        }
        if (field->source_syntax_id == 0) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] field[%zu] has no source syntax identity",
                    header_index, i);
            }
            return false;
        }
        for (size_t h = 0; h <= header_index; h++) {
            const MIRDeclHeader *other_header = &mir->decl_headers[h];
            size_t field_limit = h == header_index
                ? i
                : other_header->field_metadata_count;
            for (size_t f = 0; f < field_limit; f++) {
                if (other_header->field_metadata[f].source_syntax_id
                    == field->source_syntax_id) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR declaration header[%zu] field[%zu] duplicates source syntax identity %u",
                            header_index, i, field->source_syntax_id);
                    }
                    return false;
                }
            }
        }
        {
            char *expected_type_name =
                mir_decl_field_expected_type_name(field);
            bool matches = expected_type_name == NULL
                ? field->type_name == NULL
                : field->type_name != NULL
                    && strcmp(field->type_name, expected_type_name) == 0;
            free(expected_type_name);
            if (!matches) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] field[%zu] type metadata drift",
                        header_index, i);
                }
                return false;
            }
        }
    }

    if (header->zone_authority_metadata_count > 0
        && header->zone_authority_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu zone authority metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->zone_authority_metadata_count);
        }
        return false;
    }

    if (header->ast_type != AST_ZONE_DECL
        && header->zone_authority_metadata_count != 0) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has zone authority metadata on a non-zone declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }

    if (header->zone_authority_metadata_count
        != header->zone_authority_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' zone authority metadata count %zu does not match declaration authority count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->zone_authority_metadata_count,
                header->zone_authority_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->zone_authority_metadata_count; i++) {
        const MIRDeclZoneAuthority *authority =
            &header->zone_authority_metadata[i];
        if (authority->owner_name == NULL
            || header->name == NULL
            || strcmp(authority->owner_name, header->name) != 0
            || authority->subject_slot_name == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] zone authority[%zu] has incomplete authority metadata",
                    header_index, i);
            }
            return false;
        }
        if (authority->required_ability_ref_count > 0
            && authority->required_ability_refs == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] zone authority[%zu] has ability metadata count but no storage",
                    header_index, i);
            }
            return false;
        }
        for (size_t a = 0; a < authority->required_ability_ref_count; a++) {
            const MIRAbilityRef *ref =
                &authority->required_ability_refs[a];
            if (ref->base_name == NULL
                || (ref->actual_arg_count > 0
                    && ref->actual_arg_type_names == NULL)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] zone authority[%zu] ability[%zu] has incomplete ability-ref metadata",
                        header_index, i, a);
                }
                return false;
            }
            for (size_t arg = 0; arg < ref->actual_arg_count; arg++) {
                if (ref->actual_arg_type_names[arg] == NULL) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR declaration header[%zu] zone authority[%zu] ability[%zu] actual[%zu] has no type metadata",
                            header_index, i, a, arg);
                    }
                    return false;
                }
            }
        }
    }

    if (header->zone_refresh_metadata_count > 0
        && header->zone_refresh_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu zone refresh metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->zone_refresh_metadata_count);
        }
        return false;
    }

    if (header->ast_type != AST_RELATION_DECL
        && header->ast_type != AST_EFFECT_DECL
        && header->ast_type != AST_ZONE_DECL
        && header->zone_refresh_metadata_count != 0) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has domain refresh metadata on a non-domain declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }

    if (header->zone_refresh_metadata_count != header->zone_refresh_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' zone refresh metadata count %zu does not match declaration refresh count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->zone_refresh_metadata_count,
                header->zone_refresh_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->zone_refresh_metadata_count; i++) {
        const MIRDeclZoneRefresh *refresh =
            &header->zone_refresh_metadata[i];
        if (refresh->owner_name == NULL
            || header->name == NULL
            || strcmp(refresh->owner_name, header->name) != 0
            || refresh->object_slot_name == NULL
            || refresh->source_slot_name == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] zone refresh[%zu] has incomplete refresh metadata",
                    header_index, i);
            }
            return false;
        }
        if (refresh->field_map_count > 0 && refresh->field_maps == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] zone refresh[%zu] has field-map metadata count but no storage",
                    header_index, i);
            }
            return false;
        }
        for (size_t map_i = 0; map_i < refresh->field_map_count; map_i++) {
            const MIRDeclZoneRefreshFieldMap *map =
                &refresh->field_maps[map_i];
            if (map->target_field_name == NULL
                || map->source_field_name == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] zone refresh[%zu] field-map[%zu] has incomplete metadata",
                        header_index, i, map_i);
                }
                return false;
            }
        }
    }

    if (!mir_decl_header_validate_zone_states(
            header, header_index, error_message)) {
        return false;
    }
    if (!mir_decl_header_validate_world_states(
            header, header_index, error_message)) {
        return false;
    }
    if (!mir_decl_header_validate_world_directives(
            header, header_index, error_message)) {
        return false;
    }

    if (!mir_validate_decl_role_impl_metadata(
            header, header_index, error_message)) {
        return false;
    }
    if (!mir_validate_decl_role_include_metadata(
            header, header_index, error_message)) {
        return false;
    }

    mir_routine_inventory_from_program(mir, &inventory);

    for (size_t i = 0; i < header->method_metadata_count; i++) {
        const MIRDeclMethod *method = &header->method_metadata[i];

        if (method->owner_name == NULL
            || header->name == NULL
            || strcmp(method->owner_name, header->name) != 0) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has owner metadata drift",
                    header_index, i);
            }
            return false;
        }

        if (method->has_routine && method->routine_index >= inventory.count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] routine index %zu exceeds routine count %zu",
                    header_index, i, method->routine_index, inventory.count);
            }
            return false;
        }

        if (method->has_routine) {
            const MIRRoutine *routine =
                mir_routine_inventory_get(&inventory, method->routine_index);
            if (routine == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] method[%zu] routine link metadata drift",
                        header_index, i);
                }
                return false;
            }
            if (routine->kind != MIR_SCOPE_METHOD
                || method->name == NULL
                || routine->name == NULL
                || strcmp(method->name, routine->name) != 0
                || method->owner_name == NULL
                || routine->owner_name == NULL
                || strcmp(method->owner_name, routine->owner_name) != 0) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] method[%zu] routine link metadata drift",
                        header_index, i);
                }
                return false;
            }
            if (routine->is_action_like != method->is_action_like ||
                ((routine->within_zone == NULL) !=
                    (method->within_zone == NULL)) ||
                (routine->within_zone != NULL &&
                    strcmp(routine->within_zone,
                           method->within_zone) != 0)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] method[%zu] callable contract disagrees with routine projection",
                        header_index, i);
                }
                return false;
            }
        }

        if (method->name == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has no name metadata",
                    header_index, i);
            }
            return false;
        }
        if (method->param_count > 0 && method->params == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has parameter count but no parameter storage",
                    header_index, i);
            }
            return false;
        }
        if (method->param_count > 0 && method->param_type_names == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has parameter count but no type-name storage",
                    header_index, i);
            }
            return false;
        }
        if (method->authorized_by_count > 0
            && method->authorized_by_names == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has authorization count but no authorization storage",
                    header_index, i);
            }
            return false;
        }
        if (method->required_ability_ref_count > 0 &&
            method->required_ability_refs == NULL) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has required abilities but no structured storage",
                    header_index, i);
            }
            return false;
        }
        if (!method->is_action_like &&
            (method->required_ability_ref_count > 0 ||
             method->within_zone != NULL || method->causes_effect != NULL ||
             method->authorized_by_count > 0)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] carries action-only contract facts on a function",
                    header_index, i);
            }
            return false;
        }
        if (method->is_action_like &&
            (header->ast_type != AST_CLASS_DECL ||
             header->nominal_kind != NOMINAL_DECL_SUBJECT)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] action is not owned by a subject",
                    header_index, i);
            }
            return false;
        }
        if ((method->within_zone != NULL &&
                method->within_zone[0] == '\0') ||
            (method->causes_effect != NULL &&
                method->causes_effect[0] == '\0')) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has an empty action contract name",
                    header_index, i);
            }
            return false;
        }
        for (size_t r = 0; r < method->required_ability_ref_count; r++) {
            const MIRAbilityRef *ref = &method->required_ability_refs[r];
            if (ref->base_name == NULL || ref->base_name[0] == '\0' ||
                (ref->actual_arg_count > 0 &&
                 ref->actual_arg_type_names == NULL)) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] method[%zu] required ability[%zu] is incomplete",
                        header_index, i, r);
                }
                return false;
            }
            for (size_t a = 0; a < ref->actual_arg_count; a++) {
                if (ref->actual_arg_type_names[a] == NULL ||
                    ref->actual_arg_type_names[a][0] == '\0') {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR declaration header[%zu] method[%zu] required ability[%zu] actual[%zu] is incomplete",
                            header_index, i, r, a);
                    }
                    return false;
                }
            }
            for (size_t p = 0; p < r; p++) {
                const MIRAbilityRef *prior =
                    &method->required_ability_refs[p];
                bool same = prior->base_name != NULL &&
                    strcmp(ref->base_name, prior->base_name) == 0 &&
                    ref->actual_arg_count == prior->actual_arg_count;
                for (size_t a = 0;
                     same && a < ref->actual_arg_count; a++) {
                    same = strcmp(ref->actual_arg_type_names[a],
                                  prior->actual_arg_type_names[a]) == 0;
                }
                if (same) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR declaration header[%zu] method[%zu] required ability[%zu] is duplicated",
                            header_index, i, r);
                    }
                    return false;
                }
            }
        }
        for (size_t a = 0; a < method->authorized_by_count; a++) {
            if (method->authorized_by_names[a] == NULL ||
                method->authorized_by_names[a][0] == '\0') {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] method[%zu] authorization[%zu] has no subject metadata",
                        header_index, i, a);
                }
                return false;
            }
            for (size_t b = 0; b < a; b++) {
                if (strcmp(method->authorized_by_names[a],
                           method->authorized_by_names[b]) == 0) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR declaration header[%zu] method[%zu] authorization[%zu] is duplicated",
                            header_index, i, a);
                    }
                    return false;
                }
            }
        }
        if ((method->declared_capabilities &
                ~pgy_callable_contract_vocabulary_known_mask(
                    PGY_CALLABLE_CONTRACT_AXIS_CAPABILITY)) != 0 ||
            (!method->has_caps_clause &&
                method->declared_capabilities != 0) ||
            (method->has_caps_clause &&
                method->declared_capabilities == 0) ||
            (method->declared_effects &
                ~pgy_callable_contract_vocabulary_known_mask(
                    PGY_CALLABLE_CONTRACT_AXIS_EFFECT)) != 0 ||
            (!method->has_effects_clause && method->declared_effects != 0)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has invalid callable caps/effects presence or mask",
                    header_index, i);
            }
            return false;
        }
        if (method->projection_write_count > 0
            && (method->projection_write_root_names == NULL
                || method->projection_write_member_names == NULL)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has projection write metadata count but no storage",
                    header_index, i);
            }
            return false;
        }
        for (size_t w = 0; w < method->projection_write_count; w++) {
            if (method->projection_write_root_names[w] == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] method[%zu] projection write[%zu] has no root metadata",
                        header_index, i, w);
                }
                return false;
            }
        }
        if (method->projection_call_count > 0
            && (method->projection_call_receiver_names == NULL
                || method->projection_call_method_names == NULL)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has projection call metadata count but no storage",
                    header_index, i);
            }
            return false;
        }
        for (size_t c = 0; c < method->projection_call_count; c++) {
            if (method->projection_call_receiver_names[c] == NULL
                || method->projection_call_method_names[c] == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] method[%zu] projection call[%zu] has incomplete metadata",
                        header_index, i, c);
                }
                return false;
            }
        }
    }

    return true;
}

bool
mir_validate_decl_header_metadata(const MIRProgram *mir,
                                  char **error_message)
{
    if (mir == NULL)
        return false;

    if (mir->decl_header_count > 0 && mir->decl_headers == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR program has %zu declaration header(s) but no declaration header storage",
                mir->decl_header_count);
        }
        return false;
    }

    for (size_t i = 0; i < mir->decl_header_count; i++) {
        const MIRDeclHeader *header = &mir->decl_headers[i];
        if (header->name != NULL) {
            for (size_t j = i + 1; j < mir->decl_header_count; j++) {
                const MIRDeclHeader *other = &mir->decl_headers[j];
                if (other->name != NULL && strcmp(header->name, other->name) == 0) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR declaration header[%zu] '%s' duplicates declaration header[%zu]",
                            j, other->name, i);
                    }
                    return false;
                }
            }
        }
        if (!mir_validate_decl_method_metadata(
                mir, header, i, error_message)) {
            return false;
        }
    }

    return true;
}
