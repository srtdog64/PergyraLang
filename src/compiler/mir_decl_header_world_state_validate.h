#ifndef PGY_MIR_DECL_HEADER_WORLD_STATE_VALIDATE_H
#define PGY_MIR_DECL_HEADER_WORLD_STATE_VALIDATE_H

#include "mir_decl.h"

bool mir_decl_header_validate_world_states(const MIRDeclHeader *header,
                                           size_t header_index,
                                           char **error_message);

#endif /* PGY_MIR_DECL_HEADER_WORLD_STATE_VALIDATE_H */
