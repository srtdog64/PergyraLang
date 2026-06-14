#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_require_runnable_binary_here "stdlib-smoke" "$PGY"

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_stdlib_smoke.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
IO_ROOT="$WORK_DIR/io_root"
mkdir -p "$IO_ROOT"

FREEZE_DOC="$ROOT_DIR/docs/108_stdlib_beta_freeze.md"
if [[ ! -f "$FREEZE_DOC" ]]; then
    echo "[stdlib-smoke] missing stdlib freeze doc: $FREEZE_DOC" >&2
    exit 1
fi
for required in \
    "Stdlib Beta Freeze" \
    "beta-freeze-source-of-truth" \
    "Stable Builtin Stdlib Surface" \
    "\`FileOpen\`, \`FileRead\`, \`FileWrite\`, \`FileClose\`, \`ReadFile\`" \
    "\`WriteFile\`, \`FileExists\`" \
    "\`Args\`" \
    "\`SliceCopy(Slice<T>) -> Array<T>\`" \
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
    let view: Slice<Int> = values.Slice(1, 2);
    let owned: Array<Int> = SliceCopy(view);
    Log(ArrayLength(owned));
    Log(owned[0]);
    Log(owned[1]);

    let words: Array<String> = ["red", "blue", "green"];
    let wordView: Slice<String> = words.Slice(0, 2);
    let wordOwned: Array<String> = SliceCopy(wordView);
    Log(ArrayLength(wordOwned));
    Log(wordOwned[0]);
    Log(wordOwned[1]);

    let s: String = Concat(Upper(Trim("  hi")), Lower(" THERE"));
    Log(StringLength(s));
    Log(Contains(s, "HI"));
    Log(StringIndexOf(s, "THERE"));

    WriteFile("stable_io.txt", Replace(s, "HI", "BYE"));
    Log(FileExists("stable_io.txt"));
    Log(FileExists("missing_stable_io.txt"));
    let out: String = ReadFile("stable_io.txt");
    Log(out);
    let writer: Int = FileOpen("stable_handle.txt", "w");
    FileWrite(writer, "handle");
    FileClose(writer);
    let reader: Int = FileOpen("stable_handle.txt", "r");
    let line: String = FileRead(reader);
    FileClose(reader);
    Log(line);

    let c: Color = Red;
    if c == Red {
        Log(9);
    }

    let orderedLabels: HashMap<String, Int> = MapNew();
    MapSet(orderedLabels, "zeta", 1);
    MapSet(orderedLabels, "alpha", 2);
    MapSet(orderedLabels, "mid", 3);
    let orderedLabelKeys: Array<String> = MapKeys(orderedLabels);
    if orderedLabelKeys[0] == "alpha" && orderedLabelKeys[1] == "mid" && orderedLabelKeys[2] == "zeta" {
        Log(113);
    }

    let orderedNumbers: HashMap<Int, Int> = MapNew();
    MapSet(orderedNumbers, 9, 90);
    MapSet(orderedNumbers, 1, 10);
    MapSet(orderedNumbers, 4, 40);
    let orderedNumberKeys: Array<Int> = MapKeys(orderedNumbers);
    if orderedNumberKeys[0] == 1 && orderedNumberKeys[1] == 4 && orderedNumberKeys[2] == 9 {
        Log(127);
    }

    let orderedBools: HashMap<Bool, Int> = MapNew();
    MapSet(orderedBools, true, 1);
    MapSet(orderedBools, false, 0);
    let orderedBoolKeys: Array<Bool> = MapKeys(orderedBools);
    if orderedBoolKeys[0] == false && orderedBoolKeys[1] == true {
        Log(131);
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

cat > "$WORK_DIR/stable_exit.pgy" <<'EOF'
func Main() -> Void {
    Exit(7);
}
EOF

cat > "$WORK_DIR/stable_io_root.pgy" <<'EOF'
func Main() -> Void {
    let fd: Int = FileOpen("rooted.txt", "w");
    FileWrite(fd, "rooted");
    FileClose(fd);
    Log(FileExists("rooted.txt"));
    Log(ReadFile("rooted.txt"));
}
EOF

cat > "$WORK_DIR/stable_args.pgy" <<'EOF'
func Main() -> Void {
    let args: Array<String> = Args();
    Log(ArrayLength(args));
    if ArrayLength(args) == 3 {
        Log(args[0]);
        Log(args[1]);
        Log(args[2]);
    }
}
EOF

resolve_smoke_bin() {
    local path="$1"
    if [[ -x "$path" ]]; then
        printf '%s\n' "$path"
    elif [[ "$path" != *.exe && -x "${path}.exe" ]]; then
        printf '%s\n' "${path}.exe"
    else
        printf '%s\n' "$path"
    fi
}

run_backend() {
    local backend="$1"
    local output
    local stable_arg
    local modules_arg
    local io_root_arg
    local args_arg
    local args_bin
    local args_bin_arg

    stable_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/stable_stdlib.pgy")"
    modules_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/stable_use_modules.pgy")"
    io_root_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/stable_io_root.pgy")"
    args_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/stable_args.pgy")"
    args_bin="$WORK_DIR/stable_args_${backend}"
    args_bin_arg="$(pgy_path_for_compiler "$PGY" "$args_bin")"

    output="$(cd "$WORK_DIR" && "$PGY" "$stable_arg" --backend="$backend" --run 2>&1)"

    for expected in "42" "2" "3" "blue" "red" "8" "true" "false" "BYE there" "handle" "9" "113" "127" "131"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[stdlib-smoke] backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done

    output="$(cd "$ROOT_DIR" && "$PGY" "$modules_arg" --backend="$backend" --run 2>&1)"

    for expected in "2026-4-26" "true" "device/sensor"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[stdlib-smoke] use-modules backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done

    output="$(cd "$ROOT_DIR" && PGY_IO_ROOT="$IO_ROOT" "$PGY" "$io_root_arg" --backend="$backend" --run 2>&1)"
    for expected in "true" "rooted"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[stdlib-smoke] IO root backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done

    if ! output="$(cd "$WORK_DIR" && "$PGY" "$args_arg" --backend="$backend" -o "$args_bin_arg" 2>&1)"; then
        echo "[stdlib-smoke] Args compile backend=$backend failed" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--------------" >&2
        exit 1
    fi
    args_bin="$(resolve_smoke_bin "$args_bin")"
    output="$(cd "$WORK_DIR" && "$args_bin" alpha beta gamma 2>&1)"
    for expected in "3" "alpha" "beta" "gamma"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[stdlib-smoke] Args backend=$backend missing '$expected'" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done

    echo "[stdlib-smoke] backend=$backend ok"
}

run_exit_backend() {
    local backend="$1"
    local exit_arg
    local output
    local rc

    exit_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/stable_exit.pgy")"

    set +e
    output="$(cd "$WORK_DIR" && "$PGY" "$exit_arg" --backend="$backend" --run 2>&1)"
    rc=$?
    set -e

    if [[ "$rc" -ne 7 ]]; then
        echo "[stdlib-smoke] Exit(Int) backend=$backend expected rc=7, got rc=$rc" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--------------" >&2
        exit 1
    fi
}

BACKENDS="${PGY_STDLIB_BACKENDS:-c llvm}"

for backend in $BACKENDS; do
    run_backend "$backend"
    run_exit_backend "$backend"
done
