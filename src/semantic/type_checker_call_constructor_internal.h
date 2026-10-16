#ifndef PGY_TYPE_CHECKER_CALL_CONSTRUCTOR_INTERNAL_H
#define PGY_TYPE_CHECKER_CALL_CONSTRUCTOR_INTERNAL_H

#include "type_checker.h"

/* Private cross-component declarations; semantic authority is unchanged. */
bool domain_constructor_has_role_filtered_inputs(ASTNode *decl);
ASTNode *domain_constructor_input(ASTNode *decl, size_t index, size_t *count);
Type *constructor_decl_field_resolved_type_at(ASTNode *decl, size_t index,
    const char **field_name_out, SemanticContext *ctx);
ASTNode *constructor_decl_field_type_by_name(ASTNode *decl, const char *name,
    size_t field_count, const char **field_name_out, size_t *selected);
bool constructor_decl_has_channel_field(ASTNode *decl, SemanticContext *ctx,
    const char **field_name_out);
void constructor_reject_channel_field_store(SemanticContext *ctx, ASTNode *site,
    const char *constructor_name, const char *field_name);
bool constructor_payload_type_ready(const Type *type);

#endif
