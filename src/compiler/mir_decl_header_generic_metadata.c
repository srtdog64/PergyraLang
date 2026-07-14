#include "mir_decl_header_generic_metadata.h"

#include "mir_type_helpers.h"

#include <stdint.h>
#include <stdlib.h>

static void
mir_decl_generic_metadata_clear(MIRDeclGenericParam *metadata, size_t count)
{
    if (metadata == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        free(metadata[i].bound_type_name);
        free(metadata[i].default_arg_type_name);
        metadata[i].bound_type_name = NULL;
        metadata[i].default_arg_type_name = NULL;
    }
}

void
mir_decl_header_free_generics(MIRDeclHeader *header)
{
    if (header == NULL)
        return;
    mir_decl_generic_metadata_clear(
        header->generic_metadata, header->generic_metadata_count);
    free(header->generic_metadata);
    header->generic_metadata = NULL;
    header->generic_metadata_count = 0;
    header->generic_param_count = 0;
}

bool
mir_decl_header_set_generics(MIRDeclHeader *header, ASTNode *decl)
{
    GenericParams *params;
    size_t count;

    if (header == NULL)
        return false;

    params = ast_declaration_generic_params(decl);
    count = ast_generic_param_count(params);
    header->generic_param_count = count;
    header->generic_metadata = NULL;
    header->generic_metadata_count = 0;

    if (count == 0)
        return true;
    if (count > SIZE_MAX / sizeof(MIRDeclGenericParam))
        return false;
    header->generic_metadata = calloc(count, sizeof(MIRDeclGenericParam));
    if (header->generic_metadata == NULL)
        return false;

    for (size_t i = 0; i < count; i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        MIRDeclGenericParam *meta = &header->generic_metadata[i];
        ASTNode *constraint;
        ASTNode *default_type;

        if (param == NULL) {
            mir_decl_header_free_generics(header);
            return false;
        }

        constraint = ast_generic_param_constraint(param);
        default_type = ast_generic_param_default_type(param);
        meta->name = ast_generic_param_name(param);
        meta->bound_type_name = mir_capture_type_name(constraint, NULL);
        meta->default_arg_type_name =
            mir_capture_type_name(default_type, NULL);
        if (meta->name == NULL
            || (constraint != NULL && meta->bound_type_name == NULL)
            || (default_type != NULL
                && meta->default_arg_type_name == NULL)) {
            mir_decl_header_free_generics(header);
            return false;
        }
    }
    header->generic_metadata_count = count;
    return true;
}
