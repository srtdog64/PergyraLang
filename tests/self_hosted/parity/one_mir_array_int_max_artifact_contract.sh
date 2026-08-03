#!/usr/bin/env bash
# Both targets consume one read-only current-length Array<Int> receipt.

require_text "$WORK_DIR/base.c" '} PgyArray_Int;'
require_text "$WORK_DIR/base.c" \
    'int32_t pgy_array_int_storage[5] = {3, 7, 2, 9, 4};'
require_text "$WORK_DIR/base.c" \
    'PgyArray_Int pgy_array_int_collection = {pgy_array_int_storage, 5, 5, NULL};'
require_text "$WORK_DIR/base.c" \
    'pgy_local_0 = pgy_array_int_collection.data[(size_t)0];'
require_text "$WORK_DIR/base.c" \
    'if (((size_t)pgy_local_1) < pgy_array_int_collection.length)'
require_text "$WORK_DIR/base.c" \
    'pgy_array_int_collection.data[(size_t)pgy_local_1] > pgy_local_0'
require_text "$WORK_DIR/base.c" \
    'pgy_local_0 = pgy_array_int_collection.data[(size_t)pgy_local_1];'
require_text "$WORK_DIR/base.c" \
    'printf("%lld\n", (long long)pgy_local_0);'
reject_text "$WORK_DIR/base.c" '< pgy_array_int_collection.capacity'
reject_text "$WORK_DIR/base.c" 'pgy_array_get_Int'
reject_text "$WORK_DIR/base.c" 'pgy_array_set_Int'
reject_text "$WORK_DIR/base.c" 'realloc'
reject_text "$WORK_DIR/base.c" '(long long)9);'

require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.storage = alloca [5 x i32], align 4'
require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.object = alloca { ptr, i64, i64, ptr }, align 8'
for row in 0 1 2 3 4; do
    require_text "$WORK_DIR/base.ll" "%pgy.array.int.init.$row = getelementptr"
done
require_text "$WORK_DIR/base.ll" \
    'store i64 5, ptr %pgy.array.int.length.field, align 8'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.0.read.i64 = sext i32 %pgy.op.0.read.i32 to i64'
require_text "$WORK_DIR/base.ll" \
    '%pgy.cond.1.length = load i64, ptr %pgy.array.int.length.field'
require_text "$WORK_DIR/base.ll" \
    '%pgy.cond.2 = icmp sgt i64 %pgy.cond.2.element, %pgy.cond.2.current'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.2.read.i64 = sext i32 %pgy.op.2.read.i32 to i64'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.4.log = load i64, ptr %pgy.local.0, align 8'
reject_text "$WORK_DIR/base.ll" 'pgy_array_get_Int'
reject_text "$WORK_DIR/base.ll" 'pgy_array_set_Int'
reject_text "$WORK_DIR/base.ll" 'extractvalue { ptr, i64, i64, ptr }'
reject_text "$WORK_DIR/base.ll" 'insertvalue { ptr, i64, i64, ptr }'
reject_text "$WORK_DIR/base.ll" \
    'load i64, ptr %pgy.array.int.capacity.field'

require_text "$WORK_DIR/first-max.c" \
    'int32_t pgy_array_int_storage[5] = {12, 7, 2, 9, 4};'
require_text "$WORK_DIR/last-max.ll" 'store i32 14, ptr %pgy.array.int.init.4'
require_text "$WORK_DIR/all-negative.c" \
    'int32_t pgy_array_int_storage[5] = {-8, -3, -11, -2, -6};'
