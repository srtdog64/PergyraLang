# Compatibility hook for the cumulative CFG integration gate.
# Behavioral ownership moved to the general scalar CFG break-exit gate.

BREAK_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_scalar_cfg_break_exit_projection.sh"
require_file "$BREAK_GATE"
PGY_SELFHOST_ONE_MIR_DRIVER_BIN="$DRIVER_BIN" \
    PGY_SELF_DRIVER_BIN="$DRIVER_BIN" \
    bash "$BREAK_GATE" || fail "general scalar CFG break-exit gate failed"

echo "[$LABEL] break_after_stmt is owned by the general scalar CFG gate"
