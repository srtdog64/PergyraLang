#!/usr/bin/env bash
# Target artifacts must consume one mutable four-field Array<Int> receipt.

require_text "$WORK_DIR/base.c" '} PgyArray_Int;'
require_text "$WORK_DIR/base.c" 'int32_t pgy_array_int_storage[5] = {0};'
require_text "$WORK_DIR/base.c" \
    'PgyArray_Int pgy_array_int_collection = {pgy_array_int_storage, 0, 5, NULL};'
require_text "$WORK_DIR/base.c" \
    'data[pgy_array_int_collection.length] = (int32_t)(pgy_local_0 * pgy_local_0);'
require_order "$WORK_DIR/base.c" \
    'data[pgy_array_int_collection.length] = ' \
    'pgy_array_int_collection.length = pgy_array_int_collection.length + 1;'
require_text "$WORK_DIR/base.c" \
    'if (((size_t)pgy_local_2) < pgy_array_int_collection.length)'
require_text "$WORK_DIR/base.c" \
    'pgy_array_int_collection.data[(size_t)pgy_local_2]'
require_text "$WORK_DIR/base.c" \
    'printf("%lld\n", (long long)pgy_array_int_collection.length);'
reject_text "$WORK_DIR/base.c" 'pgy_ai'
reject_text "$WORK_DIR/base.c" 'pgy_array_push_Int'
reject_text "$WORK_DIR/base.c" 'realloc'
reject_text "$WORK_DIR/base.c" '< pgy_array_int_collection.capacity'

require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.storage = alloca [5 x i32], align 4'
require_text "$WORK_DIR/base.ll" \
    '%pgy.array.int.object = alloca { ptr, i64, i64, ptr }, align 8'
require_text "$WORK_DIR/base.ll" '.push.computed = mul i64 '
require_text "$WORK_DIR/base.ll" '.push.element = trunc i64 '
require_order "$WORK_DIR/base.ll" 'store i32 %pgy.op.2.push.element' \
    'store i64 %pgy.op.2.push.next, ptr %pgy.array.int.length.field'
require_text "$WORK_DIR/base.ll" '.read.i64 = sext i32 '
require_text "$WORK_DIR/base.ll" \
    '%pgy.cond.4.length = load i64, ptr %pgy.array.int.length.field'
require_text "$WORK_DIR/base.ll" \
    '@printf(ptr @.pgy.scalar.cfg.int.format, i64 %pgy.op.11.length)'
reject_text "$WORK_DIR/base.ll" 'pgy_ai_push'
reject_text "$WORK_DIR/base.ll" 'pgy_array_push_Int'
reject_text "$WORK_DIR/base.ll" 'extractvalue { ptr, i64, i64, ptr }'
reject_text "$WORK_DIR/base.ll" 'insertvalue { ptr, i64, i64, ptr }'
reject_text "$WORK_DIR/base.ll" \
    'load i64, ptr %pgy.array.int.capacity.field'

require_text "$WORK_DIR/value-add.c" \
    '(int32_t)(pgy_local_0 + pgy_local_0);'
require_text "$WORK_DIR/value-add.ll" '.push.computed = add i64 '
require_text "$WORK_DIR/bound-three.c" \
    'int32_t pgy_array_int_storage[3] = {0};'
require_text "$WORK_DIR/bound-three.ll" \
    '%pgy.array.int.storage = alloca [3 x i32], align 4'
