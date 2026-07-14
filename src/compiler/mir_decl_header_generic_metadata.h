#ifndef PGY_MIR_DECL_HEADER_GENERIC_METADATA_H
#define PGY_MIR_DECL_HEADER_GENERIC_METADATA_H

#include "mir.h"

bool mir_decl_header_set_generics(MIRDeclHeader *header, ASTNode *decl);
void mir_decl_header_free_generics(MIRDeclHeader *header);

#endif /* PGY_MIR_DECL_HEADER_GENERIC_METADATA_H */
