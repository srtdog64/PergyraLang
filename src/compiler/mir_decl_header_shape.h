#ifndef PERGYRA_MIR_DECL_HEADER_SHAPE_H
#define PERGYRA_MIR_DECL_HEADER_SHAPE_H

#include "mir.h"

size_t mir_decl_header_ast_domain_method_count(ASTNode *ast);
size_t mir_decl_header_ast_field_count(ASTNode *ast);
bool mir_decl_header_ast_shape(const MIRDeclHeader *header,
                               const char **name_out,
                               size_t *generic_count_out,
                               size_t *method_count_out,
                               size_t *field_count_out,
                               bool *uses_pointer_self_out);

#endif /* PERGYRA_MIR_DECL_HEADER_SHAPE_H */
