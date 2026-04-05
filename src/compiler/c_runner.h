/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_C_RUNNER_H
#define PGY_C_RUNNER_H

#include "driver_app.h"
#include "compiler.h"

/* Run the C backend pipeline: IR bundle → C source → GCC → binary.
 * Returns 0 on success. */
int c_runner_execute(const DriverFlags *flags, const CompilerIRBundle *bundle);

#endif /* PGY_C_RUNNER_H */
