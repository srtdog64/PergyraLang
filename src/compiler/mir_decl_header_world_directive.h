#ifndef PERGYRA_MIR_DECL_HEADER_WORLD_DIRECTIVE_H
#define PERGYRA_MIR_DECL_HEADER_WORLD_DIRECTIVE_H

#include "mir_decl.h"

bool mir_decl_header_set_world_directives(MIRDeclHeader *header,
                                           ASTNode *decl);
void mir_decl_header_free_world_directives(MIRDeclHeader *header);

#endif /* PERGYRA_MIR_DECL_HEADER_WORLD_DIRECTIVE_H */
