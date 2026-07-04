#!/usr/bin/env bash
#
# axis_composition_smoke.sh — A-15 pairwise axis-composition safety
# (docs/156). Two jobs, one gate:
#
#   1. MATRIX LOCK: docs/156's 15-pair disposition table stays complete —
#      every pair row, the four newly-registered edges of docs/151 §4,
#      and the registered findings cannot silently disappear.
#   2. MEASUREMENT LOCK: the composition kernels' verdicts, as measured
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

MATRIX_DOC="$ROOT_DIR/docs/156_axis_composition_safety.md"
EDGE_DOC="$ROOT_DIR/docs/151_generic_axis_composition.md"

fail() { echo "[axis-composition] FAIL: $*" >&2; exit 1; }

require_term() {
    local path="$1" term="$2"
    grep -Fq -- "$term" "$path" || fail "$(basename "$path") missing required term: $term"
}

# ---- 1a. matrix lock: all 15 pair rows present ----
[[ -f "$MATRIX_DOC" ]] || fail "docs/156_axis_composition_safety.md missing"
for pair in \
    "World×Zone" "World×Actor" "World×Authority" "World×Intent" "World×slot" \
    "Zone×Actor" "Zone×Authority" "Zone×Intent" "Zone×slot" \
    "Actor×Authority" "Actor×Intent" "Actor×slot" \
    "Authority×Intent" "Authority×slot" "Intent×slot"; do
    require_term "$MATRIX_DOC" "$pair"
done
require_term "$MATRIX_DOC" "SILENT WRITE-THROUGH"
require_term "$MATRIX_DOC" "who-swap"
require_term "$MATRIX_DOC" "AC-rung"

# ---- 1b. edge-register reconciliation: the four edges the A-15 audit
#          found measured-but-unregistered must stay registered in §4 ----
for edge in "cap ⊢ zone-cross" "cap ⊢ slot-op" "effect ⊸ comp-slots" "role ⊨ ability"; do
    require_term "$EDGE_DOC" "$edge"
done

# ---- 2. measurement lock: run the kernels ----
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[axis-composition] doc rows locked; kernel legs SKIP (pgy binary not found at $PGY)" >&2; exit 0; }

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

echo "[axis-composition] 15-pair matrix locked; kernels green (c/llvm): who-swap non-interference, boundary-fork escapes fail closed (write-through finding CLOSED 2026-07-05)"
