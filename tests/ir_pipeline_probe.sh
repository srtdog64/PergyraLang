#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_PGY="$ROOT_DIR/bin/pgy"
TMP_PGY="/tmp/pgy-PergyraLang-bin/pgy"
if [[ -x "${DEFAULT_PGY}.exe" ]]; then
    DEFAULT_PGY="${DEFAULT_PGY}.exe"
fi
if [[ -x "${TMP_PGY}.exe" ]]; then
    TMP_PGY="${TMP_PGY}.exe"
fi
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY="$PGY_BIN"
elif [[ -x "$DEFAULT_PGY" ]]; then
    PGY="$DEFAULT_PGY"
elif [[ -x "$TMP_PGY" ]]; then
    PGY="$TMP_PGY"
else
    PGY="$DEFAULT_PGY"
fi

EXAMPLE="${1:-$ROOT_DIR/examples/logistics_intent_probe/main.pgy}"
BACKENDS="${PGY_EXAMPLE_BACKENDS:-c llvm}"
WORK_BASE="$ROOT_DIR/.tmp/ir-pipeline-probe"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

if [[ ! -f "$EXAMPLE" ]]; then
    echo "missing example source: $EXAMPLE" >&2
    exit 1
fi

DIR_OUT="$WORK_DIR/dir.txt"
RIR_OUT="$WORK_DIR/rir.txt"
MIR_OUT="$WORK_DIR/mir.txt"

"$PGY" "$EXAMPLE" --dir > "$DIR_OUT"
"$PGY" "$EXAMPLE" --rir > "$RIR_OUT"
"$PGY" "$EXAMPLE" --mir > "$MIR_OUT"

grep -Fq "role-complete" "$DIR_OUT"
grep -Fq "intent-step-zone" "$DIR_OUT"
grep -Fq "transfer=loading->delivery" "$DIR_OUT"
grep -Fq "zone-slot LoadingZone.courier" "$DIR_OUT"
grep -Fq "projection-slot LoadingZone.dispatchPacket" "$DIR_OUT"
grep -Fq "authority-slot LoadingZone.dispatcher" "$DIR_OUT"
grep -Fq "owner-has-projection-slot" "$DIR_OUT"
grep -Fq "authority-slot-subject" "$DIR_OUT"

grep -Fq "kind=WorldHandle" "$RIR_OUT"
grep -Fq "kind=ZoneHandle" "$RIR_OUT"
grep -Fq "kind=RelationInstance" "$RIR_OUT"
grep -Fq "kind=EffectInstance" "$RIR_OUT"
grep -Fq "name=dispatchPacket slot=dispatchPacket" "$RIR_OUT"
grep -Fq "kind=ProjectionTObject state=Published" "$RIR_OUT"
grep -Fq "Move                 subject=loading" "$RIR_OUT"
grep -Fq "Claim                subject=delivery" "$RIR_OUT"
grep -Fq "flow-block[" "$RIR_OUT"
grep -Fq "join=yes" "$RIR_OUT"
grep -Fq "semantics=authority|world-handoff|invalidation|authority-loss" "$RIR_OUT"

grep -Fq "cleanup-block=yes rollback-block=yes invalidation-block=yes" "$MIR_OUT"
grep -Fq "value[00] score.1 slot=score" "$MIR_OUT"
grep -Fq "name=score result=score.4" "$MIR_OUT"
grep -Fq "slot=loading name=Move" "$MIR_OUT"
grep -Fq "slot=stage name=CompensateIntentStep" "$MIR_OUT"
grep -Fq "cleanup-edge" "$MIR_OUT"
grep -Fq "DetachInvalidation" "$MIR_OUT"

for backend in $BACKENDS; do
    OUT_BIN="$WORK_DIR/logistics_${backend}"
    RUN_OUT="$WORK_DIR/runtime_${backend}.txt"
    "$PGY" "$EXAMPLE" --run --backend="$backend" -o "$OUT_BIN" > "$RUN_OUT" 2>&1
    grep -Fq "[Intent] RouteCargo ok=true" "$RUN_OUT"
    grep -Fq "[Runtime] steps=2" "$RUN_OUT"
    grep -Fq "[transfer] courier: LoadingZone.courier -> DeliveryZone.courier" "$RUN_OUT"
done

echo "ir-pipeline-probe: PASS $EXAMPLE"
