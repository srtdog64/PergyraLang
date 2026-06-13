#include "mir_fact_validate.h"
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

static size_t
mir_decl_ast_domain_method_count(ASTNode *ast)
{
    size_t method_count = 0;

    if (ast == NULL)
        return 0;
    switch (ast->type) {
    case AST_WORLD_DECL:
        (void) ast_world_methods(ast, &method_count);
        break;
    case AST_RELATION_DECL:
        (void) ast_relation_methods(ast, &method_count);
        break;
    case AST_EFFECT_DECL:
        (void) ast_effect_methods(ast, &method_count);
        break;
    case AST_ZONE_DECL:
        (void) ast_zone_methods(ast, &method_count);
        break;
    default:
        break;
    }
    return method_count;
}

static size_t
mir_decl_ast_field_count(ASTNode *ast)
{
    size_t count = 0;
    size_t extra = 0;

    if (ast == NULL)
        return 0;
    switch (ast->type) {
    case AST_CLASS_DECL:
        (void) ast_class_fields(ast, &count);
        return count;
    case AST_PARTY_DECL:
        return ast_party_role_count(ast) + ast_party_shared_count(ast);
    case AST_ROSTER_DECL:
        return ast_roster_party_count(ast) + ast_roster_shared_count(ast);
    case AST_WORLD_DECL:
        (void) ast_world_rosters(ast, &count);
        (void) ast_world_zones(ast, &extra);
        count += extra;
        (void) ast_world_shared_fields(ast, &extra);
        return count + extra;
    case AST_RELATION_DECL:
        (void) ast_relation_slots(ast, &count);
        (void) ast_relation_shared_fields(ast, &extra);
        return count + extra;
    case AST_EFFECT_DECL:
        (void) ast_effect_slots(ast, &count);
        (void) ast_effect_shared_fields(ast, &extra);
        return count + extra;
    case AST_ZONE_DECL:
        (void) ast_zone_slots(ast, &count);
        (void) ast_zone_layer_slots(ast, &extra);
        count += extra;
        (void) ast_zone_shared_fields(ast, &extra);
        return count + extra;
    default:
        return 0;
    }
}

static bool
mir_decl_header_ast_shape(const MIRDeclHeader *header,
                          const char **name_out,
                          size_t *generic_count_out,
                          size_t *method_count_out,
                          size_t *field_count_out,
                          bool *uses_pointer_self_out)
{
    ASTNode *ast;

    if (name_out != NULL)
        *name_out = NULL;
    if (generic_count_out != NULL)
        *generic_count_out = 0;
    if (method_count_out != NULL)
        *method_count_out = 0;
    if (field_count_out != NULL)
        *field_count_out = 0;
    if (uses_pointer_self_out != NULL)
        *uses_pointer_self_out = false;
    ast = mir_decl_header_source_ast(header);
    if (header == NULL || ast == NULL)
        return false;
    if (generic_count_out != NULL) {
        GenericParams *params = ast_declaration_generic_params(ast);
        *generic_count_out = ast_generic_param_count(params);
    }

    switch (ast->type) {
    case AST_FUNC_DECL:
        if (name_out != NULL)
            *name_out = ast_declaration_name(ast);
        return true;
    case AST_TYPE_ALIAS:
        if (name_out != NULL)
            *name_out = ast_type_alias_name(ast);
        return true;
    case AST_CLASS_DECL:
        if (name_out != NULL)
            *name_out = ast_class_name(ast);
        if (method_count_out != NULL)
            (void) ast_class_methods(ast, method_count_out);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out =
                ast_class_nominal_kind(ast) == NOMINAL_DECL_SUBJECT
                || ast_class_nominal_kind(ast) == NOMINAL_DECL_VESSEL;
        return true;
    case AST_ENUM_DECL:
        if (name_out != NULL)
            *name_out = ast_enum_name(ast);
        if (method_count_out != NULL)
            (void) ast_enum_methods(ast, method_count_out);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        return true;
    case AST_ABILITY_DECL:
        if (name_out != NULL)
            *name_out = ast_ability_name(ast);
        return true;
    case AST_PARTY_DECL:
        if (name_out != NULL)
            *name_out = ast_party_name(ast);
        if (method_count_out != NULL)
            *method_count_out = ast_party_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ROLE_DECL:
        if (name_out != NULL)
            *name_out = ast_role_name(ast);
        if (method_count_out != NULL
            && !ast_role_impl_method_total_count(ast, method_count_out)) {
            return false;
        }
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ROSTER_DECL:
        if (name_out != NULL)
            *name_out = ast_roster_name(ast);
        if (method_count_out != NULL)
            *method_count_out = ast_roster_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_WORLD_DECL:
        if (name_out != NULL)
            *name_out = ast_world_name(ast);
        if (method_count_out != NULL)
            *method_count_out = mir_decl_ast_domain_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_RELATION_DECL:
        if (name_out != NULL)
            *name_out = ast_relation_name(ast);
        if (method_count_out != NULL)
            *method_count_out = mir_decl_ast_domain_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_EFFECT_DECL:
        if (name_out != NULL)
            *name_out = ast_effect_name(ast);
        if (method_count_out != NULL)
            *method_count_out = mir_decl_ast_domain_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    case AST_ZONE_DECL:
        if (name_out != NULL)
            *name_out = ast_zone_name(ast);
        if (method_count_out != NULL)
            *method_count_out = mir_decl_ast_domain_method_count(ast);
        if (field_count_out != NULL)
            *field_count_out = mir_decl_ast_field_count(ast);
        if (uses_pointer_self_out != NULL)
            *uses_pointer_self_out = true;
        return true;
    default:
        return false;
    }
}

static char *
mir_decl_field_expected_type_name(const MIRDeclField *field)
{
    ASTNode *source;

    if (field == NULL)
        return NULL;
    if (field->type != NULL)
        return mir_capture_type_name(field->type, NULL);

    source = field->source_ast;
    if (source == NULL)
        return NULL;

    switch (field->kind) {
    case MIR_DECL_FIELD_ROSTER_SLOT:
        return mir_capture_type_name(NULL, ast_roster_slot_party_type(source));
    case MIR_DECL_FIELD_WORLD_ROSTER_SLOT:
        return mir_capture_type_name(NULL, ast_world_roster_type_name(source));
    case MIR_DECL_FIELD_WORLD_ZONE_SLOT:
        return mir_capture_type_name(NULL, ast_world_zone_type_name(source));
    case MIR_DECL_FIELD_ZONE_LAYER_SLOT:
        return mir_capture_type_name(NULL,
            ast_zone_layer_slot_layer_type(source));
    default:
        return NULL;
    }
}

static bool
mir_validate_decl_header_ast_compat(const MIRDeclHeader *header,
                                    size_t header_index,
                                    char **error_message)
{
    const char *ast_name = NULL;
    size_t ast_generic_count = 0;
    size_t ast_method_count = 0;
    size_t ast_field_count = 0;
    bool ast_uses_pointer_self = false;

    if (header == NULL)
        return false;
    ASTNode *source_ast = mir_decl_header_source_ast(header);

    if (source_ast == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' has no AST compatibility payload",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->ast_type != source_ast->type) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' AST type metadata drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (!mir_decl_header_ast_shape(
            header, &ast_name, &ast_generic_count, &ast_method_count, &ast_field_count,
            &ast_uses_pointer_self)) {
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
    if (header->method_count != ast_method_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' AST method-count compatibility drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->generic_param_count != ast_generic_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' AST generic-count compatibility drift",
                header_index,
                header->name != NULL ? header->name : "(anonymous)");
        }
        return false;
    }
    if (header->field_count != ast_field_count) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR declaration header[%zu] '%s' AST field-count compatibility drift",
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
    MIRRoutineInventory inventory;
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

    if (header->method_metadata_count != header->method_count) {
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
                "MIR declaration header[%zu] '%s' field metadata count %zu does not match AST compatibility count %zu",
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
                "MIR declaration header[%zu] '%s' generic metadata count %zu does not match AST compatibility count %zu",
                header_index,
                header->name != NULL ? header->name : "(anonymous)",
                header->generic_metadata_count,
                header->generic_param_count);
        }
        return false;
    }

    for (size_t i = 0; i < header->generic_metadata_count; i++) {
        const MIRDeclGenericParam *generic = &header->generic_metadata[i];
        GenericParams *ast_params =
            ast_declaration_generic_params(header->source_ast);
        GenericParam *ast_param = ast_generic_param_at(ast_params, i);
        const char *ast_name_at = ast_generic_param_name(ast_param);
        if (generic->source_param != ast_param
            || generic->name != ast_name_at
            || generic->bound_ast != ast_generic_param_constraint(ast_param)
            || generic->default_arg_ast != ast_generic_param_default_type(ast_param)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] generic[%zu] metadata drift",
                    header_index, i);
            }
            return false;
        }
    }

    if (header->ast_type == AST_ENUM_DECL && header->source_ast != NULL) {
        size_t ast_variant_count = 0;
        char **ast_variants = ast_enum_variants(
            header->source_ast,
            &ast_variant_count);
        if (header->variant_metadata_count != ast_variant_count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] enum variant metadata count %zu does not match AST compatibility count %zu",
                    header_index,
                    header->variant_metadata_count,
                    ast_variant_count);
            }
            return false;
        }
        for (size_t i = 0; i < header->variant_metadata_count; i++) {
            const MIRDeclEnumVariant *variant =
                &header->variant_metadata[i];
            const char *ast_name =
                ast_variants != NULL ? ast_variants[i] : NULL;
            size_t ast_param_count =
                ast_enum_variant_param_count(header->source_ast, i);
            if ((variant->name == NULL || ast_name == NULL
                    || strcmp(variant->name, ast_name) != 0)
                || variant->param_count != ast_param_count) {
                if (error_message != NULL) {
                    *error_message = mir_strdup_fmt(
                        "MIR declaration header[%zu] enum variant[%zu] metadata drift",
                        header_index, i);
                }
                return false;
            }
            for (size_t p = 0; p < variant->param_count; p++) {
                ASTNode *ast_param =
                    ast_enum_variant_param(header->source_ast, i, p);
                char *rendered = mir_capture_type_name(ast_param, NULL);
                bool matches = rendered != NULL
                    && variant->param_type_names != NULL
                    && variant->param_type_names[p] != NULL
                    && strcmp(variant->param_type_names[p], rendered) == 0;
                free(rendered);
                if (!matches) {
                    if (error_message != NULL) {
                        *error_message = mir_strdup_fmt(
                            "MIR declaration header[%zu] enum variant[%zu] payload[%zu] type metadata drift",
                            header_index, i, p);
                    }
                    return false;
                }
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

    mir_routine_inventory_from_program(mir, &inventory);

    for (size_t i = 0; i < header->method_metadata_count; i++) {
        const MIRDeclMethod *method = &header->method_metadata[i];
        ASTNode *ast = mir_decl_method_source_ast(method);

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

        if (ast == NULL || ast->type != AST_FUNC_DECL)
            continue;
        const char *ast_name = ast_declaration_name(ast);
        if (method->name != ast_name
            && (method->name == NULL || ast_name == NULL
                || strcmp(method->name, ast_name) != 0)) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR declaration header[%zu] method[%zu] name metadata drift",
                    header_index, i);
            }
            return false;
        }
        size_t ast_param_count = 0;
        FuncParam **ast_params = ast_func_params(ast, &ast_param_count);
        if (method->params != ast_params
            || method->param_count != ast_param_count
            || method->return_type != ast_func_return_type(ast)) {
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
