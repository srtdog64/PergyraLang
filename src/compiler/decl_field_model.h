#ifndef PERGYRA_DECL_FIELD_MODEL_H
#define PERGYRA_DECL_FIELD_MODEL_H

#include "parser/ast_types.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * F2 (docs/144): pre-semantic declaration-field shape model.
 *
 * Phase 1 scaffold. This is the future single owner of class-field SHAPE, built
 * once after parse and (in later phases) consumed by both semantic and MIR
 * lowering instead of each re-reading ast_class_fields(...).
 *
 * Field shape is syntactic (docs/144 §3): name / access / mutability / vessel /
 * default-presence are all parse-time facts, so this model has no semantic
 * type-resolution dependency. The field TYPE is carried as an AST reference
 * (type_ast); rendering it to a lossless string is F1-gated (callable/tuple
 * types are not yet losslessly representable) and is a consumer concern, so it
 * is deliberately absent here.
 */
typedef struct {
    uint32_t        declaration_syntax_id;
    const char     *name;
    ASTNode        *type_ast;            /* type carrier; lossless rendering is F1-gated */
    AccessModifier  access;
    bool            has_explicit_access;
    bool            is_mutable;
    bool            is_vessel_field;
    bool            has_default;
} PgyDeclField;

/*
 * Build the field-shape model from a class declaration's AST fields.
 * Returns the field count; on a non-NULL out, *out receives a heap array to be
 * released with pgy_decl_field_model_free. *out is NULL when the count is 0.
 */
size_t pgy_class_decl_field_model_build(const ASTNode *class_decl, PgyDeclField **out);

void pgy_decl_field_model_free(PgyDeclField *fields, size_t count);

/*
 * Phase-1 dual-run drift check (opt-in via the PGY_F2_DRIFT_CHECK env var).
 * Rebuilds the model and compares it field-by-field against ast_class_fields,
 * returning the number of diverging fields (0 = faithful, total carrier).
 * No-op returning 0 when the env var is unset, so default builds pay nothing and
 * change no behavior. Mismatch details are written to stderr.
 */
size_t pgy_class_decl_field_model_drift(const ASTNode *class_decl);

#endif /* PERGYRA_DECL_FIELD_MODEL_H */
