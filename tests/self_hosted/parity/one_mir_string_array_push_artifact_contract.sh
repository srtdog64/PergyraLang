#!/usr/bin/env bash
# Target artifacts must consume the mutable four-field String-array receipt.

C_ABI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_string_abi_projection_owner.pgy"
LLVM_IMMUTABLE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_llvm_materialization_owner.pgy"

require_text "$C_ABI_OWNER" 'return "PgyArray_String";'
for owner in "$LLVM_STORAGE" "$LLVM_MUTATION"; do
    reject_text "$owner" 'DirectMirScalarCfgArrayStringLlvmValue('
done
require_text "$LLVM_IMMUTABLE_OWNER" 'DirectMirScalarCfgArrayStringLlvmValue('

require_text "$WORK_DIR/base.c" '} PgyArray_String;'
require_text "$WORK_DIR/base.c" \
    'PgyArray_String pgy_indexed_collection_0 = {pgy_indexed_storage_0, 0, 3, NULL};'
require_text "$WORK_DIR/base.c" \
    'if (((size_t)pgy_local_1) < pgy_indexed_collection_0.length)'
require_text "$WORK_DIR/base.c" \
    'printf("%lld\n", (long long)pgy_indexed_collection_0.length);'
reject_text "$WORK_DIR/base.c" 'pgy_as'
reject_text "$WORK_DIR/base.c" 'pgy_indexed_collection_0.capacity'
require_order "$WORK_DIR/base.c" '.data[0] = "a"' '.length = 1'
require_order "$WORK_DIR/base.c" '.length = 1' '.data[1] = "bb"'
require_order "$WORK_DIR/base.c" '.data[1] = "bb"' '.length = 2'
require_order "$WORK_DIR/base.c" '.length = 2' '.data[2] = "ccc"'
require_order "$WORK_DIR/base.c" '.data[2] = "ccc"' '.length = 3'

require_text "$WORK_DIR/base.ll" \
    '%pgy.array.indexed.0.storage = alloca [3 x ptr]'
require_text "$WORK_DIR/base.ll" \
    'store i64 0, ptr %pgy.array.indexed.0.length.field'
require_order "$WORK_DIR/base.ll" \
    '@.pgy.scalar.cfg.indexed.push.0, ptr %pgy.op.0.push.slot' \
    'store i64 1, ptr %pgy.array.indexed.0.length.field'
require_order "$WORK_DIR/base.ll" \
    'store i64 1, ptr %pgy.array.indexed.0.length.field' \
    '@.pgy.scalar.cfg.indexed.push.1, ptr %pgy.op.1.push.slot'
require_order "$WORK_DIR/base.ll" \
    '@.pgy.scalar.cfg.indexed.push.1, ptr %pgy.op.1.push.slot' \
    'store i64 2, ptr %pgy.array.indexed.0.length.field'
require_order "$WORK_DIR/base.ll" \
    'store i64 2, ptr %pgy.array.indexed.0.length.field' \
    '@.pgy.scalar.cfg.indexed.push.2, ptr %pgy.op.2.push.slot'
require_order "$WORK_DIR/base.ll" \
    '@.pgy.scalar.cfg.indexed.push.2, ptr %pgy.op.2.push.slot' \
    'store i64 3, ptr %pgy.array.indexed.0.length.field'
require_text "$WORK_DIR/base.ll" \
    'icmp ult i64 %pgy.cond.1.index, %pgy.cond.1.length'
require_text "$WORK_DIR/base.ll" \
    '@printf(ptr @.pgy.scalar.cfg.int.format, i64 %pgy.op.10.length)'
reject_text "$WORK_DIR/base.ll" \
    'load i64, ptr %pgy.array.indexed.0.capacity.field'
reject_text "$WORK_DIR/base.ll" \
    'extractvalue { ptr, i64, i64, ptr } %pgy.array.indexed.'
reject_text "$WORK_DIR/base.ll" 'insertvalue { ptr, i64, i64, ptr }'
