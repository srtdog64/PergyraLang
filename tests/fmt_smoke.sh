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

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_fmt_smoke.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

SOURCE="$WORK_DIR/fmt_case.pgy"
cat > "$SOURCE" <<'EOF'
func Add(a:Int,b:Int)->Int{return a+b;}
func Main()->Void{if(true){Log(Add(1,2));}}
EOF

if "$PGY" fmt "$SOURCE" --check >/dev/null 2>"$WORK_DIR/check_before.err"; then
    echo "[fmt-smoke] expected --check to fail before formatting" >&2
    exit 1
fi
grep -Fq "needs formatting" "$WORK_DIR/check_before.err"

"$PGY" fmt "$SOURCE" --write >"$WORK_DIR/write1.out"
grep -Fq "formatted" "$WORK_DIR/write1.out"
grep -Fq "func Add(a: Int, b: Int) -> Int" "$SOURCE"
grep -Fxq "{" "$SOURCE"
grep -Fq "return a + b;" "$SOURCE"
grep -Fq "func Main() -> Void" "$SOURCE"
grep -Fq "if (true)" "$SOURCE"
grep -Fq "Log(Add(1, 2));" "$SOURCE"

"$PGY" fmt "$SOURCE" --check

"$PGY" fmt "$SOURCE" --write >"$WORK_DIR/write2.out"
grep -Fq "already formatted" "$WORK_DIR/write2.out"

"$PGY" "$SOURCE" --backend=c -o "$WORK_DIR/fmt_case.out" >/dev/null 2>"$WORK_DIR/compile.err"

echo "fmt-smoke: PASS"
