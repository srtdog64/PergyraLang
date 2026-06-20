#ifndef PGY_MIR_DECL_HEADER_ZONE_STATE_H
#define PGY_MIR_DECL_HEADER_ZONE_STATE_H

#include "mir_decl_headers.h"

bool mir_decl_header_set_zone_states(MIRDeclHeader *header, ASTNode *decl);
void mir_decl_header_free_zone_states(MIRDeclHeader *header);

#endif /* PGY_MIR_DECL_HEADER_ZONE_STATE_H */
