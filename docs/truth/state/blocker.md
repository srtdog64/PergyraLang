# Current blocker

Owner: Claude root lane. Checked 2026-11-15 on main `24fe553f4`.

## Objective card

- Objective: neither front end admits a program that reads or frees freed
  memory on either backend, with required CI green on one published commit
  (user goal 2026-11-14: all memory errors, SoT rows they pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Required CI on the commit carrying this file (main after `16ac154e1`).
2. Self-host memory errors from the four-leg sweep
   ([evidence](../evidence/2026-11-15_four_leg_wide_memory_sweep.md)):
   40 on the self-host C leg and 10 on the self-host LLVM leg, plus three
   `readonly_map_query_string_*_landed_valid` cases. Most are
   `*_landed_valid` cases, where native lands a `Clone` and the self-host
   does not. Native C/LLVM admit none except endless recursion.
3. The semantic `disposition` stays `live` after a constructor or a callee
   takes a local's storage. Direct MIR refuses that release instead, which
   costs 38 LLVM lifetime fixtures and the public LLVM lane of
   `ast_graph_atom_prebind_valid`. Restoring them needs a retirement or
   escape fact. Changing the semantic owner, the MIR state table, the
   direct MIR transition policy and the native parity gate
   (`collection_ownership_fact_carrier_owner.sh`) together is required.
4. Weekly pipeline: the Windows driver parity shard needs an installed
   composition root, and the Linux driver parity and `platform-full-linux`
   jobs stop on runner shutdown or the time limit.
5. The self-host admits `let first = values` for a `ref` formal and `fail`
   as a name; native refuses both.

## Done 2026-11-15

- LLVM `ToString(Bool)` returns the runtime's owned copy (`dab1fc0f7`).
- Direct MIR refuses releases of storage another holder has
  (`f6ab5f0ce`, `afbd9ab7f`).
- Native refusals restored or added (`226613c3c`): defer releases, extern
  callees, member-held String borrows, reassigned member, shared sort
  result.
- SoT: `selfhost.statement_result_type`, `selfhost.expression_place_kind`
  (`f5e171720`) and `semantic.function_param_flow_summary` (`16ac154e1`)
  closed; census CLOSED=70 BRIDGE=23 ACTIVE=2.
- Weekly pipeline fixes: macOS guard, InstCombine, three stale gates.

## Falsifiers kept

- `direct_mir_array_string_cleanup_fact_owner.sh`: 9 refusals, and reading
  callees and a read-only record run on both legs.
- `native_unsafe_negative_refusal_smoke.sh`: 39 refusals, 17 admissions,
  8 copy landings.
- `statement_type_place_contract_parity.sh`: the four contracts refuse.
