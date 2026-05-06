#ifndef PERGYRA_MIR_CFG_CONTRACT_PIN_H
#define PERGYRA_MIR_CFG_CONTRACT_PIN_H

#include "mir.h"

const MIRInstruction *mir_block_find_pin_cleanup_edge_fact(
    const MIRBasicBlock *block);
bool mir_block_has_pin_cleanup_edge(const MIRBasicBlock *block);
const char *mir_block_pin_cleanup_missing_reason(const MIRBasicBlock *block);

#endif
