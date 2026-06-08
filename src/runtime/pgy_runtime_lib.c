/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime_lib.c -- Non-inline runtime symbols for LLVM linking
 */

#ifdef PGY_LLVM_ENABLED
#define PGY_RUNTIME_LIB_INTERNAL

#include "pgy_runtime_lib_authority_file_core.h"
#include "pgy_runtime_lib_core_exports.h"
#include "pgy_runtime_lib_list_raw_exports.h"
#include "pgy_runtime_lib_raw_collection_exports.h"
#include "pgy_runtime_lib_set_intent_trace_exports.c"
#include "pgy_runtime_lib_mir_trace_exports.c"
#include "pgy_runtime_lib_intent_exports.h"
#include "pgy_runtime_lib_intent_slot_core_exports.h"
#include "pgy_runtime_lib_slot_exports.h"
#include "pgy_runtime_lib_slot_array_io_string_exports.h"
#include "pgy_runtime_lib_std_exports.h"
#include "pgy_runtime_lib_channel_quantum_exports.h"
#include "pgy_runtime_lib_quantum_exports.h"

#endif /* PGY_LLVM_ENABLED */
