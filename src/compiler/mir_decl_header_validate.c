#include "mir_fact_validate.h"

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

static bool
mir_decl_header_ast_shape(const MIRDeclHeader *header,
                          const char **name_out,
                          size_t *method_count_out,
                          bool *uses_pointer_self_out)
{
    ASTNode *ast;

    if (name_out != NULL)
        *name_out = NULL;
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (uses_pointer_self_out != NULL)
        *uses_pointer_self_out = false;
    if (header == NULL || header->source_ast == NULL)
        return false;

    ast = header->source_ast;
    switch (ast->type) {
    case AST_CLASS_DECL:
        if (name_out != NULL)
            *name_out = ast->data.class_decl.name;
        if (method_count_out != NULL)
            *method_count_out = ast->data.class_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out =
                ast->data.class_decl.nominal_kind == NOMINAL_DECL_SUBJECT
                || ast->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL;
        return true;
    case AST_ENUM_DECL:
        if (name_out != NULL)
            *name_out = ast->data.enum_decl.name;
        if (method_count_out != NULL)
            *method_count_out = ast->data.enum_decl.method_count;
        return true;
    case AST_PARTY_DECL:
        if (name_out != NULL)
            *name_out = ast->data.party_decl.name;
        if (method_count_out != NULL)
            *method_count_out = ast->data.party_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ROLE_DECL:
        if (name_out != NULL)
            *name_out = ast->data.role_decl.name;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ROSTER_DECL:
        if (name_out != NULL)
            *name_out = ast->data.roster_decl.name;
        if (method_count_out != NULL)
            *method_count_out = ast->data.roster_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_WORLD_DECL:
        if (name_out != NULL)
            *name_out = ast->data.world_decl.name;
        if (method_count_out != NULL)
            *method_count_out = ast->data.world_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_RELATION_DECL:
        if (name_out != NULL)
            *name_out = ast->data.relation_decl.name;
        if (method_count_out != NULL)
            *method_count_out = ast->data.relation_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_EFFECT_DECL:
        if (name_out != NULL)
            *name_out = ast->data.effect_decl.name;
        if (method_count_out != NULL)
            *method_count_out = ast->data.effect_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ZONE_DECL:
        if (name_out != NULL)
            *name_out = ast->data.zone_decl.name;
        if (method_count_out != NULL)
            *method_count_out = ast->data.zone_decl.method_count;
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    default:
        return false;
    }
}

static bool
mir_validate_decl_header_ast_compat(const MIRDeclHeader *header,
                                    size_t header_index,
                                    char **error_message)
{
    const char *ast_name = NULL;
    size_t ast_method_count = 0;
    bool ast_uses_pointer_self = false;

    if (header == NULL)
        return false;
    if (header->source_ast == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has no AST compatibility payload",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->ast_type != header->source_ast->type) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' AST type metadata drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (!mir_decl_header_ast_shape(
            header, &ast_name, &ast_method_count, &ast_uses_pointer_self)) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has unsupported declaration AST shape",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->name == NULL
        || ast_name == NULL
        || strcmp(header->name, ast_name) != 0) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] name metadata drift",
                header_index);
        }
        return false;
    }
    if (header->method_count != ast_method_count
        && header->ast_type != AST_ROLE_DECL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' AST method-count compatibility drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->uses_pointer_self != ast_uses_pointer_self) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' pointer-self ABI metadata drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
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
    if (mir == NULL || header == NULL)
        return false;

    if (!mir_validate_decl_header_ast_compat(header, header_index, error_message))
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

    if (header->ast_type != AST_ROLE_DECL
        && header->method_metadata_count != header->method_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' method metadata count %zu does not match AST compatibility count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->method_metadata_count,
                header->method_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->method_metadata_count; i++) {
        const MIRDeclMethod *method = &header->method_metadata[i];
        ASTNode *ast = method->source_ast;

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

        if (method->has_routine && method->routine_index >= mir->routine_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] routine index %zu exceeds routine count %zu",
                    header_index, i, method->routine_index, mir->routine_count);
            }
            return false;
        }

        if (ast == NULL || ast->type != AST_FUNC_DECL)
            continue;
        if (method->name != ast->data.func_decl.name
            && (method->name == NULL || ast->data.func_decl.name == NULL
                || strcmp(method->name, ast->data.func_decl.name) != 0)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] name metadata drift",
                    header_index, i);
            }
            return false;
        }
        if (method->params != ast->data.func_decl.params
            || method->param_count != ast->data.func_decl.param_count
            || method->return_type != ast->data.func_decl.return_type) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] signature metadata drift",
                    header_index, i);
            }
            return false;
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
