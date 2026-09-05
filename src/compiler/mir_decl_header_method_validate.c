#include "mir_decl_header_method_validate.h"
#include "mir_fact_validate_internal.h"

#include "../semantic/callable_contract_vocabulary.h"

#include <string.h>

#define mir_strdup_fmt mir_fact_strdup_fmt

bool
mir_validate_decl_method_metadata(const MIRProgram *mir,
                                  const MIRDeclHeader *header,
                                  size_t header_index,
                                  char **error_message)
{
    MIRRoutineInventory inventory;

    if (mir == NULL || header == NULL)
        return false;
    mir_routine_inventory_from_program(mir, &inventory);

    for (size_t i = 0; i < header->method_metadata_count; i++) {
        const MIRDeclMethod *method = &header->method_metadata[i];

        if (method->source_syntax_id == 0) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] has no source identity",
                    header_index, i);
            }
            return false;
        }

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
                || routine->source_syntax_id != method->source_syntax_id
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
