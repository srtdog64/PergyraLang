# Compatibility hook for the cumulative CFG integration gate.
# Behavioral ownership moved to the general scalar CFG break-exit gate.

BREAK_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_scalar_cfg_break_exit_projection.sh"
require_file "$BREAK_GATE"
PGY_SELFHOST_ONE_MIR_DRIVER_BIN="$DRIVER_BIN" \
    PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$BREAK_GATE" || fail "general scalar CFG break-exit gate failed"

echo "[$LABEL] break_after_stmt is owned by the general scalar CFG gate"

FOR_BREAK_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_scalar_cfg_for_break_exit_projection.sh"
require_file "$FOR_BREAK_GATE"
PGY_SELFHOST_ONE_MIR_DRIVER_BIN="$DRIVER_BIN" \
    PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$FOR_BREAK_GATE" || fail "general scalar CFG for-break exit gate failed"

echo "[$LABEL] for_break_exit is owned by the general scalar CFG gate"

CONTINUE_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_scalar_cfg_continue_backedge_projection.sh"
require_file "$CONTINUE_GATE"
PGY_SELFHOST_ONE_MIR_DRIVER_BIN="$DRIVER_BIN" \
    PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$CONTINUE_GATE" || fail "general scalar CFG continue-backedge gate failed"

echo "[$LABEL] break_continue is owned by producer backedge snapshots"

PROGRAM_TRANSFER_GATE="$ROOT_DIR/tests/self_hosted/parity/direct_mir/direct_mir_scalar_program_control_transfer_owner.sh"
require_file "$PROGRAM_TRANSFER_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$PROGRAM_TRANSFER_GATE" || fail "program control-transfer gate failed"

echo "[$LABEL] program GraphPlan consumes one shared break/continue fact"

ITERATION_SCOPE_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_iteration_binding_scope_owner.sh"
require_file "$ITERATION_SCOPE_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$ITERATION_SCOPE_GATE" || fail "iteration binding scope gate failed"

echo "[$LABEL] iteration binding shadow substitutes through one LocalRef plan"

NESTED_SCOPE_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_nested_iteration_binding_scope_owner.sh"
require_file "$NESTED_SCOPE_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$NESTED_SCOPE_GATE" || fail "nested iteration binding scope gate failed"

echo "[$LABEL] nested range binders substitute through one canonical receipt set"

NESTED_CONTINUE_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_nested_iteration_continue_scope_owner.sh"
require_file "$NESTED_CONTINUE_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$NESTED_CONTINUE_GATE" || fail "nested iteration continue scope gate failed"

echo "[$LABEL] nested continue/fallthrough transfers stay with their innermost range"

STRING_ARRAY_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_string_array_mutation_projection.sh"
require_file "$STRING_ARRAY_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$STRING_ARRAY_GATE" || fail "String-array mutation receipt gate failed"

echo "[$LABEL] while read and bounded static set share one String-array receipt"

ARRAY_INDEX_ASSIGNMENT_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir/one_mir_array_index_assignment_projection.sh"
require_file "$ARRAY_INDEX_ASSIGNMENT_GATE"
PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$ARRAY_INDEX_ASSIGNMENT_GATE" || \
    fail "bounded collection operation legalization gate failed"

echo "[$LABEL] indexed assignments legalize as CollectionValue/Get/Set rows"
