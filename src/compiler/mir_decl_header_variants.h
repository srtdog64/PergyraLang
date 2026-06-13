#ifndef PERGYRA_MIR_DECL_HEADER_VARIANTS_H
#define PERGYRA_MIR_DECL_HEADER_VARIANTS_H

#include "mir.h"

bool mir_decl_header_set_variants(MIRDeclHeader *header, ASTNode *decl);
void mir_decl_header_free_variants(MIRDeclHeader *header);

#endif /* PERGYRA_MIR_DECL_HEADER_VARIANTS_H */
