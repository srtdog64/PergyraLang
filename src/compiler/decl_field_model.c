/*
 * F2 (docs/144) pre-semantic declaration-field shape model — Phase 1 scaffold.
 */

#include "compiler/decl_field_model.h"
#include "parser/ast_api.h"

#include <stdio.h>
#include <stdlib.h>

size_t
pgy_class_decl_field_model_build(const ASTNode *class_decl, PgyDeclField **out)
{
    size_t       count  = 0;
    ClassField **fields = ast_class_fields(class_decl, &count);

    if (out != NULL)
        *out = NULL;
    if (count == 0 || out == NULL)
        return count;

    PgyDeclField *model = calloc(count, sizeof(*model));
    if (model == NULL)
        return 0; /* allocation failure surfaces as an empty model to the caller */

    for (size_t i = 0; i < count; i++) {
        ClassField *f = (fields != NULL) ? fields[i] : NULL;
        if (f == NULL)
            continue;
        model[i].declaration_syntax_id = f->stable_id;
        model[i].name                = f->name;
        model[i].type_ast            = f->type;
        model[i].access              = f->access;
        model[i].has_explicit_access = f->has_explicit_access;
        model[i].is_mutable          = f->is_mutable;
        model[i].is_vessel_field     = f->is_vessel_field;
        model[i].has_default         = (f->default_value != NULL);
    }

    *out = model;
    return count;
}

void
pgy_decl_field_model_free(PgyDeclField *fields, size_t count)
{
    (void) count;
    free(fields);
}

static bool
drift_enabled(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = (getenv("PGY_F2_DRIFT_CHECK") != NULL) ? 1 : 0;
    return enabled != 0;
}

size_t
pgy_class_decl_field_model_drift(const ASTNode *class_decl)
{
    if (!drift_enabled())
        return 0;

    size_t       ast_count = 0;
    ClassField **fields    = ast_class_fields(class_decl, &ast_count);

    PgyDeclField *model       = NULL;
    size_t        model_count = pgy_class_decl_field_model_build(class_decl, &model);
    size_t        mismatches  = 0;

    if (model_count != ast_count) {
        fprintf(stderr, "[f2-drift] class field count: model=%zu ast=%zu\n",
                model_count, ast_count);
        mismatches++;
    }

    size_t n = (model_count < ast_count) ? model_count : ast_count;
    for (size_t i = 0; i < n; i++) {
        ClassField   *f = (fields != NULL) ? fields[i] : NULL;
        PgyDeclField *m = &model[i];
        if (f == NULL)
            continue;
        if (m->declaration_syntax_id != f->stable_id
            || m->name != f->name
            || m->type_ast != f->type
            || m->access != f->access
            || m->has_explicit_access != f->has_explicit_access
            || m->is_mutable != f->is_mutable
            || m->is_vessel_field != f->is_vessel_field
            || m->has_default != (f->default_value != NULL)) {
            fprintf(stderr, "[f2-drift] class field %zu '%s' shape mismatch\n",
                    i, (f->name != NULL) ? f->name : "<field>");
            mismatches++;
        }
    }

    pgy_decl_field_model_free(model, model_count);
    return mismatches;
}
