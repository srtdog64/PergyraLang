/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_LLVM_RUNNER_H
#define PGY_LLVM_RUNNER_H

#include "driver_app.h"
#include "compiler.h"

/* Run the LLVM backend pipeline: IR bundle + verified AIR handle → LLVM IR → object → binary.
 * Returns 0 on success. */
int llvm_runner_execute(const DriverFlags *flags,
                        const CompilerIRBundle *bundle,
                        const PgyAirVerification *air,
                        CompilerBackendTimings *backend_timings);

#endif /* PGY_LLVM_RUNNER_H */
