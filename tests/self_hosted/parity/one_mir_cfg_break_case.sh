# Compatibility hook for the cumulative CFG integration gate.
# Behavioral ownership moved to the general scalar CFG break-exit gate.

BREAK_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_scalar_cfg_break_exit_projection.sh"
require_file "$BREAK_GATE"
PGY_SELFHOST_ONE_MIR_DRIVER_BIN="$DRIVER_BIN" \
    PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$BREAK_GATE" || fail "general scalar CFG break-exit gate failed"

echo "[$LABEL] break_after_stmt is owned by the general scalar CFG gate"

FOR_BREAK_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_scalar_cfg_for_break_exit_projection.sh"
require_file "$FOR_BREAK_GATE"
PGY_SELFHOST_ONE_MIR_DRIVER_BIN="$DRIVER_BIN" \
    PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$FOR_BREAK_GATE" || fail "general scalar CFG for-break exit gate failed"

echo "[$LABEL] for_break_exit is owned by the general scalar CFG gate"

CONTINUE_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_scalar_cfg_continue_backedge_projection.sh"
require_file "$CONTINUE_GATE"
PGY_SELFHOST_ONE_MIR_DRIVER_BIN="$DRIVER_BIN" \
    PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$CONTINUE_GATE" || fail "general scalar CFG continue-backedge gate failed"

echo "[$LABEL] break_continue is owned by producer backedge snapshots"

ITERATION_SCOPE_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_iteration_binding_scope_owner.sh"
require_file "$ITERATION_SCOPE_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$ITERATION_SCOPE_GATE" || fail "iteration binding scope gate failed"

echo "[$LABEL] iteration binding shadow substitutes through one LocalRef plan"

NESTED_SCOPE_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_nested_iteration_binding_scope_owner.sh"
require_file "$NESTED_SCOPE_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$NESTED_SCOPE_GATE" || fail "nested iteration binding scope gate failed"

echo "[$LABEL] nested range binders substitute through one canonical receipt set"

NESTED_CONTINUE_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_nested_iteration_continue_scope_owner.sh"
require_file "$NESTED_CONTINUE_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$NESTED_CONTINUE_GATE" || fail "nested iteration continue scope gate failed"

echo "[$LABEL] nested continue/fallthrough transfers stay with their innermost range"

FOREACH_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_scalar_cfg_foreach_array_int_projection.sh"
require_file "$FOREACH_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$FOREACH_GATE" || fail "Array<Int> foreach receipt gate failed"

echo "[$LABEL] Array<Int> foreach substitutes through one collection receipt"

RETURNED_FOREACH_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_returned_array_foreach_projection.sh"
require_file "$RETURNED_FOREACH_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$RETURNED_FOREACH_GATE" || fail "returned Array<Int> foreach gate failed"

echo "[$LABEL] returned Array<Int> foreach composes through one producer receipt"

MIXED_FOREACH_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_mixed_collection_foreach_projection.sh"
require_file "$MIXED_FOREACH_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$MIXED_FOREACH_GATE" || fail "mixed Int/String foreach gate failed"

echo "[$LABEL] mixed Int/String foreach substitutes through one typed receipt"

INDEXED_STRING_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_indexed_string_array_projection.sh"
require_file "$INDEXED_STRING_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$INDEXED_STRING_GATE" || fail "indexed Array<String> gate failed"

echo "[$LABEL] ArrayLength-bounded parts[i] substitutes through one indexed receipt"

STRING_ARRAY_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_string_array_mutation_projection.sh"
require_file "$STRING_ARRAY_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$STRING_ARRAY_GATE" || fail "String-array mutation receipt gate failed"

echo "[$LABEL] while read and bounded static set share one String-array receipt"
