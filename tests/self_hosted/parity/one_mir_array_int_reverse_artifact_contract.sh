#!/usr/bin/env bash
# Both targets materialize a fresh four-field result and copy source values.

require_text "$WORK_DIR/base.c" \
    'int32_t pgy_array_int_storage[3] = {3, 1, 2};'
require_text "$WORK_DIR/base.c" \
    'int32_t pgy_array_int_reverse_storage[3];'
require_text "$WORK_DIR/base.c" \
    'PgyArray_Int pgy_array_int_reverse_collection = {pgy_array_int_reverse_storage, 0, 3, NULL};'
for pair in '0] = pgy_array_int_collection.data[2' \
    '1] = pgy_array_int_collection.data[1' \
    '2] = pgy_array_int_collection.data[0'; do
    require_text "$WORK_DIR/base.c" \
        "pgy_array_int_reverse_collection.data[$pair]"
done
require_text "$WORK_DIR/base.c" \
    'pgy_array_int_reverse_collection.length = pgy_array_int_collection.length;'
reject_text "$WORK_DIR/base.c" 'pgy_ai_reverse'
reject_text "$WORK_DIR/base.c" 'realloc'
reject_text "$WORK_DIR/base.c" 'pgy_array_int_reverse_storage[3] = {'
! grep -Eq '^[[:space:]]*pgy_array_int_collection\.data\[[^]]+\][[:space:]]*=' \
    "$WORK_DIR/base.c" || fail "C reverse mutated source storage"

require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.storage = alloca [3 x i32], align 4'
require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.reverse.storage = alloca [3 x i32], align 4'
require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.reverse.object = alloca { ptr, i64, i64, ptr }, align 8'
for row in 0 1 2; do
    require_text "$WORK_DIR/base.ll" \
        "%pgy.op.0.reverse.$row.source = load i32"
    require_text "$WORK_DIR/base.ll" \
        "store i32 %pgy.op.0.reverse.$row.source, ptr %pgy.op.0.reverse.$row.result.ptr"
done
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.0.reverse.length = load i64, ptr %pgy.array.int.length.field'
require_text "$WORK_DIR/base.ll" \
    'store i64 %pgy.op.0.reverse.length, ptr %pgy.array.int.reverse.length.field'
reject_text "$WORK_DIR/base.ll" 'pgy_ai_reverse'
reject_text "$WORK_DIR/base.ll" 'realloc'
reject_text "$WORK_DIR/base.ll" 'insertvalue { ptr, i64, i64, ptr }'
reject_text "$WORK_DIR/base.ll" 'extractvalue { ptr, i64, i64, ptr }'
! grep -Eq 'store i32 .*%pgy\.op\.0\.reverse\.[0-9]+\.source\.ptr' \
    "$WORK_DIR/base.ll" || fail "LLVM reverse mutated source storage"

require_text "$WORK_DIR/alternate.c" \
    'int32_t pgy_array_int_storage[3] = {4, -5, 6};'
require_text "$WORK_DIR/alternate.ll" \
    'store i32 -5, ptr %pgy.array.int.init.1, align 4'
