#ifndef PERGYRA_MIR_DECL_METHOD_PROJECTION_H
#define PERGYRA_MIR_DECL_METHOD_PROJECTION_H

#include "mir_decl.h"

void mir_decl_method_projection_metadata_clear(MIRDeclMethod *meta);
bool mir_decl_method_projection_metadata_capture(MIRDeclMethod *meta,
                                                 ASTNode *method_body);

#endif
