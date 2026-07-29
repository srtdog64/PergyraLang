/*
 * Copyright (c) 2025 Pergyra Language Project
 * LLVM target-machine lifecycle owner declarations.
 */

#ifndef PERGYRA_LLVM_TARGET_MACHINE_INTERNAL_H
#define PERGYRA_LLVM_TARGET_MACHINE_INTERNAL_H

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

LLVMTargetMachineRef llvm_create_host_machine(char **triple_out,
                                              char **cpu_out,
                                              char **features_out);
void llvm_apply_target_machine(LLVMGenCtx *ctx,
                               LLVMTargetMachineRef machine,
                               const char *triple);

#endif /* PGY_LLVM_ENABLED */

#endif /* PERGYRA_LLVM_TARGET_MACHINE_INTERNAL_H */
