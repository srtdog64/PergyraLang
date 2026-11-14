#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H

#include "transpiler_mir_ssa_map.h"

bool transpiler_mir_seed_block_phi_names(const MIRBasicBlock *block,
                                         TranspilerSSANameMap *ssa_map_out);
bool transpiler_mir_inst_is_cfg_container(const MIRInstruction *inst);
bool transpiler_mir_seed_pin_view_alias(const MIRBasicBlock *block,
                                        TranspilerSSANameMap *ssa_map_out);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H */
