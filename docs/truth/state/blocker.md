# Current blocker

Owner: Claude root lane. Checked 2026-11-10 23:30 KST at main `e875e9e90`.

## Objective card

- Objective: required CI green on one published commit, with the bootstrap
  and installed-driver jobs executed.
- Priority: semantic identity and one fact owner; fail-closed admission;
  C/LLVM parity; fixed point; installed execution; exact-SHA CI.
- Forbidden: a larger job timeout, a reduced canonical input, a skipped
  backend, a weakened negative, per-program-shape route admission.

## Open, in dependency order

1. Fixed point (`self-host-bootstrap-linux`).
   - `gen2_emit` refused at `collection_ownership_transition` because the
     scalar C route claimed programs it cannot emit. The claim is back to the
     validated condition in `658a9fcc9`; the full fixed point has not been
     rerun on it yet.
   - Time: the native compile of the canonical driver takes about 32 minutes
     on CI inside a 60-minute job. The landing fixed point runs about 65
     whole-program analyses, and one analysis costs 13 s with no landed copy
     and 40 to 100 s with 865 to 2308. Keeping copies across store growth
     reaches the same plan (checked on `mir_lower`) but saves only 31 to 25
     analyses. Next step: profile one analysis under a full copy plan.
2. `build-linux` contracts shard: 8 of 11 steps fail with the corrected
   driver, and the 2026-11-07 staged driver fails the same ones. Known
   causes: `task_lifecycle_invalid` on `let task = spawn ...; await task` in
   the self-host checker; `routine MIR fact index is incomplete: Classify
   [collection_ownership_row]`; field `let` writes in the identity-cell probe;
   `effect_array_transform_valid` refused natively; `RcGet` has no retention
   row. The core shard has not been rerun since the claim fix.
3. Backend compare: 7 native refusals remain of 23
   (`subject_param_constructor_store`, `reflect_type_name`,
   `generic_shadowed_argument_binding`, `option_of_result_consumers`,
   `lambda_reduce`, `function_pointer_reduce`,
   `generic_default_ability_bind_dispatch`).
4. Environment: after a memory exhaustion on 2026-11-10 the WSL distro
   restarts about two minutes into a heavy job. A `wsl --shutdown` by the
   user is needed before the long local runs can be repeated.

## Falsifiers kept

- A carrier actually replaced by a borrowed aggregate is still refused
  (`aggregate_member_replaced_carrier_borrowed_source_invalid`,
  `aggregate_result_replaced_root_borrowed_source_invalid`).
- Text finished with a local non-result allocator still cannot leave its
  routine; only compiler builtins stopped producing lane loans.
- An owned push or a drop on a collection fact with unestablished elements is
  still an orphan receipt; only a shallow mutation is admitted.
