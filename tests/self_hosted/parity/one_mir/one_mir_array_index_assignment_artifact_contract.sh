#!/usr/bin/env bash
# Both backends execute Set-before-Get from one legalized collection plan.

require_text "$WORK_DIR/base.c" \
    'int32_t pgy_collection_storage_0[3] = {1, 2, 3};'
require_text "$WORK_DIR/base.c" \
    'pgy_collection_0.data[1] = (int32_t)(9);'
require_text "$WORK_DIR/base.c" \
    'pgy_collection_0.data[0] + pgy_collection_0.data[1] + pgy_collection_0.data[2]'
require_text "$WORK_DIR/base.c" \
    'char * pgy_collection_storage_2[2] = {"a", "b"};'
require_text "$WORK_DIR/base.c" 'pgy_collection_2.data[0] = "z";'
require_text "$WORK_DIR/base.c" \
    'pgy_concat(pgy_collection_2.data[0], pgy_collection_2.data[1])'
reject_text "$WORK_DIR/base.c" 'pgy_ai_set'
reject_text "$WORK_DIR/base.c" 'pgy_as_set'
reject_text "$WORK_DIR/base.c" '"zb"'
reject_text "$WORK_DIR/base.c" '(long long)(13)'

require_text "$WORK_DIR/base.ll" \
    '%pgy.array.collection.0.object = alloca { ptr, i64, i64, ptr }, align 8'
require_text "$WORK_DIR/base.ll" \
    'store i32 9, ptr %pgy.op.0.set.ptr, align 4'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.1.get.1.value = load i32, ptr %pgy.op.1.get.1.ptr, align 4'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.1.sum.2 = add i32 %pgy.op.1.sum.1, %pgy.op.1.get.2.value'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.1.sum.i64 = sext i32 %pgy.op.1.sum.2 to i64'
require_text "$WORK_DIR/base.ll" \
    '%pgy.array.collection.2.object = alloca { ptr, i64, i64, ptr }, align 8'
require_text "$WORK_DIR/base.ll" \
    'store ptr @.pgy.collection.set.6, ptr %pgy.op.2.set.ptr, align 8'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.3.concat = call ptr @pgy_concat(ptr %pgy.op.3.get.0.value, ptr %pgy.op.3.get.1.value)'
reject_text "$WORK_DIR/base.ll" 'pgy_ai_set'
reject_text "$WORK_DIR/base.ll" 'pgy_as_set'
reject_text "$WORK_DIR/base.ll" 'i64 13)'

[[ "$(grep -Fc 'pgy_collection_storage_' "$WORK_DIR/base.c")" -eq 4 ]] || \
    fail "C storage/object references do not resolve to exactly two roots"
[[ "$(grep -Fc '.storage = alloca' "$WORK_DIR/base.ll")" -eq 2 ]] || \
    fail "LLVM materialized more than two collection storage roots"
