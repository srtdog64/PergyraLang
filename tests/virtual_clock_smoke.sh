#!/usr/bin/env bash
#
# virtual_clock_smoke.sh — the docs/181 SS2.3 clock substrate:
#
#   - real mode: monotonic nonzero host clock, is_virtual == 0
#   - PGY_VIRTUAL_CLOCK=1: starts at 0, moves ONLY by
#     pgy_clock_advance_ns_export (5ms advance observed exactly),
#     is_virtual == 1
#   - advancing the REAL clock is a lifecycle violation: fail-closed
#     panic (class=invalid-lifecycle-state), never a silent no-op
#
# The probe links against the built runtime object so the export-side
# single-instance state (the g_pgy_budget/cap_granted rule) is the
# thing under test. Standalone gate: builds its own probe, needs only
# a C compiler and build/runtime/pgy_runtime_lib.o.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RUNTIME_OBJ="$ROOT_DIR/build/runtime/pgy_runtime_lib.o"
CC_BIN="${CC:-gcc}"

fail() { echo "[virtual-clock] FAIL: $*" >&2; exit 1; }

[[ -f "$RUNTIME_OBJ" ]] || {
    echo "[virtual-clock] SKIP: runtime object not built at $RUNTIME_OBJ" >&2
    exit 0
}

OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

cat > "$OUT_DIR/probe.c" <<'EOF'
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int64_t pgy_clock_now_ns_export(void);
extern void pgy_clock_advance_ns_export(int64_t ns);
extern int32_t pgy_clock_is_virtual_export(void);
int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "virtual") == 0) {
        if (pgy_clock_is_virtual_export() != 1) return 1;
        if (pgy_clock_now_ns_export() != 0) return 1;
        pgy_clock_advance_ns_export(5000000);
        if (pgy_clock_now_ns_export() != 5000000) return 1;
        printf("virtual clock ok\n");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "misuse") == 0) {
        pgy_clock_advance_ns_export(1); /* real mode: must panic */
        printf("SURVIVED\n");
        return 0;
    }
    {
        int64_t a = pgy_clock_now_ns_export();
        int64_t b = pgy_clock_now_ns_export();
        if (pgy_clock_is_virtual_export() != 0) return 1;
        if (a <= 0 || b < a) return 1;
        printf("real clock ok\n");
        return 0;
    }
}
EOF

"$CC_BIN" -std=c11 -O1 -o "$OUT_DIR/probe" "$OUT_DIR/probe.c" \
    "$RUNTIME_OBJ" -lpthread -lm ||
    fail "probe did not compile/link against the runtime object"

env -u PGY_VIRTUAL_CLOCK "$OUT_DIR/probe" >"$OUT_DIR/real.log" 2>&1 ||
    fail "real-mode probe failed: $(cat "$OUT_DIR/real.log")"
grep -q "real clock ok" "$OUT_DIR/real.log" ||
    fail "real-mode probe printed no witness"

PGY_VIRTUAL_CLOCK=1 "$OUT_DIR/probe" virtual >"$OUT_DIR/virt.log" 2>&1 ||
    fail "virtual-mode probe failed: $(cat "$OUT_DIR/virt.log")"
grep -q "virtual clock ok" "$OUT_DIR/virt.log" ||
    fail "virtual-mode probe printed no witness"

set +e
env -u PGY_VIRTUAL_CLOCK "$OUT_DIR/probe" misuse >"$OUT_DIR/misuse.log" 2>&1
rc=$?
set -e
[[ $rc -ne 0 ]] || fail "advancing the real clock exited clean but must panic"
grep -q "clock lifecycle violation" "$OUT_DIR/misuse.log" ||
    fail "real-clock advance died without the lifecycle diagnostic"
grep -q "SURVIVED" "$OUT_DIR/misuse.log" &&
    fail "real-clock advance continued past the violation"

echo "[virtual-clock] substrate ok: real monotonic + virtual advance-only + misuse fail-closed"
