#include "mir_decl_header_variants.h"
#include "mir_type_helpers.h"

#include "../parser/ast_api.h"

#include <stdlib.h>

static void
mir_decl_enum_variant_metadata_clear(MIRDeclEnumVariant *meta)
{
    if (meta == NULL)
        return;
    if (meta->param_type_names != NULL) {
        for (size_t p = 0; p < meta->param_count; p++)
            free((void *)meta->param_type_names[p]);
    }
    free((void *)meta->param_type_names);
    meta->param_type_names = NULL;
    meta->param_count = 0;
}

void
mir_decl_header_free_variants(MIRDeclHeader *header)
{
    if (header == NULL || header->variant_metadata == NULL)
        return;
    for (size_t v = 0; v < header->variant_metadata_count; v++)
        mir_decl_enum_variant_metadata_clear(&header->variant_metadata[v]);
    free(header->variant_metadata);
    header->variant_metadata = NULL;
    header->variant_metadata_count = 0;
}

bool
mir_decl_header_set_variants(MIRDeclHeader *header, ASTNode *decl)
{
    size_t variant_count = 0;
    char **names;
    MIRDeclEnumVariant *meta;

    if (header == NULL || decl == NULL || decl->type != AST_ENUM_DECL)
        return true;
    names = ast_enum_variants(decl, &variant_count);
    header->variant_count = variant_count;
    if (variant_count == 0)
        return true;
    meta = calloc(variant_count, sizeof(MIRDeclEnumVariant));
    if (meta == NULL)
        return false;
    for (size_t i = 0; i < variant_count; i++) {
        size_t pc = ast_enum_variant_param_count(decl, i);
        meta[i].name = names != NULL ? names[i] : NULL;
        meta[i].param_count = pc;
        meta[i].param_type_names = NULL;
        if (pc == 0)
            continue;
        meta[i].param_type_names = calloc(pc, sizeof(const char *));
        if (meta[i].param_type_names == NULL) {
            for (size_t k = 0; k < i; k++)
                mir_decl_enum_variant_metadata_clear(&meta[k]);
            free(meta);
            return false;
        }
        for (size_t p = 0; p < pc; p++) {
            ASTNode *pt = ast_enum_variant_param(decl, i, p);
            meta[i].param_type_names[p] =
                mir_capture_type_name(pt, NULL);
            if (meta[i].param_type_names[p] == NULL) {
                for (size_t k = 0; k <= i; k++)
                    mir_decl_enum_variant_metadata_clear(&meta[k]);
                free(meta);
                return false;
            }
        }
    }
    header->variant_metadata = meta;
    header->variant_metadata_count = variant_count;
    return true;
}
