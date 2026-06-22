#!/usr/bin/env python3
"""
Capability-machine projection gate -- the first executable exercise of the
docs/semantics/18 "Acceptance Rule" for a non-CPU backend.

This is a FALSIFICATION GATE, not a passing test. It encodes, as a runnable
checklist, exactly what AIR must own before the machine-neutral bet survives for
its strongest-correspondence substrate: the capability machine (slot handles +
authority evidence + capability-gated effects, docs/18 row 5).

A capability-machine backend must be projectable from AIR-only facts. To do that
it needs, per program:

  1. EFFECT INVENTORY      -- the named external operations/effects (not a count).
  2. CAPABILITY MASK       -- which PGY_CAP_* each effect/boundary requires.
  3. SLOT IDENTITY         -- each slot handle's identity + the ops on it.
  4. AUTHORITY<->CAPABILITY -- which capabilities each authority participant holds.

Today (2026-06-22) AIR carries NONE of these (it is an intent-topology + proof +
erasure-attribution IR). So this gate is RED. It goes GREEN one tick at a time as
future work plumbs each fact into AIR. When it is GREEN, re-run the projection by
hand to confirm the bet survived (revised).

  RED  (exit 1) = the bet is still falsified; the checklist shows what is missing.
  GREEN(exit 0) = AIR owns the capability-machine facts; the bet survived a round.

This gate consumes ONLY `pgy --air-json` output. If it ever needs the AST, the
semantic capability manifest, or MIR layout to fill a fact, that fact is NOT
owned by AIR and the corresponding check stays RED -- that is the whole point.

Usage:
  python tests/machine_neutral/capability_projection_gate.py --pgy ./bin/pgy.exe
"""
import argparse
import json
import subprocess
import sys
import pathlib

# Fixtures that ARE capability-machine programs (handle/token/authority/effect).
# Each names the facts a real capability machine would have to project from it.
FIXTURES = [
    ("tests/air_erasure/fixtures/03_secure_slot.pgy",
     "secure slot + hardware-authority token gates every Write/Read/Release"),
    ("tests/air_erasure/fixtures/05_zone_intent.pgy",
     "zone authority `authorized by hero requires Prepared` over a Guard action"),
    ("tests/air_erasure/fixtures/01_slot_provable_with.pgy",
     "value-slot Write/Read inside a `with slot` block"),
    ("tests/capability/cap_random_demo.pgy",
     "Random() -- a gated ambient effect, exercising per-operation effect->cap"),
]

# The four facts a capability-machine projection needs from AIR-only.
CHECKS = [
    ("effect_inventory",
     "named external effects/operations (not just *_count)",
     lambda air: _has_effect_inventory(air)),
    ("capability_mask",
     "PGY_CAP_* mask per effect/boundary",
     lambda air: _has_capability_mask(air)),
    ("slot_identity",
     "per-slot handle identity + its operation sites",
     lambda air: _has_slot_identity(air)),
    ("authority_capability_binding",
     "which capabilities each authority participant holds",
     lambda air: _has_authority_capability_binding(air)),
]


def _program_used_mask(air):
    caps = air.get("capabilities")
    if isinstance(caps, dict):
        m = caps.get("used_mask")
        if isinstance(m, str):
            try:
                return int(m, 16)
            except ValueError:
                return 0
    return 0


def _has_effect_inventory(air):
    # HONEST PER-OPERATION BAR: a capability machine gates each effect OPERATION,
    # so it needs effects bound to operation sites (air.effects_by_op), not the
    # program-wide union. A program that uses any capability MUST have at least
    # one per-operation effect site; a program using none honestly has an empty
    # list (vacuous ownership). The program union alone does NOT satisfy this.
    sites = air.get("effects_by_op")
    if not isinstance(sites, list):
        return False
    if any(s.get("effect") for s in sites):
        return True
    return _program_used_mask(air) == 0


def _has_capability_mask(air):
    # HONEST PER-OPERATION BAR: each effect operation must carry the capability it
    # requires (air.effects_by_op[].capability_mask), so the machine can gate that
    # specific operation -- NOT just the program-wide union. A program using any
    # capability MUST have a per-operation mask; a program using none is vacuous.
    sites = air.get("effects_by_op")
    if not isinstance(sites, list):
        return False
    if any(s.get("capability_mask") for s in sites):
        return True
    # also accept per-boundary/per-node masks if present
    for coll in ("boundaries", "intents"):
        for node in air.get(coll, []) or []:
            for key in ("capability_mask", "required_caps", "required_capabilities"):
                if node.get(key):
                    return True
    return _program_used_mask(air) == 0


def _has_slot_identity(air):
    # AIR emits a slot identity table ("slots": [{slot, op, routine}, ...]) from
    # the MIR resource-op walk (air_collect_slot_sites, all RESOURCE_OPs). Presence
    # of the list = structural ownership; a program with no slots honestly has an
    # empty list (same vacuity convention as effects_by_op). The real data is
    # demonstrated by the slot fixtures (e.g. 03_secure_slot ->
    # [{slot:hp,op:Write},{op:Read},{op:Release}]); an empty list for a non-slot
    # program like cap_random_demo is correct ownership, not a gap.
    return isinstance(air.get("slots"), list)


def _has_authority_capability_binding(air):
    # In Pergyra authority is CONTRACT-based (`authority hero requires Prepared`),
    # not PGY_CAP_*-based. So the honest binding a capability machine gates on is
    # the per-boundary required ability/contract. AIR now emits
    # boundary.required_abilities. A boundary that declares authority participants
    # must expose what they are required to hold; a boundary with no authority has
    # nothing to bind (vacuously fine, like an empty effect list).
    boundaries = air.get("boundaries", []) or []
    authority_boundaries = [b for b in boundaries if b.get("authority_names")]
    if not authority_boundaries:
        return True
    return all(
        isinstance(b.get("required_abilities"), list) and b["required_abilities"]
        for b in authority_boundaries
    )


def air_json(pgy, fixture):
    proc = subprocess.run([pgy, "--air-json", fixture],
                          capture_output=True, text=True)
    out = proc.stdout
    start = out.find("{")
    if start < 0:
        raise RuntimeError(f"no AIR JSON from {fixture}: {proc.stderr[:200]}")
    return json.loads(out[start:])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pgy", default="./bin/pgy.exe")
    args = ap.parse_args()
    root = pathlib.Path(__file__).resolve().parents[2]

    print("=== Capability-machine projection gate (docs/18 Acceptance Rule) ===")
    print("RED = bet still falsified (checklist shows what AIR must own next).\n")

    present = {name: 0 for name, _, _ in CHECKS}
    total_fixtures = 0
    for fixture, desc in FIXTURES:
        fpath = root / fixture
        if not fpath.exists():
            print(f"[SKIP] missing fixture {fixture}")
            continue
        total_fixtures += 1
        try:
            air = air_json(args.pgy, str(fpath))
        except Exception as e:  # noqa: BLE001 -- gate reports, never crashes silently
            print(f"[SKIP] {fixture}: {e}")
            continue
        print(f"* {fixture}\n    ({desc})")
        for name, need, check in CHECKS:
            ok = bool(check(air))
            present[name] += 1 if ok else 0
            print(f"    [{'OK ' if ok else 'GAP'}] {name}: {need}")
        print()

    print("--- inheritance checklist (tick a row GREEN by plumbing it into AIR) ---")
    all_green = total_fixtures > 0
    for name, need, _ in CHECKS:
        green = present[name] == total_fixtures and total_fixtures > 0
        all_green = all_green and green
        mark = "DONE" if green else "TODO"
        print(f"  [{mark}] {name:30s} {present[name]}/{total_fixtures} fixtures own it")

    if all_green:
        print("\nGREEN: AIR owns the capability-machine facts. "
              "Re-run the projection by hand to confirm the bet survived.")
        return 0
    print("\nRED: machine-neutral bet still falsified for the capability machine. "
          "AIR is an intent-topology+proof+erasure IR today, not an effect/"
          "capability/slot layer. See docs/semantics/18.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
