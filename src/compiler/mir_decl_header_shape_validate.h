#ifndef PGY_MIR_DECL_HEADER_SHAPE_VALIDATE_H
#define PGY_MIR_DECL_HEADER_SHAPE_VALIDATE_H

#include "mir.h"

bool mir_validate_decl_header_shape_metadata(const MIRDeclHeader *header,
                                             size_t header_index,
                                             char **error_message);

#endif /* PGY_MIR_DECL_HEADER_SHAPE_VALIDATE_H */
