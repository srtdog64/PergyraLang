#ifndef PGY_MIR_DECL_HEADER_METHODS_H
#define PGY_MIR_DECL_HEADER_METHODS_H

#include "mir.h"

bool mir_decl_header_set_methods(MIRDeclHeader *header,
                                 ASTNode **methods,
                                 size_t method_count);
bool mir_decl_header_set_role_impl_methods(MIRDeclHeader *header,
                                           ASTNode *role_decl);

#endif /* PGY_MIR_DECL_HEADER_METHODS_H */
