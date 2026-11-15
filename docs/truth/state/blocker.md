# Current blocker

Owner: Claude root lane. Checked 2026-11-16 on main `98bcf67ec`.

## Objective card

- Objective: neither front end admits a program that reads or frees freed
  memory on either backend, with required CI green on one published commit
  (user goal 2026-11-14: all memory errors, SoT rows they pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the commit carrying this file (green at `98bcf67ec`).
2. Memory errors: on four legs only endless recursion is left
   ([evidence](../evidence/2026-11-16_selfhost_handoff_fact.md)). The C leg
   lost two `*_duplicate*_landed_valid` positives.
3. Hand-off fact: the self-host issues `handed-off` at constructor and
   growing-callee edges since `98bcf67ec`
   ([decision](../decisions/2026-11-15_handed_off_array_storage_is_not_released.md)).
   Native issues none, so direct MIR keeps its by-value sharing refusal for
   `--mir-json` input; `semantic.hashmap_collection_ownership` stays ACTIVE.
   Of the lifetime fixtures, native LLVM admits 568 and self-host LLVM 167
   ([evidence](../evidence/2026-11-16_selfhost_handoff_fact.md)).
4. Weekly pipeline ([evidence](../evidence/2026-11-15_weekly_driver_shard_causes.md)):
   - Native admits `driver_rung2_main.pgy` since `41ab40e3f`
     ([evidence](../evidence/2026-11-16_native_flow_audit_demand_order.md));
     its LLVM lane is unconfirmed on CI's LLVM.
   - The self-host source-to-C compile of that entry passed 16.6 GB.
   - `platform-full-linux` loses its runner during the `test-all` compile.
5. The self-host admits `let first = values` for a `ref` formal and `fail`
   as a name; native refuses both.

## Done 2026-11-15

- LLVM `ToString(Bool)` returns the runtime's owned copy (`dab1fc0f7`).
- Direct MIR refuses releases another holder shares (`f6ab5f0ce`, `afbd9ab7f`).
- Native refusals restored or added (`226613c3c`).
- SoT: `selfhost.statement_result_type`, `selfhost.expression_place_kind`
  (`f5e171720`) and `semantic.function_param_flow_summary` (`16ac154e1`)
  closed; census CLOSED=70 BRIDGE=23 ACTIVE=2.
- Weekly pipeline fixes, with the installed driver for both driver shards.
- Self-host lands native's copies and refuses the rest (`667e45554`).
- Native flow audit no longer depends on demand order (`41ab40e3f`).
- Self-host hand-off fact; last two ownership crashes (`98bcf67ec`).

## Falsifiers kept

- `direct_mir_array_string_cleanup_fact_owner.sh`: 9 refusals, and reading
  callees and a read-only record run on both legs.
- `native_unsafe_negative_refusal_smoke.sh`: 39 refusals, 17 admissions,
  8 copy landings.
- `statement_type_place_contract_parity.sh`: the four contracts refuse.
- `shared_storage_copy_landing_owner.sh`: 2 landings, 12 refusals.
