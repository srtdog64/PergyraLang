/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR pin-region emission helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_PIN_EMIT_H
#define PERGYRA_TRANSPILER_MIR_PIN_EMIT_H

#include "transpiler.h"
#include "transpiler_mir_ssa_map.h"

bool transpiler_emit_mir_pin_enter_local(CodeBuf *buf,
                                         TranspilerCtx *ctx,
                                         const MIRBasicBlock *block,
                                         char *reason,
                                         size_t reason_cap);
bool transpiler_emit_mir_pin_exit_local(CodeBuf *buf,
                                        TranspilerCtx *ctx,
                                        const MIRBasicBlock *block,
                                        char *reason,
                                        size_t reason_cap);
bool transpiler_mir_block_has_local_def_for_anchor(const MIRBasicBlock *block,
                                                   const char *anchor);
bool transpiler_mir_seed_resource_alias_local(TranspilerSSANameMap *ssa_map,
                                              const MIRInstruction *inst);

#endif /* PERGYRA_TRANSPILER_MIR_PIN_EMIT_H */
