/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_LLVM_RUNNER_H
#define PGY_LLVM_RUNNER_H

#include "driver_app.h"
#include "hir.h"

/* Run the LLVM backend pipeline: HIR → LLVM IR → object → binary.
 * Returns 0 on success. */
int llvm_runner_execute(const DriverFlags *flags, const HIRProgram *hir);

#endif /* PGY_LLVM_RUNNER_H */
