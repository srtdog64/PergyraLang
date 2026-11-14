#ifndef PGY_MIR_DECL_HEADER_AUTHORITY_H
#define PGY_MIR_DECL_HEADER_AUTHORITY_H

#include "mir_decl_headers.h"

bool mir_decl_header_set_authorities(MIRDeclHeader *header, ASTNode *decl);
void mir_decl_header_free_authorities(MIRDeclHeader *header);

#endif /* PGY_MIR_DECL_HEADER_AUTHORITY_H */
