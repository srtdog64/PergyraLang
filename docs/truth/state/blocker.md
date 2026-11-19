# Current blocker

Owner: Claude lane (GPT root lane's work since 2026-11-19); published
revision/CI: [main](main.md).
The v11 full MIR stops at transition ordinal 1438, the same first refusal as
published v8; no C output ([receipt](../evidence/2026-11-18_inout_member_restore_transition.md)).
A scratch diagnostic traversed all 9,623 route instances: 109 distinct first
refusals, no C output, and no claim about masked failures
([audit](../../audits/selfhost_full_mir_independent_refusal_census_2026-11-18.md)).
Next: publish and validate the sealed owned-result callee proof in MIR before
admitting a returned call-result row across branches. The semantic plan is
retired before MIR publication; changing its `retired` class alone would trust
forgeable input. The reached owner/consumer map and falsifiers are in the
[active plan](../../agent_work_directives/selfhost_owned_value_lifetime_execution_2026-11-18.md).
The general emitter leaks the deleted collection route's `ToString` results
and returned `Array<Int>` ([run record](../evidence/2026-11-19_collection_program_route_deleted.md));
nonempty record-field retirement leaks 32 bytes per backend (S2). The
one-process full DRV-2 bound fails with OOM, exit 134 and no C artifact
([measurements](measurements.md)). Default LLVM runs 7 programs unlike native:
six `function_param_flow_summary/*_landed_valid` (wrong values, an abort,
garbage String bytes) and `log_trailing_newline` ([sweep](../evidence/2026-11-19_scalar_cfg_mutable_declaration.md)).
Default C, through the MIR-to-AST round trip, prints garbage for a foreach
whose body pushes to the walked local ([record](../evidence/2026-11-19_general_route_collection_iteration.md)).

## Objective card

- Objective: neither front end admits freed-memory access on C or LLVM;
  required CI green on one published commit (user goal 2026-11-14).
- Priority: memory safety, one fact owner, fail-closed admission, backend
  parity, fixed point, exact-SHA CI.
- Forbidden: larger budgets, reduced input, skipped backend, weakened
  negative or per-program-shape route admission.

## Retained obligations (not parallel implementation tracks)

1. Required CI on the commit carrying this file ([gates](gates.md)).
2. Memory: four legs, LLVM IR at -O0, 3143 inputs: only endless recursion
   ([evidence](../evidence/2026-11-16_llvm_legs_at_o0.md)). The `--mir-json`
   route admits MIR that lacks a match temporary or loop binding row (the
   direct backend refuses it); a fix is on the DM3 branch, unverified.
3. One hand-off rule ([decision](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)):
   S0-S4 landed ([S3](../evidence/2026-11-17_s3_formal_alias_closure.md),
   [S4](../evidence/2026-11-17_s4_native_array_formal_class.md)). S5a (one
   later-use definition, Unproved moves) is an unlanded branch. Before S6
   deletes the backend sharing guard, `alias_read_after` must move or be
   refused: with the guard off it double-frees. S5c (`seed`, owned-element
   Clone) needs compiler Clones carried in direct MIR: 174 chosen, 0 in
   MIR statement text. `semantic.hashmap_collection_ownership` stays ACTIVE.
4. Compiler temporaries released at last use
   ([decision](../decisions/2026-11-16_compiler_temporaries_are_released_at_last_use.md)):
   DRV-2 source-to-C still fails the bound ([measurements](measurements.md)).
5. Front ends differ: native admits a `let` field write through a world,
   zone slot and subject path; `x = values` from a ref formal; generic
   `ref value: T`; reserved zone/role/effect names. F2 branch, unlanded.
   A `List` passed to a `ref` formal is refused even for `ListSize`. Native
   admits a `ref` formal stored in a local constructor; self-host refuses.

## Falsifiers kept

- `direct_mir_array_string_cleanup_fact_owner.sh`: refused 18+4+6+2, admitted 8x2 plus retained/nested cases; call-handoff row reader: 44.
