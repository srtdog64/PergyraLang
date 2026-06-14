#ifndef PERGYRA_MIR_DECL_HEADER_FIELDS_H
#define PERGYRA_MIR_DECL_HEADER_FIELDS_H

#include "mir.h"

bool mir_decl_header_set_fields(MIRDeclHeader *header, ASTNode *decl);
void mir_decl_header_free_fields(MIRDeclHeader *header);

#endif /* PERGYRA_MIR_DECL_HEADER_FIELDS_H */
