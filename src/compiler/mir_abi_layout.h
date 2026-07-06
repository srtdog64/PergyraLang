#ifndef PGY_MIR_ABI_LAYOUT_H
#define PGY_MIR_ABI_LAYOUT_H

#include "mir.h"

const MIRTypeLayout *mir_abi_lookup(const char *pergyra_type_name);
const char *mir_abi_resource_runtime_fn(const MIRTypeLayout *layout,
                                        const char *resource_op_name);
void mir_abi_table_init(void);

#endif /* PGY_MIR_ABI_LAYOUT_H */
