#ifndef PGY_MIR_DECL_HEADER_ZONE_STATE_VALIDATE_H
#define PGY_MIR_DECL_HEADER_ZONE_STATE_VALIDATE_H

#include "mir_decl_headers.h"

#include <stdbool.h>
#include <stddef.h>

bool mir_decl_header_validate_zone_states(const MIRDeclHeader *header,
                                          size_t header_index,
                                          char **error_message);

#endif /* PGY_MIR_DECL_HEADER_ZONE_STATE_VALIDATE_H */
