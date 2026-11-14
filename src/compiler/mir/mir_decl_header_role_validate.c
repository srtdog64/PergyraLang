#include "mir_decl_header_role_validate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_decl_header_role_strdup_fmt(const char *fmt, ...)
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

bool
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
            *error_message = mir_decl_header_role_strdup_fmt(
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
            *error_message = mir_decl_header_role_strdup_fmt(
                "MIR declaration header[%zu] '%s' has role impl metadata on a non-role declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }

    if (header->role_impl_metadata_count != header->role_impl_count) {
        if (error_message != NULL) {
            *error_message = mir_decl_header_role_strdup_fmt(
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
                *error_message = mir_decl_header_role_strdup_fmt(
                    "MIR declaration header[%zu] role impl[%zu] has owner metadata drift",
                    header_index, i);
            }
            return false;
        }
        if (impl->method_start_index != expected_method_index) {
            if (error_message != NULL) {
                *error_message = mir_decl_header_role_strdup_fmt(
                    "MIR declaration header[%zu] role impl[%zu] method span metadata drift",
                    header_index, i);
            }
            return false;
        }
        if (impl->method_start_index > header->method_metadata_count
            || impl->method_count
                > header->method_metadata_count - impl->method_start_index) {
            if (error_message != NULL) {
                *error_message = mir_decl_header_role_strdup_fmt(
                    "MIR declaration header[%zu] role impl[%zu] method span exceeds method metadata",
                    header_index, i);
            }
            return false;
        }
        if (ref->base_name == NULL
            || (ref->actual_arg_count > 0
                && ref->actual_arg_type_names == NULL)) {
            if (error_message != NULL) {
                *error_message = mir_decl_header_role_strdup_fmt(
                    "MIR declaration header[%zu] role impl[%zu] has incomplete ability-ref metadata",
                    header_index, i);
            }
            return false;
        }
        for (size_t arg = 0; arg < ref->actual_arg_count; arg++) {
            if (ref->actual_arg_type_names[arg] == NULL) {
                if (error_message != NULL) {
                    *error_message = mir_decl_header_role_strdup_fmt(
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
        && impl_method_total + header->role_override_method_count
            != header->method_count) {
        if (error_message != NULL) {
            *error_message = mir_decl_header_role_strdup_fmt(
                "MIR declaration header[%zu] '%s' role method count %zu plus override count %zu does not match method metadata count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                impl_method_total,
                header->role_override_method_count,
                header->method_count);
        }
        return false;
    }

    return true;
}

bool
mir_validate_decl_role_include_metadata(const MIRDeclHeader *header,
                                        size_t header_index,
                                        char **error_message)
{
    if (header == NULL)
        return false;

    if (header->role_include_metadata_count > 0
        && header->role_include_metadata == NULL) {
        if (error_message != NULL) {
            *error_message = mir_decl_header_role_strdup_fmt(
                "MIR declaration header[%zu] '%s' has %zu role include metadata row(s) but no storage",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->role_include_metadata_count);
        }
        return false;
    }

    if (header->role_include_metadata_count != 0
        && header->ast_type != AST_ROLE_DECL) {
        if (error_message != NULL) {
            *error_message = mir_decl_header_role_strdup_fmt(
                "MIR declaration header[%zu] '%s' has role include metadata on a non-role declaration",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }

    if (header->role_include_metadata_count != header->role_include_count) {
        if (error_message != NULL) {
            *error_message = mir_decl_header_role_strdup_fmt(
                "MIR declaration header[%zu] '%s' role include metadata count %zu does not match declaration include count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->role_include_metadata_count,
                header->role_include_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->role_include_metadata_count; i++) {
        const MIRDeclRoleInclude *include =
            &header->role_include_metadata[i];
        if (include->owner_name == NULL
            || header->name == NULL
            || strcmp(include->owner_name, header->name) != 0) {
            if (error_message != NULL) {
                *error_message = mir_decl_header_role_strdup_fmt(
                    "MIR declaration header[%zu] role include[%zu] has owner metadata drift",
                    header_index, i);
            }
            return false;
        }
        if (include->role_name == NULL) {
            if (error_message != NULL) {
                *error_message = mir_decl_header_role_strdup_fmt(
                    "MIR declaration header[%zu] role include[%zu] has no role name metadata",
                    header_index, i);
            }
            return false;
        }
    }

    return true;
}
