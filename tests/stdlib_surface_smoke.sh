#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_stdlib_smoke.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

FREEZE_DOC="$ROOT_DIR/docs/108_stdlib_beta_freeze.md"
if [[ ! -f "$FREEZE_DOC" ]]; then
    echo "[stdlib-smoke] missing stdlib freeze doc: $FREEZE_DOC" >&2
    exit 1
fi
for required in \
    "Stdlib Beta Freeze" \
    "beta-freeze-source-of-truth" \
    "Stable Builtin Stdlib Surface" \
    "Stable \`use\` Modules" \
    "Known But Experimental Modules" \
    "\`datetime\`" \
    "\`money\`" \
    "\`timer\`" \
    "\`versioning\`" \
    "\`ledger\`" \
    "\`obligation\`" \
    "\`device_adapter\`" \
    "\`http\`: transport adapter draft" \
    "\`storage\`: persistence adapter draft" \
    "\`page\`: UI/page adapter draft" \
    "\`spray\`: GPU/Spray design placeholder"; do
    if ! grep -Fq "$required" "$FREEZE_DOC"; then
        echo "[stdlib-smoke] freeze doc missing: $required" >&2
        exit 1
    fi
done

cat > "$WORK_DIR/stable_stdlib.pgy" <<'EOF'
enum Color { Red, Green }

func Main() -> Void {
    let counter: Slot<Int> = 41;
    counter = counter + 1;
    Log(counter);

    let values: Array<Int> = [1, 2, 3];
    Log(values[1]);
    Log(ArrayLength(values));

    let s: String = Concat(Upper(Trim("  hi")), Lower(" THERE"));
    Log(StringLength(s));
    Log(Contains(s, "HI"));

    WriteFile("stable_io.txt", Replace(s, "HI", "BYE"));
    let out: String = ReadFile("stable_io.txt");
    Log(out);

    let c: Color = Red;
    if c == Red {
        Log(9);
    }
}
EOF

cat > "$WORK_DIR/stable_use_modules.pgy" <<'EOF'
use datetime;
use money;
use timer;
use versioning;
use ledger;
use obligation;
use device_adapter;

func Main() -> Void {
    let date: LocalDate = LocalDate(2026, 4, 26);
    Log(FormatDate(date));

    let amount: Money = MoneyOf(1200, "USD");
    let stamp: VersionStamp = VersionInitial("acct");
    let key: IdempotencyKey = MakeIdempotencyKey("transfer", 7);
    let posting: LedgerPosting = BuildTransferPosting("cash", "sales", amount, stamp, key, "sale");
    Log(LedgerBalanced(posting));

    let timer: TimerSpec = TimerAfter("ship", 100, 50);
    Log(TimerExpired(timer, 151));

    let obligation: Obligation = OpenObligation("ship", "ops", 100, 25);
    let check: ObligationCheck = EvaluateObligation(obligation, 140);
    Log(check.violated);

    let register: DeviceRegister = Register("sensor", 3);
    let sample: DeviceSample = SampleDevice(register, 44, 200);
    Log(SampleEventTopic(sample));
}
EOF

run_backend() {
    local backend="$1"
    local output

    output="$(cd "$WORK_DIR" && "$PGY" "stable_stdlib.pgy" --backend="$backend" --run 2>&1)"

    for expected in "42" "2" "3" "8" "true" "BYE there" "9"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[stdlib-smoke] backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done

    output="$(cd "$ROOT_DIR" && "$PGY" "$WORK_DIR/stable_use_modules.pgy" --backend="$backend" --run 2>&1)"

    for expected in "2026-4-26" "true" "device/sensor"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[stdlib-smoke] use-modules backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done

    echo "[stdlib-smoke] backend=$backend ok"
}

BACKENDS="${PGY_STDLIB_BACKENDS:-c llvm}"

for backend in $BACKENDS; do
    run_backend "$backend"
done
