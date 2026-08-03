#!/usr/bin/env bash
# Both targets consume one initialized mutable four-field Array<Int> receipt.

require_text "$WORK_DIR/base.c" '} PgyArray_Int;'
require_text "$WORK_DIR/base.c" \
    'int32_t pgy_array_int_storage[3] = {10, 20, 30};'
require_text "$WORK_DIR/base.c" \
    'PgyArray_Int pgy_array_int_collection = {pgy_array_int_storage, 3, 3, NULL};'
require_text "$WORK_DIR/base.c" \
    'if (((size_t)pgy_local_1) < pgy_array_int_collection.length)'
require_text "$WORK_DIR/base.c" \
    'pgy_local_0 + pgy_array_int_collection.data[(size_t)pgy_local_1]'
require_text "$WORK_DIR/base.c" \
    'pgy_array_int_collection.data[1] = (int32_t)(99);'
require_order "$WORK_DIR/base.c" \
    'pgy_array_int_collection.data[1] = (int32_t)(99);' \
    '(long long)pgy_array_int_collection.data[1]'
require_order "$WORK_DIR/base.c" \
    '(long long)pgy_array_int_collection.data[1]' \
    '(long long)pgy_array_int_collection.length'
reject_text "$WORK_DIR/base.c" 'pgy_ai'
reject_text "$WORK_DIR/base.c" 'pgy_array_set_Int'
reject_text "$WORK_DIR/base.c" 'realloc'
reject_text "$WORK_DIR/base.c" '< pgy_array_int_collection.capacity'

require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.storage = alloca [3 x i32], align 4'
require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.object = alloca { ptr, i64, i64, ptr }, align 8'
for row in 0 1 2; do
    require_text "$WORK_DIR/base.ll" "%pgy.array.int.init.$row = getelementptr"
done
require_text "$WORK_DIR/base.ll" \
    'store i64 3, ptr %pgy.array.int.length.field, align 8'
require_text "$WORK_DIR/base.ll" \
    '%pgy.cond.1.length = load i64, ptr %pgy.array.int.length.field'
require_text "$WORK_DIR/base.ll" '.read.i64 = sext i32 '
require_order "$WORK_DIR/base.ll" \
    'store i32 99, ptr %pgy.op.7.set.ptr' \
    '%pgy.op.8.read.i32 = load i32'
require_order "$WORK_DIR/base.ll" \
    '%pgy.op.8.read.i64 = sext i32' \
    '%pgy.op.9.length = load i64, ptr %pgy.array.int.length.field'
reject_text "$WORK_DIR/base.ll" 'pgy_array_set_Int'
reject_text "$WORK_DIR/base.ll" 'extractvalue { ptr, i64, i64, ptr }'
reject_text "$WORK_DIR/base.ll" 'insertvalue { ptr, i64, i64, ptr }'
reject_text "$WORK_DIR/base.ll" \
    'load i64, ptr %pgy.array.int.capacity.field'

require_text "$WORK_DIR/array-values.c" \
    'int32_t pgy_array_int_storage[3] = {4, 5, 6};'
require_text "$WORK_DIR/set-value-42.c" \
    'pgy_array_int_collection.data[1] = (int32_t)(42);'
require_text "$WORK_DIR/set-index-two.ll" \
    '%pgy.op.7.set.ptr = getelementptr inbounds i32, ptr %pgy.array.int.data, i64 2'
require_text "$WORK_DIR/post-read-index-two.ll" \
    '%pgy.op.8.read.ptr = getelementptr inbounds i32, ptr %pgy.array.int.data, i64 2'
