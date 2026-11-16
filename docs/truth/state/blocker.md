# Current blocker

Owner: Claude root lane. Checked 2026-11-16 on main `a2b78e746`.

## Objective card

- Objective: neither front end admits a program that reads or frees freed
  memory on either backend, with required CI green on one published commit
  (user goal 2026-11-14: all memory errors, SoT rows they pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the commit carrying this file (green at `a2b78e746`).
2. Memory: four legs, LLVM IR at -O0, 3143 inputs: only endless recursion
   ([evidence](../evidence/2026-11-16_llvm_legs_at_o0.md)), and one native
   LLVM report not confirmed under the mismatched ASan runtime.
3. One hand-off rule for both front ends
   ([decision](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)):
   S0 attributed all 69 disagreeing rows
   ([evidence](../evidence/2026-11-16_handoff_disagreement_attribution.md));
   S1 (the rule `.def`) is next. `semantic.hashmap_collection_ownership`
   stays ACTIVE until the backend sharing guard is deleted (S6).
4. Compiler temporaries released at last use
   ([decision](../decisions/2026-11-16_compiler_temporaries_are_released_at_last_use.md)):
   the self-host-emitted driver frees about a quarter of what it allocates,
   so the one-process DRV-2 source-to-C compile needs about 23 GB. Slice 0
   counts every direct MIR refusal of the compiler's own entry.
5. Weekly pipeline: the integration head reuses the same-run compiler pair
   (Track J); `machine_layer_runtime_projection_owner.pgy` assigns
   `r.value` to a union Result in its await bridge, so formal-semantics
   stays red; the driver shard's next stop is `valid_array_builtins`
   (direct MIR `stage=builtin-call`).
6. Self-host front end still differs from native: `x = values`
   reassignment from a ref formal, generic `ref value: T`, reserved words
   in zone/role/effect names, and no generic own/ref rule in the graph
   checker (later refusals catch the falsifiers).

## Done 2026-11-15 and 2026-11-16

- `226613c3c`, `41ab40e3f`, `667e45554`, `98bcf67ec`: native refusals,
  demand-order audit, self-host copies, hand-off fact, ownership crashes.
- `f1456722d`, `23989a49a`: native hand-off fact; by-value member and
  shared record release double frees closed.
- `a2b78e746`: generic moves; Bool arithmetic refused; native C
  `Echo(a)` then `ArrayPush(a)` use after free closed.
- LLVM-leg sweeps at -O0 (`-O1` hid double frees).

## Falsifiers kept

- `direct_mir_array_string_cleanup_fact_owner.sh`: refusals 14+4, record
  and loop takes refused, glibc heap checks.
- `native_unsafe_negative_refusal_smoke.sh`: 39 refusals, 17 admissions,
  8 copy landings.
- `shared_storage_copy_landing_owner.sh`: 2 landings, 12 refusals.
- `reserved_name_ref_alias_refusal_owner.sh` (Track F).
