/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime.h — Hybrid memory model runtime for Pergyra
 */

#ifndef PGY_RUNTIME_H
#define PGY_RUNTIME_H

#include "pgy_runtime_platform_io_core.h"
#include "pgy_runtime_inline_core.h"
/* Channel waits need the pool compensation hook at definition time. Keep the
 * pool owner before the channel inline bodies; including pgy_parallel.h only
 * after pgy_runtime.h silently compiles PGY_CHANNEL_BLOCKED_TICK as a no-op. */
#include "pgy_parallel.h"
#include "pgy_runtime_channel_inline.h"
#include "pgy_runtime_io_qubit_inline.h"
#include "pgy_runtime_string_window_inline.h"
#include "pgy_runtime_media_stub.h"

#endif /* PGY_RUNTIME_H */
