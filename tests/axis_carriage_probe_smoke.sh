#!/usr/bin/env bash
#
# axis_carriage_probe_smoke.sh — empirical kernels for docs/151 Decision-0
# (axis carriage: positional / value-typed / runtime-tag), measured 2026-07-04.
# Minimal-combination kernel matrix; every verdict below is a MEASURED fact
# about today's compiler, locked so it cannot drift silently:
#
#   Effect/Auth x positional  : caps declared>=used is STATIC and catches the
#                               interprocedural laundering hop (reused
#                               tests/capability fixtures — the cap>=effect
#                               edge of docs/151 §4 in the flesh).
#   Auth x value-typed        : nominal per-type token — legit leg runs;
#                               field-identical ForgedToken is a STATIC reject.
#   Auth x runtime-tag        : tag field + Result gate — catches at RUNTIME
#                               through a wrapper hop, identical C/LLVM voice
#                               (the GATE verdict of docs/151 §3, live).
#   Zone x positional         : CLOSED 2026-07-05 (AC-3 S1, docs/157) — a
#                               live subject binding into a zone constructor
#                               is now a STATIC reject demanding Clone(...);
#                               the declared form (probe_clone_zone in
#                               tests/cases/axis_composition) preserves the
#                               old copy-isolation behavior exactly.
#   World x positional        : embedding a named zone binding is a STATIC
#                               reject ("implicitly copies zone binding",
#                               Clone demanded) — no-silent-override enforced.
#   Zone/World x value-typed  : per-type subject boundary — STATIC ctor
#                               field-type reject.
#
# Registered finding (docs/151 실측 부록): the zone level allowed the implicit
# copy the world level statically forbids — a cross-level inconsistency in
# no-silent-override discipline. CLOSED 2026-07-05 exactly as this header
# demanded: the zone-level diagnostic landed (AC-3 S1) and this leg was
# updated in the same commit. Measurement lock did its job.
#
# This is NOT the docs/151 §7 matrix-lock gate (that one locks the DECISION
# table after Decision-0/GATE close); this locks today's measured behavior.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[axis-carriage] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/axis_carriage_probe"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[axis-carriage] FAIL: $*" >&2; exit 1; }

compile() {
    local backend="$1" fixture="$2" out_name="$3"
    local src out rc
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$fixture")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$out_name")"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$out_name.log" 2>&1
    rc=$?
    set -e
    return $rc
}

expect_reject() {
    local backend="$1" fixture="$2" needle="$3"
    local tag="rej_${backend}_$(echo "$fixture" | tr '/.' '__').exe"
    if compile "$backend" "$fixture" "$tag"; then
        fail "$backend/$fixture compiled but the measured verdict is a static reject"
    fi
    grep -Fq "$needle" "$OUT_DIR/$tag.log" ||
        fail "$backend/$fixture rejected without the measured diagnostic: $needle"
}

expect_runs() {
    local backend="$1" fixture="$2" want="$3"
    local tag="run_${backend}_$(echo "$fixture" | tr '/.' '__').exe"
    compile "$backend" "$fixture" "$tag" ||
        fail "$backend/$fixture must compile: $(tail -2 "$OUT_DIR/$tag.log")"
    local got
    got="$("$OUT_DIR/$tag" | tr -d '\r')" || fail "$backend/$fixture crashed at runtime"
    [ "$got" = "$want" ] || fail "$backend/$fixture printed '$got', expected '$want'"
}

for backend in c llvm; do
    # Auth x value-typed: nominal token accepts holder, rejects the forgery.
    expect_runs   "$backend" auth_val/main.pgy "7"
    expect_reject "$backend" auth_val/forged.pgy "to accept 'ForgedToken'"

    # Auth x runtime-tag: three legs, one program, one voice.
    expect_runs "$backend" auth_tag/main.pgy $'42\ndirect-denied\nlaundered-denied'

    # Zone x positional: the silent copy is CLOSED — live subject bindings
    # into zone constructors fail closed, demanding the declared Clone form.
    expect_reject "$backend" zone_pos_share/main.pgy "implicitly copies subject binding"

    # World x positional: implicit copy of a named zone binding fails closed.
    expect_reject "$backend" world_pos_share/main.pgy "implicitly copies zone binding"

    # Zone/World x value-typed: per-type subject boundary is static.
    expect_reject "$backend" boundary_val/mismatch.pgy "got 'CartBuyer'"
done

# Effect/Auth x positional: reuse the capability fixtures (semantic layer,
# backend-independent) — legit clean, direct + interprocedural laundering fire.
cap() {
    set +e
    (cd "$ROOT_DIR" && "$PGY" --capability-manifest "tests/capability/$1") \
        >"$OUT_DIR/cap_$1.log" 2>&1
    local rc=$?
    set -e
    return $rc
}
cap manifest_declared_ok.pgy || fail "caps declared_ok must stay clean"
if cap manifest_violation.pgy; then fail "caps direct violation must fire"; fi
grep -Fq "missing declared capabilities" "$OUT_DIR/cap_manifest_violation.pgy.log" ||
    fail "caps violation lost its diagnostic"
if cap manifest_interproc.pgy; then fail "caps interprocedural laundering must fire"; fi

# ---- 중(medium) scale: one refund domain, three carriage arms, 3-hop ----
# Same domain logic and identical happy-path output per arm; what differs is
# WHERE the violation is caught (hop-1 static / analysis-time static at
# depth 3 / hop-3 runtime) and what each arm costs in the signatures.
for backend in c llvm; do
    expect_runs   "$backend" medium_pos/main.pgy "30"
    expect_reject "$backend" medium_pos/vio_deep.pgy "missing declared capabilities"
    expect_runs   "$backend" medium_val/main.pgy "30"
    expect_reject "$backend" medium_val/forged_deep.pgy "to accept 'ForgedToken'"
    expect_runs   "$backend" medium_tag/main.pgy $'30\ndeep-denied'
done

# Cost census over the three medium arms (informative, not asserted —
# these numbers feed docs/151 §2.2's cost table).
arm_loc() { grep -cv '^\s*//' "$FIXTURES/$1/main.pgy"; }
code_count() { grep -v '^\s*//' "$FIXTURES/$1/main.pgy" | grep -c "$2" || true; }
pos_caps="$(code_count medium_pos "with caps")"
val_thread="$(code_count medium_val "t: AdminToken")"
tag_plumb="$(code_count medium_tag "Result<Int>")"
echo "[axis-carriage] medium cost: pos loc=$(arm_loc medium_pos) caps-decl=$pos_caps | val loc=$(arm_loc medium_val) token-threading=$val_thread/3 sigs | tag loc=$(arm_loc medium_tag) result-plumbing=$tag_plumb/3 sigs"

# ---- 대(corpus) scale: what the largest real Pergyra program (self-host)
# already reaches for, by natural selection. Print-only census + existence
# floor (the corpus keeps growing; exact counts belong in the doc snapshot).
SH="$ROOT_DIR/src/self_hosted"
n_caps="$(grep -rE "with caps" "$SH" --include='*.pgy' | wc -l | tr -d ' ')"
n_authority="$(grep -rE "^\s*(authority |authorized by:)" "$SH" --include='*.pgy' | wc -l | tr -d ' ')"
n_errgate="$(grep -rE "return Err\(" "$SH" --include='*.pgy' | wc -l | tr -d ' ')"
echo "[axis-carriage] corpus census (self_hosted): positional caps=$n_caps, authority/step=$n_authority, runtime Result-gates=$n_errgate, value-typed token threading=0 (observed none)"
[ "$n_caps" -ge 1 ] || fail "corpus census: expected at least one with-caps site"
[ "$n_authority" -ge 1 ] || fail "corpus census: expected at least one authority site"
[ "$n_errgate" -ge 1 ] || fail "corpus census: expected at least one Err gate"

echo "[axis-carriage] measured verdicts locked (c/llvm): cap=STATIC+interproc(depth3), world=STATIC no-silent-copy, zone=STATIC no-silent-copy (closed 2026-07-05), per-type=STATIC(hop-1), tag=RUNTIME(hop-3)"
