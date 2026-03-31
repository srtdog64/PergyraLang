/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifndef PGY_C_RUNNER_H
#define PGY_C_RUNNER_H

#include "driver_app.h"
#include "hir.h"

/* Run the C backend pipeline: HIR → C source → GCC → binary.
 * Returns 0 on success. */
int c_runner_execute(const DriverFlags *flags, const HIRProgram *hir);

#endif /* PGY_C_RUNNER_H */
