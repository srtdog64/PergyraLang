#!/usr/bin/env bash
#
# axis_composition_smoke.sh — A-15 pairwise axis-composition safety
# (docs/156).
#
#   MEASUREMENT LOCK: the composition kernels' verdicts, as measured
#      2026-07-04 (C==LLVM identical voice):
#
#      (World x Intent)  control : same-scope transfer runs ("2").
#      (World x Intent)  cross   : FINDING CLOSED (2026-07-05, full
#                                  theorem T — docs/157 §5, BDFL twice
#                                  re-ratified option A). The measured
#                                  silent write-through ("2\n1\n2": dest
#                                  world interior mutated without a
#                                  Channel) is now a STATIC reject: a
#                                  world-owned zone binding cannot escape
#                                  as a live value into any call argument
#                                  or let-init; Clone(w.zone) is the
#                                  declared detached copy. The handoff
#                                  fixtures and flagship examples were
#                                  converted in the landing commit; the
#                                  handoff feature's live-mutation form is
#                                  registered as a redesign track on the
#                                  board (BDFL accepted the tension).
#      (Actor x Intent)  who-swap: who_a (who: buyer) and who_b
#                                  (who: observer) print IDENTICAL output —
#                                  `who` is descriptive participation, not
#                                  approval and not behavior. The reject
#                                  direction was also measured: without the
#                                  zone authority fact, BOTH legs reject
#                                  with "expected authority participant(s):
#                                  buyer" — the expectation is who-blind.
#
# The fixtures live in tests/cases/axis_composition/.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() { echo "[axis-composition] FAIL: $*" >&2; exit 1; }

# ---- measurement lock: run the kernels ----
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[axis-composition] kernel legs SKIP (pgy binary not found at $PGY)" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/axis_composition"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

run_kernel() {
    local backend="$1" fixture="$2"
    local tag="run_${backend}_$(echo "$fixture" | tr '/.' '__').exe"
    local src out
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$fixture")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$tag")"
    (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$tag.log" 2>&1 ||
        fail "$backend/$fixture must compile: $(tail -2 "$OUT_DIR/$tag.log")"
    "$OUT_DIR/$tag" | tr -d '\r' || fail "$backend/$fixture crashed at runtime"
}

for backend in c llvm; do
    got="$(run_kernel "$backend" comp_world_intent/control.pgy)"
    [ "$got" = "2" ] || fail "$backend control: got '$got', expected '2'"

    tag="rej_${backend}_cross.exe"
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/comp_world_intent/cross.pgy")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$tag")"
    if (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$tag.log" 2>&1; then
        fail "$backend cross-world transfer COMPILED — the write-through closure (docs/157 §5, full theorem T) regressed"
    fi
    grep -Fq "cannot escape as a live binding" "$OUT_DIR/$tag.log" ||
        fail "$backend cross-world transfer rejected without the boundary-fork diagnostic"

    tag="rej_${backend}_member.exe"
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/probe_world_member/main.pgy")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$tag")"
    if (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$tag.log" 2>&1; then
        fail "$backend let-init world-zone escape COMPILED — S2 regressed"
    fi
    grep -Fq "cannot escape as a live binding" "$OUT_DIR/$tag.log" ||
        fail "$backend let-init escape rejected without the boundary-fork diagnostic"

    got_a="$(run_kernel "$backend" comp_actor_intent/who_a.pgy)"
    got_b="$(run_kernel "$backend" comp_actor_intent/who_b.pgy)"
    [ "$got_a" = $'2\n100' ] || fail "$backend who_a: got '$got_a', expected '2 100'"
    [ "$got_a" = "$got_b" ] ||
        fail "$backend who-swap NON-INTERFERENCE BROKEN: who_a '$got_a' != who_b '$got_b' — the Actor axis is leaking into the Intent step"
done

echo "[axis-composition] kernels green (c/llvm): who-swap non-interference, boundary-fork escapes fail closed (write-through finding CLOSED 2026-07-05)"
