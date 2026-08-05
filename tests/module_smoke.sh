#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   module visibility stopped being enforced.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_module_smoke.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

run_ok() {
    local name="$1"
    local main_file="$2"
    shift 2
    local output
    local main_arg

    main_arg="$(pgy_path_for_compiler "$PGY" "$main_file")"
    if ! output="$("$PGY" "$main_arg" --run --backend=c 2>&1)"; then
        echo "[module-smoke] $name compiler/run failed" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--------------" >&2
        exit 1
    fi
    for expected in "$@"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[module-smoke] $name failed" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[module-smoke] $name ok"
}

run_fail() {
    local name="$1"
    local expected="$2"
    local main_file="$3"
    local output
    local main_arg
    local out_arg

    main_arg="$(pgy_path_for_compiler "$PGY" "$main_file")"
    out_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/out")"
    if output="$("$PGY" "$main_arg" --backend=c -o "$out_arg" 2>&1)"; then
        echo "[module-smoke] $name unexpectedly succeeded" >&2
        echo "$output" >&2
        exit 1
    fi
    if ! grep -Fq "$expected" <<<"$output"; then
        echo "[module-smoke] $name missing expected error" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--------------" >&2
        exit 1
    fi
    echo "[module-smoke] $name ok"
}

mkdir -p "$WORK_DIR/explicit_export"
cat > "$WORK_DIR/explicit_export/math.pgy" <<'EOF'
namespace Math {
    func HiddenAdd(a: Int, b: Int) -> Int { return a + b; }
    export func Add(a: Int, b: Int) -> Int { return HiddenAdd(a, b); }
}
EOF
cat > "$WORK_DIR/explicit_export/main.pgy" <<'EOF'
import "math.pgy";
func Main() -> Void {
    Log(Math.Add(2, 5));
}
EOF
run_ok "explicit_export" "$WORK_DIR/explicit_export/main.pgy" "7"

mkdir -p "$WORK_DIR/private_hidden"
cat > "$WORK_DIR/private_hidden/math.pgy" <<'EOF'
namespace Math {
    func HiddenAdd(a: Int, b: Int) -> Int { return a + b; }
    export func Add(a: Int, b: Int) -> Int { return HiddenAdd(a, b); }
}
EOF
cat > "$WORK_DIR/private_hidden/main.pgy" <<'EOF'
import "math.pgy";
func Main() -> Void {
    Log(Math.HiddenAdd(2, 5));
}
EOF
run_fail "private_hidden" "Undefined function 'Math.HiddenAdd'" \
    "$WORK_DIR/private_hidden/main.pgy"

mkdir -p "$WORK_DIR/implicit_export"
cat > "$WORK_DIR/implicit_export/util.pgy" <<'EOF'
namespace Util {
    func Twice(n: Int) -> Int { return n + n; }
}
EOF
cat > "$WORK_DIR/implicit_export/main.pgy" <<'EOF'
import "util.pgy";
func Main() -> Void {
    Log(Util.Twice(4));
}
EOF
run_ok "implicit_export" "$WORK_DIR/implicit_export/main.pgy" "8"

mkdir -p "$WORK_DIR/ability_zone_same_module"
cat > "$WORK_DIR/ability_zone_same_module/mod.pgy" <<'EOF'
ability Analyst {
    func CanAnalyze(self) -> Bool;
}

zone AnalysisZone {
    shared n: Int = 7
}
EOF
cat > "$WORK_DIR/ability_zone_same_module/main.pgy" <<'EOF'
import "mod.pgy";
func Main() -> Void {
    let zone = AnalysisZone();
    Log(ToString(zone.n));
}
EOF
run_ok "ability_zone_same_module" "$WORK_DIR/ability_zone_same_module/main.pgy" "7"

mkdir -p "$WORK_DIR/circular"
cat > "$WORK_DIR/circular/a.pgy" <<'EOF'
import "b.pgy";
func A() -> Int { return 1; }
EOF
cat > "$WORK_DIR/circular/b.pgy" <<'EOF'
import "a.pgy";
func B() -> Int { return 2; }
EOF
cat > "$WORK_DIR/circular/main.pgy" <<'EOF'
import "a.pgy";
func Main() -> Void { Log(1); }
EOF
run_fail "circular_import" "circular import detected" \
    "$WORK_DIR/circular/main.pgy"

mkdir -p "$WORK_DIR/intent_failure_result"
cat > "$WORK_DIR/intent_failure_result/main.pgy" <<'EOF'
subject Driver {
    let hp: Int;
    let started: Bool;

    action Ignite(self) -> Void {
        started = true;
        hp = hp + 1;
    }

    action RollbackIgnite(self) -> Void {
        started = false;
        hp = hp - 1;
    }
}

zone CockpitZone {
    subject slot driver: Driver
}

intent DriveCar(cockpit: CockpitZone, driver: Driver) {
    step Ignite {
        where: CockpitZone;
        using: cockpit;
        who: driver;
        on: driver.Ignite();
        compensate: driver.RollbackIgnite();
        guard: false;
        post: driver.started;
    }

    success: true;
    failure: true;
}

func Main() -> Void {
    let d = Driver(0, false);
    let cockpit = CockpitZone(Driver(99, true));
    Log(DriveCar(cockpit, d));
    Log(IntentLastFailed());
}
EOF
run_ok "intent_failure_result" "$WORK_DIR/intent_failure_result/main.pgy" "false" "true"

mkdir -p "$WORK_DIR/nested_intent_orchestration"
cat > "$WORK_DIR/nested_intent_orchestration/main.pgy" <<'EOF'
subject Driver {
    let reserved: Int;
    let charged: Int;

    action Reserve(self) -> Void {
        reserved = reserved + 1;
    }

    action Charge(self) -> Void {
        charged = charged + 1;
    }
}

zone OrderZone {
    subject slot driver: Driver
}

zone PaymentZone {
    subject slot driver: Driver
}

intent Reserve(order: OrderZone, driver: Driver) {
    step reserve {
        where: OrderZone;
        using: order;
        who: driver;
        on: driver.Reserve();
        post: order.driver.reserved > 0;
    }

    success: order.driver.reserved > 0;
    failure: false;
}

intent Charge(payment: PaymentZone, driver: Driver) {
    step charge {
        where: PaymentZone;
        using: payment;
        who: driver;
        on: driver.Charge();
        post: payment.driver.charged > 0;
    }

    success: payment.driver.charged > 0;
    failure: false;
}

intent Fulfill(order: OrderZone, payment: PaymentZone,
               order_driver: Driver, payment_driver: Driver) {
    step reserve {
        intent: Reserve(order, order_driver);
        expect: order.driver.reserved > 0;
    }

    step charge {
        intent: Charge(payment, payment_driver);
        expect: payment.driver.charged > 0;
    }

    success: payment.driver.charged > 0;
    failure: false;
}

func Main() -> Void {
    let order = OrderZone(Driver(0, 0));
    let payment = PaymentZone(Driver(0, 0));
    Log(Fulfill(order, payment, order.driver, payment.driver));
    Log(order.driver.reserved);
    Log(payment.driver.charged);
}
EOF
run_ok "nested_intent_orchestration" "$WORK_DIR/nested_intent_orchestration/main.pgy" "true" "1" "1"

mkdir -p "$WORK_DIR/parallel_ref_slot_conflict"
cat > "$WORK_DIR/parallel_ref_slot_conflict/main.pgy" <<'EOF'
subject WorkerLedger {
    let reserved: Int;
}

func Reserve(ref ledger: Slot<WorkerLedger>, load: Int) -> Int {
    let cur: WorkerLedger = Read(ledger);
    let next: WorkerLedger = WorkerLedger(cur.reserved + load);
    Write(ledger, next);
    return next.reserved;
}

func Main() -> Void {
    let left: Slot<WorkerLedger> = WorkerLedger(0);
    parallel {
        Reserve(left, 3);
        Reserve(left, 5);
    }
}
EOF
run_fail "parallel_ref_slot_conflict" "Parallel context slot conflict on 'left'" \
    "$WORK_DIR/parallel_ref_slot_conflict/main.pgy"
