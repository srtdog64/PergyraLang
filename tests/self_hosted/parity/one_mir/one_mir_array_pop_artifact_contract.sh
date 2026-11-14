#!/usr/bin/env bash
# Both targets mutate only live length and preserve full backing storage.

require_text "$WORK_DIR/base.c" \
    'int32_t pgy_foreach_storage_0[4] = {10, 20, 30, 40};'
require_text "$WORK_DIR/base.c" \
    'pgy_foreach_collection_0.length = pgy_foreach_collection_0.length - 1;'
[[ "$(grep -Fc 'pgy_foreach_collection_0.length = pgy_foreach_collection_0.length - 1;' "$WORK_DIR/base.c")" -eq 2 ]] || \
    fail "C did not emit exactly two Int live-length decrements"
require_text "$WORK_DIR/base.c" \
    'char * pgy_indexed_storage_0[3] = {"a", "b", "c"};'
require_text "$WORK_DIR/base.c" \
    'pgy_indexed_collection_0.length = pgy_indexed_collection_0.length - 1;'
reject_text "$WORK_DIR/base.c" 'pgy_ai_pop'
reject_text "$WORK_DIR/base.c" 'pgy_as_pop'
reject_text "$WORK_DIR/base.c" '.capacity = .length'

require_text "$WORK_DIR/base.ll" \
    '%pgy.foreach.collection.0.object = alloca { ptr, i64, i64, ptr }, align 8'
require_text "$WORK_DIR/base.ll" \
    '%pgy.foreach.collection.0.length.field = getelementptr inbounds { ptr, i64, i64, ptr }'
for operation in 0 1; do
    require_text "$WORK_DIR/base.ll" \
        "%pgy.op.$operation.pop.length = load i64, ptr %pgy.foreach.collection.0.length.field"
    require_text "$WORK_DIR/base.ll" \
        "store i64 %pgy.op.$operation.pop.next, ptr %pgy.foreach.collection.0.length.field"
done
require_text "$WORK_DIR/base.ll" \
    '%pgy.cond.1.length = load i64, ptr %pgy.foreach.collection.0.length.field'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.6.length = load i64, ptr %pgy.foreach.collection.0.length.field'
require_text "$WORK_DIR/base.ll" \
    '%pgy.op.7.pop.length = load i64, ptr %pgy.array.indexed.0.length.field'
require_text "$WORK_DIR/base.ll" \
    'store i64 %pgy.op.7.pop.next, ptr %pgy.array.indexed.0.length.field'
reject_text "$WORK_DIR/base.ll" 'pgy_ai_pop'
reject_text "$WORK_DIR/base.ll" 'pgy_as_pop'
reject_text "$WORK_DIR/base.ll" 'store i64 2, ptr %pgy.foreach.collection.0.length.field'

require_text "$WORK_DIR/popped-only.c" \
    'int32_t pgy_foreach_storage_0[4] = {10, 20, -7, -8};'
require_text "$WORK_DIR/popped-only.c" \
    'char * pgy_indexed_storage_0[3] = {"a", "b", "removed"};'
