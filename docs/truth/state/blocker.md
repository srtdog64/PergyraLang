# Current blocker

Owner: GPT root integration. The user resumed the self-compile memory plan
on 2026-11-17 from main `5b0d354fb`; other paused tracks are not reopened.

A2 is published through `7ea3c781f`; CI run 44024724037 passed after
the [publication fixes](../evidence/2026-11-17_selfhost_memory_a2_ci_publication_fix.md).
A1 and the reached array OOM repair passed local integration; exact-candidate
CI and full DRV-2 memory remain open ([evidence](../evidence/2026-11-17_selfhost_memory_a1_flat_projection_and_oom.md)).
Then follow the direct-MIR/owned-value dependencies of the [memory plan](../../agent_work_directives/selfhost_compile_memory_plan_2026-11-17.md).

## Objective card

- Objective: neither front end admits a program that reads or frees freed
  memory on either backend, with required CI green on one published commit
  (user goal 2026-11-14: all memory errors, SoT rows they pass through).
- Priority: memory safety of what either front end admits; one fact owner;
  fail-closed admission; C/LLVM parity; fixed point; exact-SHA CI.
- Forbidden: a larger job or gate timeout, a reduced canonical input, a
  skipped backend, a weakened negative, per-program-shape route admission.

## Retained obligations (not parallel implementation tracks)

1. Required CI on the commit carrying this file ([gates](gates.md)).
2. Memory: four legs, LLVM IR at -O0, 3143 inputs: only endless recursion
   ([evidence](../evidence/2026-11-16_llvm_legs_at_o0.md)). The `--mir-json`
   route admits MIR that lacks a match temporary or loop binding row (the
   direct backend refuses it); a fix is on the DM3 branch, unverified. The
   three driver-shard "admitted negatives" were stale probes.
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
   DRV-2 source-to-C needs about 23 GB; direct MIR refuses routines at
   semantic 31, mir_lower 52, codegen 59; DRV-2 stops at `role-declaration`
   ([DM2](../evidence/2026-11-17_direct_mir_dm2_driver_and_fuzz_failure_set.md)).
5. Weekly pipeline (run 43914895554): W1 fixed the let/var fixtures, the
   DAG floor and the rule header; open are direct MIR (driver shard 91
   failures, fuzz variant 1), `dnd_tavern_campaign` (S5), and line caps
   (32 semantic TUs, 2 headers, 6 test includes, 39 owners).
6. Front ends differ: native admits a `let` field write through a world,
   zone slot and subject path; `x = values` from a ref formal; generic
   `ref value: T`; reserved zone/role/effect names. F2 branch, unlanded.
   A `List` passed to a `ref` formal is refused even for `ListSize`.

## Unlanded branches at the pause

`s5a-one-later-use`, `dm3-direct-mir`, `f2-front-end-rules`: wip, not
verified as a whole; read each branch's last commit before reuse.

## Falsifiers kept

- `direct_mir_array_string_cleanup_fact_owner.sh`: refused 18+4, admitted
  8x2, heap checks; `self-host-call-handoff-row-reader-test-smoke`: 44.
