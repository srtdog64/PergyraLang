# Current blocker

Owner: Claude root lane. Checked 2026-11-16 on main `8f03ec5b0`.

## Objective card

- Objective: neither front end admits a program that reads or frees freed
  memory on either backend, with required CI green on one published commit
  (user goal 2026-11-14: all memory errors, SoT rows they pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the commit carrying this file (green at `8f03ec5b0`).
2. Self-host memory errors ([evidence](../evidence/2026-11-15_four_leg_wide_memory_sweep.md)):
   3 of 50 crashes left after `667e45554` (endless recursion; growth only
   through a call result; two records releasing one member, sl). The C leg
   lost two `*_duplicate*_landed_valid` positives.
3. The semantic `disposition` stays `live` after a constructor or a callee
   takes a local's storage. Direct MIR refuses that release instead, which
   costs 38 LLVM lifetime fixtures and the public LLVM lane of
   `ast_graph_atom_prebind_valid`. Restoring them needs a retirement or
   escape fact; after a callee hand-off nobody releases, as in native
   ([decision](../decisions/2026-11-15_handed_off_array_storage_is_not_released.md)).
   Semantic owner, MIR state table, direct MIR transition policy and
   `collection_ownership_fact_carrier_owner.sh` change together.
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

## Falsifiers kept

- `direct_mir_array_string_cleanup_fact_owner.sh`: 9 refusals, and reading
  callees and a read-only record run on both legs.
- `native_unsafe_negative_refusal_smoke.sh`: 39 refusals, 17 admissions,
  8 copy landings.
- `statement_type_place_contract_parity.sh`: the four contracts refuse.
- `shared_storage_copy_landing_owner.sh`: 2 landings, 12 refusals.
