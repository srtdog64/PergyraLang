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

run_backend() {
    local backend="$1"
    local out_bin="$WORK_DIR/out_$backend"
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

    echo "[stdlib-smoke] backend=$backend ok"
}

BACKENDS="${PGY_STDLIB_BACKENDS:-c llvm}"

for backend in $BACKENDS; do
    run_backend "$backend"
done
