#include "mir_fact_validate.h"
#include "mir_decl_header_zone_state_validate.h"
#include "mir_decl_headers.h"
#include "mir_type_helpers.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_decl_header_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    int written;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    written = vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    if (written < 0 || written != length) {
        free(result);
        return NULL;
    }
    return result;
}

#define mir_strdup_fmt mir_decl_header_strdup_fmt

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
mir_decl_header_type_requires_pointer_self(ASTNodeType type)
{
    switch (type) {
    case AST_PARTY_DECL:
    case AST_ROSTER_DECL:
    case AST_WORLD_DECL:
    case AST_RELATION_DECL:
    case AST_EFFECT_DECL:
    case AST_ROLE_DECL:
    case AST_ZONE_DECL:
        return true;
    default:
        return false;
    }
}

static bool
mir_validate_decl_header_shape_metadata(const MIRDeclHeader *header,
                                        size_t header_index,
                                        char **error_message)
{
    if (header == NULL)
        return false;
    if (header->name == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] has no declaration name metadata",
                header_index);
        }
        return false;
    }
    if (header->ast_type == AST_PROGRAM) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has invalid declaration type metadata",
                header_index, header->name);
        }
        return false;
    }
    if (header->ast_type == AST_TYPE_ALIAS
        && header->type_alias_target_type_name == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' type-alias target metadata drift",
                header_index, header->name);
        }
        return false;
    }
    if (header->ast_type == AST_INTENT_DECL
        && header->intent_retry_count < 0) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' intent retry metadata drift",
                header_index, header->name);
        }
        return false;
    }
    if (mir_decl_header_type_requires_pointer_self(header->ast_type)
        && !header->uses_pointer_self) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' pointer-self ABI metadata drift",
                header_index, header->name);
        }
        return false;
    }
    if (header->ast_type == AST_CLASS_DECL
        && (header->nominal_kind == NOMINAL_DECL_SUBJECT
            || header->nominal_kind == NOMINAL_DECL_VESSEL)
        && !header->uses_pointer_self) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' nominal pointer-self metadata drift",
                header_index, header->name);
        }
        return false;
    }
    return true;
}

static bool
mir_validate_decl_role_impl_metadata(const MIRDeclHeader *header,
                                     size_t header_index,
                                     char **error_message)
{
    size_t impl_method_total = 0;
    size_t expected_method_index = 0;

    if (header == NULL)
        return false;

    if (header->role_impl_metadata_count > 0
        && header->role_impl_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu role impl metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->role_impl_metadata_count);
        }
        return false;
    }

    if (header->role_impl_metadata_count != 0
        && !(header->ast_type == AST_ROLE_DECL)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has role impl metadata on a non-role declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }

    if (header->role_impl_metadata_count != header->role_impl_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' role impl metadata count %zu does not match declaration impl count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->role_impl_metadata_count,
                header->role_impl_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->role_impl_metadata_count; i++) {
        const MIRDeclRoleImpl *impl = &header->role_impl_metadata[i];
        const MIRAbilityRef *ref = &impl->ability_ref;

        if (impl->owner_name == NULL
            || header->name == NULL
            || strcmp(impl->owner_name, header->name) != 0) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] role impl[%zu] has owner metadata drift",
                    header_index, i);
            }
            return false;
        }
        if (impl->method_start_index != expected_method_index) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] role impl[%zu] method span metadata drift",
                    header_index, i);
            }
            return false;
        }
        if (impl->method_start_index > header->method_metadata_count
            || impl->method_count
                > header->method_metadata_count - impl->method_start_index) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] role impl[%zu] method span exceeds method metadata",
                    header_index, i);
            }
            return false;
        }
        if (ref->base_name == NULL
            || (ref->actual_arg_count > 0
                && ref->actual_arg_type_names == NULL)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] role impl[%zu] has incomplete ability-ref metadata",
                    header_index, i);
            }
            return false;
        }
        for (size_t arg = 0; arg < ref->actual_arg_count; arg++) {
            if (ref->actual_arg_type_names[arg] == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] role impl[%zu] actual[%zu] has no type metadata",
                        header_index, i, arg);
                }
                return false;
            }
        }
        impl_method_total += impl->method_count;
        expected_method_index += impl->method_count;
    }

    if (header->ast_type == AST_ROLE_DECL
        && impl_method_total != header->method_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' role impl method count %zu does not match method metadata count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                impl_method_total,
                header->method_count);
        }
        return false;
    }

    return true;
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

    if (!mir_validate_decl_role_impl_metadata(
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
