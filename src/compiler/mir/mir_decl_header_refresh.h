#ifndef PGY_MIR_DECL_HEADER_REFRESH_H
#define PGY_MIR_DECL_HEADER_REFRESH_H

#include "mir_decl_headers.h"

bool mir_decl_header_set_refreshes(MIRDeclHeader *header, ASTNode *decl);
void mir_decl_header_free_refreshes(MIRDeclHeader *header);

#endif /* PGY_MIR_DECL_HEADER_REFRESH_H */
