# Current blocker

Owner: Claude root lane. Checked 2026-11-17 on main `a1cb26a93`.

## Objective card

- Objective: neither front end admits a program that reads or frees freed
  memory on either backend, with required CI green on one published commit
  (user goal 2026-11-14: all memory errors, SoT rows they pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the commit carrying this file ([gates](gates.md)).
2. Memory: four legs, LLVM IR at -O0, 3143 inputs: only endless recursion
   ([evidence](../evidence/2026-11-16_llvm_legs_at_o0.md)), and one native
   LLVM report not confirmed under the mismatched ASan runtime.
3. One hand-off rule for both front ends
   ([decision](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)):
   S0, S1 and S2 done; both front ends publish call hand-off rows in MIR
   JSON and the self-host reader refuses forged rows
   ([evidence](../evidence/2026-11-16_s2_call_handoff_rows.md)). S3, the
   self-host alias closure (`grown_handoff`), is next.
   `semantic.hashmap_collection_ownership` stays ACTIVE until S6.
4. Compiler temporaries released at last use
   ([decision](../decisions/2026-11-16_compiler_temporaries_are_released_at_last_use.md)):
   the one-process DRV-2 source-to-C compile needs about 23 GB. Direct MIR
   must admit the compiler's own entry first; DM1 cut refused routines to
   semantic 31, mir_lower 52, codegen 59
   ([evidence](../evidence/2026-11-16_direct_mir_dm1_coverage_families.md)).
   DRV-2 stops at route stage `role-declaration`: the role receiver ABI row
   and override identity live only in `direct_mir_role_override_*`.
5. Weekly pipeline run 43914895554 on `3ad7444db`, the first in weeks whose
   `platform-full-linux` reached its gates: let/var debt in weekly-only
   fixtures, the type-resolution DAG floor (1453 < 2000), bodies in
   `call_handoff_rule.h`, direct MIR `stage=builtin-call` (driver shard
   `valid_array_builtins`, fuzz generator), `dnd_tavern_campaign` (S5),
   and line caps: 32 semantic TUs over 599 and 2 headers over 600, deferred
   until S5-S7 rewrite those owners.
6. Self-host front end still differs from native: `x = values`
   reassignment from a ref formal, generic `ref value: T`, reserved words
   as zone/role/effect names, and no generic own/ref rule in the graph
   checker (later refusals catch the falsifiers).

## Done 2026-11-16 and 2026-11-17

- `a2b78e746`, `7cf0c6be1`: generic moves; self-host reserved names.
- `577f9b722`, `add4254c5`, `a1cb26a93`: hand-off rule and MIR rows; DM1.
- `f01cb5758`, `4a9c2c0a9`: macOS `timeout`; emitted-C feature macros.

## Falsifiers kept

- `direct_mir_array_string_cleanup_fact_owner.sh`: refusals 14+4, record
  and loop takes refused, glibc heap checks.
- `native_unsafe_negative_refusal_smoke.sh`: 39 refusals, 17 admissions.
- `self-host-call-handoff-row-reader-test-smoke`: 44 forged rows refused.
- `direct_mir_general_emitter_coverage_owner.sh`: 5 positives on 5 legs.
