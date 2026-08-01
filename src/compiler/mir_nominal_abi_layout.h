#ifndef PERGYRA_MIR_NOMINAL_ABI_LAYOUT_H
#define PERGYRA_MIR_NOMINAL_ABI_LAYOUT_H

#include "mir.h"

/* Capture complete physical rows only for value structs whose fields are in
 * the currently admitted fixed scalar/nested-value subset. Unsupported or
 * cyclic declarations remain explicitly without a row. */
bool mir_nominal_abi_layouts_capture(MIRProgram *program,
                                     char **error_message);
const MIRTypeLayout *mir_decl_header_abi_layout(
    const MIRDeclHeader *header);

#endif /* PERGYRA_MIR_NOMINAL_ABI_LAYOUT_H */
