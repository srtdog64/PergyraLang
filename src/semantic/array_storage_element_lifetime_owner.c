/* Canonical native plain-element lifetime predicate for public storage release. */
#include "array_storage_element_lifetime_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "compiler/decl_field_model.h"
#include <stdlib.h>

static bool
array_storage_plain_at_depth(const Type *type, SemanticContext *ctx, unsigned depth)
{
    if (type == NULL || depth >= 32)
        return false;
    if (type_equals(type, TYPE_INT) || type_equals(type, TYPE_LONG) || type_equals(type, TYPE_FLOAT)
        || type_equals(type, TYPE_DOUBLE) || type_equals(type, TYPE_BOOL) || type_equals(type, TYPE_DURATION)) return true;
    if (type_is_constructed_named(type, "Result"))
        return type_constructed_arg_count(type) == 2 && array_storage_plain_at_depth(type_get_constructed_arg(type, 0), ctx, depth + 1)
            && array_storage_plain_at_depth(type_get_constructed_arg(type, 1), ctx, depth + 1);
    if (type->kind == TYPE_KIND_ENUM) {
        ASTNode *decl = semantic_find_enum_decl_by_name(ctx, type->name);
        size_t count = 0;
        if (decl == NULL)
            return false;
        (void)ast_enum_variants(decl, &count);
        for (size_t v = 0; v < count; v++) {
            for (size_t p = 0; p < ast_enum_variant_param_count(decl, v); p++) {
                Type *payload = semantic_type_resolution_lookup_metadata_type_ref(ctx, ast_enum_variant_param(decl, v, p));
                if (!array_storage_plain_at_depth(payload, ctx, depth + 1))
                    return false;
            }
        }
        return count != 0;
    }
    if (type->kind != TYPE_KIND_CLASS || type->nominal_flavor != TYPE_NOMINAL_STRUCT) return false;
    ASTNode *decl = semantic_host_decl_for_type(ctx, type);
    if (decl == NULL || decl->type != AST_CLASS_DECL) return false;
    PgyDeclField *fields = NULL;
    size_t count = 0;
    if (!pgy_class_decl_field_model_try_build(decl, &fields, &count)) return false;
    bool plain = true;
    for (size_t i = 0; i < count && plain; i++) {
        Type *field = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, fields[i].type_ast);
        plain = array_storage_plain_at_depth(field, ctx, depth + 1);
    }
    free(fields);
    return plain;
}

bool
semantic_array_storage_plain_element(const Type *type, SemanticContext *ctx)
{
    return array_storage_plain_at_depth(type, ctx, 0);
}
